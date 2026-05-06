// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.Size;
import android.util.SizeF;
import android.view.Surface;
import android.widget.SeekBar;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrQuadSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;

/** Tests for {@link ImmersiveVideoPlaybackCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ImmersiveVideoPlaybackCoordinatorTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ImmersiveVideoControlDelegate mVideoControlDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private XrSceneCoreSessionManager mXrSceneCoreSessionManager;
    @Mock private CompositorView mCompositorView;
    @Mock private XrMovableComponent mControlPanelMovableComponent;
    @Mock private XrSurfaceEntityView mSurfaceEntityView;
    @Mock private XrResizableComponent mResizableComponent;
    @Mock private XrMovableComponent mSurfaceMovableComponent;
    private FakeSurfaceHolder mSurfaceEntityHolder;
    private FakePanelEntityHolder mMainPanelEntity;
    private FakePanelEntityHolder mControlPanelHolder;
    private ImmersiveVideoPlaybackCoordinator mCoordinator;
    private static Activity sActivity;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        mSurfaceEntityHolder = new FakeSurfaceHolder(mSurfaceMovableComponent, mResizableComponent);
        mMainPanelEntity = new FakePanelEntityHolder(null);
        mControlPanelHolder = new FakePanelEntityHolder(mControlPanelMovableComponent);

        when(mXrSceneCoreSessionManager.getMainPanelEntity()).thenReturn(mMainPanelEntity);
        when(mXrSceneCoreSessionManager.createPanelEntity(any(), any()))
                .thenReturn(mControlPanelHolder);
        when(mCompositorView.getView()).thenReturn(mSurfaceEntityView);
        when(mSurfaceEntityView.getHolder()).thenReturn(mSurfaceEntityHolder);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new TestImmersiveVideoPlaybackCoordinator(
                                    sActivity,
                                    mWindowAndroid,
                                    mVideoControlDelegate,
                                    mXrSceneCoreSessionManager,
                                    mCompositorView);

                    mCoordinator.createXrCompositorView(
                            XrSurfaceEntityStereoMode.MONO, XrSurfaceEntityShape.QUAD);
                });
    }

    /** Tests that updateMediaPosition correctly updates the seek bar. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdateMediaPosition() {
        // When playback is paused, updateMediaPosition should update the seek bar
        // immediately, otherwise it should update the seek bar asynchronously every
        // 500ms.

        int durationSeconds = 10;
        int positionSeconds = 5;

        mCoordinator.updateMediaPosition(
                /* durationMs= */ durationSeconds * 1000,
                /* positionMs= */ positionSeconds * 1000,
                /* playbackRate= */ 0.0f);

        ImmersiveVideoControlPanel panel = mCoordinator.getControlPanelForTesting();
        SeekBar seekBar = panel.getSeekBarForTesting();
        assertEquals(durationSeconds, seekBar.getMax());
        assertEquals(positionSeconds, seekBar.getProgress());
    }

    /** Tests that updatePlaybackState correctly updates the playback state in the panel. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdatePlaybackState() {
        ImmersiveVideoControlPanel panel = mCoordinator.getControlPanelForTesting();

        mCoordinator.updatePlaybackState(true);
        assertTrue(panel.isPlayingForTesting());

        mCoordinator.updatePlaybackState(false);
        assertFalse(panel.isPlayingForTesting());
    }

    /** Tests that updatePlayerSize forwards the call to the surface holder. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdatePlayerSize() {
        int width = 1920;
        int height = 1080;

        mCoordinator.updatePlayerSize(width, height);

        assertEquals(width, mSurfaceEntityHolder.lastWidth);
        assertEquals(height, mSurfaceEntityHolder.lastHeight);
    }

    /** Tests that updateVideoLayout updates shape and stereo mode. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdateVideoLayout() {
        clearInvocations(mSurfaceMovableComponent);
        clearInvocations(mControlPanelHolder.getMovableComponent());

        // Switch to HEMISPHERE and SIDE_BY_SIDE.
        mCoordinator.updateVideoLayout(
                XrSurfaceEntityStereoMode.SIDE_BY_SIDE, XrSurfaceEntityShape.HEMISPHERE);

        assertEquals(XrSurfaceEntityShape.HEMISPHERE, mSurfaceEntityHolder.lastShape);
        assertEquals(XrSurfaceEntityStereoMode.SIDE_BY_SIDE, mSurfaceEntityHolder.lastStereoMode);

        verify(mSurfaceMovableComponent).setMovable(false, false);
        verify(mControlPanelHolder.getMovableComponent()).setMovable(true, false);

        clearInvocations(mSurfaceMovableComponent);
        clearInvocations(mControlPanelHolder.getMovableComponent());

        // Switch to SPHERE and TOP_BOTTOM.
        mCoordinator.updateVideoLayout(
                XrSurfaceEntityStereoMode.TOP_BOTTOM, XrSurfaceEntityShape.SPHERE);

        assertEquals(XrSurfaceEntityShape.SPHERE, mSurfaceEntityHolder.lastShape);
        assertEquals(XrSurfaceEntityStereoMode.TOP_BOTTOM, mSurfaceEntityHolder.lastStereoMode);

        verify(mSurfaceMovableComponent).setMovable(false, false);
        verify(mControlPanelHolder.getMovableComponent()).setMovable(true, false);

        clearInvocations(mSurfaceMovableComponent);
        clearInvocations(mControlPanelHolder.getMovableComponent());

        // Switch to QUAD and MONO (default state).
        mCoordinator.updateVideoLayout(XrSurfaceEntityStereoMode.MONO, XrSurfaceEntityShape.QUAD);

        assertEquals(XrSurfaceEntityShape.QUAD, mSurfaceEntityHolder.lastShape);
        assertEquals(XrSurfaceEntityStereoMode.MONO, mSurfaceEntityHolder.lastStereoMode);

        verify(mSurfaceMovableComponent).setMovable(true, false);
        verify(mControlPanelHolder.getMovableComponent()).setMovable(false, false);
    }

    /** Test subclass that allows injecting mocked dependencies by overriding protected methods. */
    private static class TestImmersiveVideoPlaybackCoordinator
            extends ImmersiveVideoPlaybackCoordinator {
        private final XrSceneCoreSessionManager mMockManager;
        private final CompositorView mMockCompositorView;

        public TestImmersiveVideoPlaybackCoordinator(
                Activity activity,
                WindowAndroid windowAndroid,
                ImmersiveVideoControlDelegate videoControlDelegate,
                XrSceneCoreSessionManager mockManager,
                CompositorView mockCompositorView) {
            super(activity, windowAndroid, videoControlDelegate);
            mMockManager = mockManager;
            mMockCompositorView = mockCompositorView;
        }

        @Override
        protected XrSceneCoreSessionManager createXrSceneCoreSessionManager() {
            return mMockManager;
        }

        @Override
        protected CompositorView createCompositorView(int shape) {
            return mMockCompositorView;
        }
    }

    /**
     * Fake implementation of XrQuadSurfaceEntityHolder and XrCurvedSurfaceEntityHolder that wraps
     * an XrSurfaceEntityHolder and provides fake implementations for the additional methods
     * required by these interfaces.
     */
    private static class FakeSurfaceHolder
            implements XrQuadSurfaceEntityHolder, XrCurvedSurfaceEntityHolder {
        private final XrMovableComponent mMovableComponent;
        private final XrResizableComponent mResizableComponent;

        // State variables for verification
        public int lastShape = -1;
        public int lastStereoMode = -1;
        public int lastWidth = -1;
        public int lastHeight = -1;
        public float lastRadius = -1f;

        public FakeSurfaceHolder(
                XrMovableComponent movableComponent, XrResizableComponent resizableComponent) {
            mMovableComponent = movableComponent;
            mResizableComponent = resizableComponent;
        }

        @Override
        public void setEntityRadius(float radius) {
            lastRadius = radius;
        }

        @Override
        public float getEntityRadius() {
            return lastRadius;
        }

        @Override
        public XrMovableComponent getMovableComponent() {
            return mMovableComponent;
        }

        @Override
        public XrResizableComponent getResizableComponent() {
            return mResizableComponent;
        }

        @Override
        public SizeF getEntitySize() {
            return new SizeF(1f, 1f);
        }

        @Override
        public void setEntitySize(float width, float height) {}

        @Override
        public void addCallback(Callback callback) {}

        @Override
        public void removeCallback(Callback callback) {}

        @Override
        public Surface getSurface() {
            return null;
        }

        @Override
        public int getSurfaceStereoMode() {
            return lastStereoMode;
        }

        @Override
        public void setSurfaceStereoMode(int stereoMode) {
            lastStereoMode = stereoMode;
        }

        @Override
        public int getSurfaceShape() {
            return lastShape;
        }

        @Override
        public void setSurfaceShape(int shape) {
            lastShape = shape;
        }

        @Override
        public void setSurfacePixelDimensions(int width, int height) {
            lastWidth = width;
            lastHeight = height;
        }

        @Override
        public Object getEntity() {
            return null;
        }

        @Override
        public void setEntityPose(float[] translation) {}

        @Override
        public void setEntityPose(float[] translation, float[] rotation) {}

        @Override
        public float[] getEntityTranslation() {
            return null;
        }

        @Override
        public float[] getEntityRotation() {
            return null;
        }

        @Override
        public void setEntityScale(float scale) {}

        @Override
        public float getEntityScale() {
            return 1f;
        }

        @Override
        public void setEntityAlpha(float alpha) {}

        @Override
        public float getEntityAlpha() {
            return 1f;
        }

        @Override
        public void setEntityEnabled(boolean enabled) {}

        @Override
        public boolean isEntityEnabled() {
            return true;
        }

        @Override
        public void dispose() {}
    }

    private static class FakePanelEntityHolder implements XrPanelEntityHolder {
        private final XrMovableComponent mMovableComponent;
        public boolean isEnabled = true;

        public FakePanelEntityHolder(XrMovableComponent movableComponent) {
            mMovableComponent = movableComponent;
        }

        @Override
        public void setEntityEnabled(boolean enabled) {
            isEnabled = enabled;
        }

        @Override
        public boolean isEntityEnabled() {
            return isEnabled;
        }

        @Override
        public XrMovableComponent getMovableComponent() {
            return mMovableComponent;
        }

        @Override
        public void setEntitySizeInPixels(int w, int h) {}

        @Override
        public Size getEntitySizeInPixels() {
            return null;
        }

        @Override
        public void setEntityCornerRadius(float r) {}

        @Override
        public float getEntityCornerRadius() {
            return 0f;
        }

        @Override
        public Object getEntity() {
            return null;
        }

        @Override
        public void setEntityPose(float[] t) {}

        @Override
        public void setEntityPose(float[] t, float[] r) {}

        @Override
        public float[] getEntityTranslation() {
            return null;
        }

        @Override
        public float[] getEntityRotation() {
            return null;
        }

        @Override
        public void setEntityScale(float scale) {}

        @Override
        public float getEntityScale() {
            return 1f;
        }

        @Override
        public void setEntityAlpha(float alpha) {}

        @Override
        public float getEntityAlpha() {
            return 1f;
        }

        @Override
        public void dispose() {}

        @Override
        public XrResizableComponent getResizableComponent() {
            return null;
        }

        @Override
        public SizeF getEntitySize() {
            return null;
        }

        @Override
        public void setEntitySize(float w, float h) {}
    }
}
