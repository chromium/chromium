// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSpace;
import org.chromium.ui.xr.scenecore.XrVector3;

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
        XrModuleProviderImpl.initialize();
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
        mCoordinator.show(mParentEntity);

        assertTrue(mCoordinator.isShowing());
        verify(mHolder).setParent(mParentEntity);
        verify(mHolder).setEntityEnabled(true);
    }

    @Test
    public void testDismiss_HidesAndDetaches() {
        mCoordinator.show(mParentEntity);
        mCoordinator.dismiss();

        assertFalse(mCoordinator.isShowing());

        mCoordinator.setParent(mParentEntity);
        mCoordinator.dismiss();
        verify(mHolder, times(1)).setParent(mParentEntity);
        verify(mHolder, times(1)).setParent(null);
        verify(mHolder, times(1)).setEntityEnabled(false);
    }

    @Test
    public void testUpdatePose() {
        mCoordinator.show(mParentEntity);
        XrPose expectedPose = XrPose.create(XrVector3.create(0f, -0.5f, 0f));
        mCoordinator.updatePose(expectedPose);

        verify(mHolder).setEntityPose(expectedPose, XrSpace.PARENT);
    }

    @Test
    public void testDispose_ReleasesBindingsAndListenersAndIsTerminal() {
        mCoordinator.show(mParentEntity);
        PropertyModel model = mCoordinator.getModelForTesting();
        mCoordinator.dispose();

        verify(mHolder).dispose();
        verify(mMovableComponent).removeMoveListener(any());
        verify(mControlView).setHoverListener(null);
        verify(mControlView).setAccessibilityFocusListener(null);

        clearInvocations(mSessionManager, mHolder, mMovableComponent, mControlView);
        model.set(ImmersiveVideoControlProperties.PROGRESS, 1234);
        model.set(
                ImmersiveVideoControlProperties.POSE,
                XrPose.create(XrVector3.create(1f, 2f, 3f)));
        mCoordinator.dispose();
        mCoordinator.show(mParentEntity);

        verify(mSessionManager, never()).createPanelEntity(any(), any());
        verify(mHolder, never()).dispose();
        verify(mHolder, never()).setEntityPose(any(), anyInt());
        verify(mControlView, never()).setProgress(anyInt());
    }

    @Test
    public void testRepeatedShowDoesNotDuplicateInitialization() {
        mCoordinator.show(mParentEntity);
        mCoordinator.show(mParentEntity);

        verify(mSessionManager, times(1)).createPanelEntity(any(), any());
        verify(mMovableComponent, times(1)).addMoveListener(any());
    }
}
