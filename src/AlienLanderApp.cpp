/*
TODO list:
 - standardize on either Y or Z axis for heights
 - fix point of rotation, should move towards closer edge as you descend
 - Use touch for scaling/rotation/panning
 - Compute point in texture, extract height
 - Display height over ground
 - Detect landing/collision
*/
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"

#include "cinder/Camera.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Vbo.h"
#include "cinder/Utilities.h"
#include <boost/format.hpp>
#include "Resources.h"
#include "SegmentDisplay.h"
#include "Ship.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class AlienLanderApp : public App {
public:
    void setup();
    void buildMeshes();
    void resize();
    void mouseMove( MouseEvent event );
    void touchesMoved( TouchEvent event );
    void keyDown( KeyEvent event );
    void keyUp( KeyEvent event );
    void update();
    void draw();

    uint mPoints = 21;
    uint mLines = 40;

    Ship mShip;
    vector<SegmentDisplay> mDisplays;

    gl::BatchRef    mLineBatch;
    gl::BatchRef    mMaskBatch;

    gl::VboMeshRef	mMaskMesh;
    gl::VboMeshRef	mLineMesh;
    gl::TextureRef	mTexture;
    gl::GlslProgRef	mShader;
    CameraPersp     mCamera;
    mat4            mTextureMatrix;

    Color mBlack = Color::black();
    Color mBlue = Color8u(66, 161, 235);
    Color mDarkBlue = Color8u::hex(0x1A3E5A);
    Color mRed = Color8u(240,0,0);

};

void prepareSettings( App::Settings *settings )
{
    settings->setHighDensityDisplayEnabled();
    settings->setWindowSize(800, 800);
}

void AlienLanderApp::setup()
{
    try {
        mTexture = gl::Texture::create( loadImage( loadResource( RES_US_SQUARE ) ) );
        mTexture->bind( 0 );
    }
    catch( ... ) {
        console() << "unable to load the texture file!" << std::endl;
    }
    mTexture->setWrap(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);

    try {
        mShader = ci::gl::GlslProg::create(
            ci::app::loadResource( RES_VERT ),
            ci::app::loadResource( RES_FRAG )
        );
    }
    catch( gl::GlslProgCompileExc &exc ) {
        console() << "Shader compile error: " << std::endl;
        console() << exc.what();
    }
    catch( ... ) {
        console() << "Unable to load shader" << std::endl;
    }


    buildMeshes();

    mShip.setup();

    mDisplays.push_back( SegmentDisplay(10).position( vec2( 5 ) ).scale( 2 ) );
    mDisplays.push_back( SegmentDisplay(10).rightOf( mDisplays.back() ) );
    mDisplays.push_back( SegmentDisplay(35).below( mDisplays.front() ) );
    mDisplays.push_back( SegmentDisplay(35).below( mDisplays.back() ) );

    for ( auto display = mDisplays.begin(); display != mDisplays.end(); ++display ) {
        display->colors( ColorA( mBlue, 0.8 ), ColorA( mDarkBlue, 0.8 ) );
        display->setup();
    }

//    setFullScreen( true );
    setFrameRate(60);
    gl::enableVerticalSync(true);
}

void AlienLanderApp::buildMeshes()
{
    vector<vec3> lineCoords;
    vector<vec3> maskCoords;

    for( uint z = 0; z < mLines; ++z ) {
        for( uint x = 0; x < mPoints; ++x ) {
            vec3 vert = vec3( x / (float) mPoints, 1, z / (float) mLines );
            lineCoords.push_back( vert );

            maskCoords.push_back( vert );
            // To speed up the vertex shader it only does the texture lookup
            // for vertexes with y values greater than 0. This way we can build
            // a strip: 1 1 1  that will become: 2 9 3
            //          |\|\|                    |\|\|
            //          0 0 0                    0 0 0
            vert.y = 0.0;
            maskCoords.push_back( vert );
        }
    }
    mLineMesh = gl::VboMesh::create( lineCoords.size(), GL_LINE_STRIP, {
        gl::VboMesh::Layout().usage( GL_STATIC_DRAW ).attrib( geom::Attrib::POSITION, 3 ),
    });
    mLineMesh->bufferAttrib( geom::Attrib::POSITION, lineCoords );
    mLineBatch = gl::Batch::create( mLineMesh, mShader );

    mMaskMesh = gl::VboMesh::create( maskCoords.size(), GL_TRIANGLE_STRIP, {
        gl::VboMesh::Layout().usage( GL_STATIC_DRAW ).attrib( geom::Attrib::POSITION, 3 ),
    });
    mMaskMesh->bufferAttrib( geom::Attrib::POSITION, maskCoords );
    mMaskBatch = gl::Batch::create( mMaskMesh, mShader );
}

void AlienLanderApp::resize()
{
    uint height = getWindowHeight();
    uint width = getWindowWidth();
    uint margin = 20;
    mPoints = (width - (2 * margin)) / 10.0;
    mLines = (height - (2 * margin)) / 25.0;
    buildMeshes();
}

void AlienLanderApp::update()
{
    mShip.update();

    // TODO Need to figure our what to do with the scale... should probably
    // affect the distance to the points rather than being handled by moving
    // the camera...
    float scale = mShip.mPos.z;
    mTextureMatrix = glm::translate( vec3( 0.5, 0.5, 0 ) );
    mTextureMatrix = glm::rotate( mTextureMatrix, mShip.mPos.w, vec3( 0, 0, 1 ) );
    mTextureMatrix = glm::scale( mTextureMatrix, vec3( scale, scale, 0.25 ) );
    mTextureMatrix = glm::translate( mTextureMatrix, vec3( mShip.mPos.x, mShip.mPos.y, 0 ) );
    mTextureMatrix = glm::translate( mTextureMatrix, vec3( -0.5, -0.5, 0 ) );

    vec4 vel = mShip.mVel;
    vec4 acc = mShip.mAcc;
    float fps = getAverageFps();
    boost::format zeroToOne( "%+07.5f" );
    boost::format shortForm( "%+08.4f" );
    mDisplays[0].display( "ALT " + (zeroToOne % mShip.mPos.z).str() );
    mDisplays[1]
        .display( "FPS " + (shortForm % fps).str() )
        .colors( ColorA( fps < 50 ? mRed : mBlue, 0.8 ), ColorA( mDarkBlue, 0.8 ) );
    mDisplays[2].display( " X " + (shortForm % vel.x).str()
        + "  Y " + (shortForm % vel.y).str()
        + "  R " + (shortForm % vel.w).str() );
    mDisplays[3].display( "dX " + (shortForm % acc.x).str()
        + " dY " + (shortForm % acc.y).str()
        + " dR " + (shortForm % acc.w).str() );

    float z = math<float>::clamp(mShip.mPos.z, 0.0, 1.0);
    // TODO: Need to change the focus point to remain parallel as we descend
    mCamera.setPerspective( 40.0f, 1.0f, 0.5f, 3.0f );
    mCamera.lookAt( vec3( 0.0f, 1.5f * z, 1.0f ), vec3(0.0,0.1,0.0), vec3( 0, 1, 0 ) );
}

void AlienLanderApp::draw()
{

    gl::clear( mBlack, true );

    {
        gl::ScopedMatrices matrixScope;
        gl::setMatrices( mCamera );
        gl::translate(-0.5, 0.0, -0.5);

        gl::ScopedDepth depthScope(true);

        mShader->uniform( "textureMatrix", mTextureMatrix );

        uint indiciesInLine = mPoints;
        uint indiciesInMask = mPoints * 2;
        for (int i = mLines - 1; i >= 0; --i) { // front to back
//        for (int i = 0; i <= mLines; ++i) { // back to front
            gl::color( mBlack );
//            gl::color( Color::gray( i % 2 == 1 ? 0.5 : 0.25) );
            mMaskBatch->draw( i * indiciesInMask, indiciesInMask);

            gl::color( mBlue );
            mLineBatch->draw( i * indiciesInLine, indiciesInLine);
        }

        // FIXME: Direction of rotation seems backwards
//        // Vector pointing north
//        gl::color( mRed );
//        vec2 vec = vec2(cos(mShip.mPos.w), sin(mShip.mPos.w)) / vec2(40.0);
//        gl::drawVector(vec3(0.0,1/10.0,0.0), vec3(vec.x,1/10.0, vec.y), 1/20.0, 1/100.0);
    }

    for ( auto display = mDisplays.begin(); display != mDisplays.end(); ++display ) {
        display->draw();
    }
}


void AlienLanderApp::mouseMove( MouseEvent event )
{
//    int height = getWindowHeight();
//    int width = getWindowWidth();
//    mZoom = 1 - (math<float>::clamp(event.getY(), 0, height) / height);
//    mAngle = (math<float>::clamp(event.getX(), 0, width) / width);
//    2 * M_PI *
}

void AlienLanderApp::touchesMoved( TouchEvent event )
{
    return;
    vec2 mDelta1, mDelta2;

    // TODO treat the two deltas as forces acting on a rigid body.
    // Acceleration becomes translation
    // Torque becomes rotation
    // Compression/tension becomes zooming
    const vector<TouchEvent::Touch>&touches = event.getTouches();
    if (touches.size() == 2) {
        mDelta1 = touches[0].getPrevPos() - touches[0].getPos();
        mDelta2 = touches[1].getPrevPos() - touches[1].getPos();

        mShip.mPos.x += (mDelta1.x + mDelta2.x) / 768.0;
        mShip.mPos.y += (mDelta1.y + mDelta2.y) / 768.0;
    }
}

void AlienLanderApp::keyDown( KeyEvent event )
{
    switch( event.getCode() ) {
        case KeyEvent::KEY_ESCAPE:
            quit();
            break;
        default:
            mShip.keyDown(event);
            break;
    }
}

void AlienLanderApp::keyUp( KeyEvent event )
{
    mShip.keyUp(event);
}


CINDER_APP( AlienLanderApp, RendererGl( RendererGl::Options().msaa( 16 ) ), prepareSettings )
