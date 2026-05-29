// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSpace;

/** Tests for {@link ImmersiveVideoControlCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoControlCoordinatorTest {
    @Mock private XrSceneCoreSessionManager mSessionManager;
    @Mock private ImmersiveVideoControlCoordinator.Delegate mDelegate;
    @Mock private XrPanelEntityHolder<?> mHolder;
    @Mock private XrEntityHolder<?> mParentEntity;
    @Mock private ImmersiveVideoControlView mControlView;
    @Mock private XrMovableComponent mMovableComponent;

    private Activity mActivity;
    private ImmersiveVideoControlCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        when(mSessionManager.createPanelEntity(any(), any())).thenReturn(mHolder);
        when(mHolder.getMovableComponent()).thenReturn(mMovableComponent);

        mCoordinator =
                new TestImmersiveVideoControlCoordinator(
                        mActivity, mSessionManager, mDelegate, mControlView);
    }

    private static class TestImmersiveVideoControlCoordinator
            extends ImmersiveVideoControlCoordinator {
        private final ImmersiveVideoControlView mMockView;

        public TestImmersiveVideoControlCoordinator(
                Activity activity,
                XrSceneCoreSessionManager sessionManager,
                Delegate delegate,
                ImmersiveVideoControlView mockView) {
            super(activity, sessionManager, delegate);
            mMockView = mockView;
        }

        @Override
        ImmersiveVideoControlView createView(
                Activity activity, ImmersiveVideoControlView.UserInteractionListener listener) {
            return mMockView;
        }
    }

    @Test
    public void testCreate() {
        assertNotNull(mCoordinator);
        assertFalse(mCoordinator.isShowing());
    }

    @Test
    public void testShow_InitializesAndEnablesHolder() {
        doReturn(mParentEntity).when(mHolder).getParent();

        mCoordinator.show(mParentEntity);

        assertTrue(mCoordinator.isShowing());
        verify(mHolder).setParent(mParentEntity);
        verify(mHolder).setEntityEnabled(true);
    }

    @Test
    public void testDismiss_HidesAndDetaches() {
        mCoordinator.show(mParentEntity);
        doReturn(null).when(mHolder).getParent();
        mCoordinator.dismiss();

        assertFalse(mCoordinator.isShowing());
        verify(mHolder).setEntityEnabled(false);
        verify(mHolder).setParent(null);
    }

    @Test
    public void testUpdatePose() {
        mCoordinator.show(mParentEntity);
        float[] expectedTranslation = new float[] {0f, -0.5f, 0f};
        float[] expectedRotation = new float[] {0f, 0f, 0f, 1f};
        mCoordinator.updatePose(expectedTranslation, expectedRotation);

        verify(mHolder).setEntityPose(expectedTranslation, expectedRotation, XrSpace.PARENT);
    }

    @Test
    public void testDispose_DisposesHolder() {
        mCoordinator.show(mParentEntity);
        mCoordinator.dispose();

        verify(mHolder).dispose();
    }
}
