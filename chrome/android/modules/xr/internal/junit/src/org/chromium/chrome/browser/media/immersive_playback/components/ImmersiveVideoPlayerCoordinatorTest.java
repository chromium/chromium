// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;

/** Tests for {@link ImmersiveVideoPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoPlayerCoordinatorTest {
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private XrSceneCoreSessionManager mSessionManager;
    @Mock private CompositorView mCompositorView;
    @Mock private XrSurfaceEntityView mSurfaceEntityView;
    @Mock private XrSurfaceEntityHolder mHolder;
    @Mock private XrInteractableComponent mInteractableComponent;
    @Mock private ImmersiveVideoPlayerCoordinator.Delegate mDelegate;
    @Mock private XrResizableComponent mResizableComponent;
    @Mock private XrMovableComponent mMovableComponent;
    @Mock private XrPanelEntityHolder mMainPanelEntity;

    private Activity mActivity;
    private ImmersiveVideoPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();

        when(mCompositorView.getView()).thenReturn(mSurfaceEntityView);
        when(mSurfaceEntityView.getHolder()).thenReturn(mHolder);
        when(mHolder.getInteractableComponent()).thenReturn(mInteractableComponent);
        when(mHolder.getResizableComponent()).thenReturn(mResizableComponent);
        when(mHolder.getMovableComponent()).thenReturn(mMovableComponent);
        when(mSessionManager.getMainPanelEntity()).thenReturn(mMainPanelEntity);

        mCoordinator =
                new TestImmersiveVideoPlayerCoordinator(
                        mActivity, mWindowAndroid, mSessionManager, mDelegate, mCompositorView);
    }

    @Test
    public void testCreate() {
        assertNotNull(mCoordinator);
    }

    @Test
    public void testShow_InitializesAndEnablesHolder() {
        mCoordinator.show();

        verify(mHolder).setEntityEnabled(true);
        verify(mInteractableComponent).addOnClickListener(any());
        verify(mResizableComponent).addResizeListener(any());
        assertEquals(mCompositorView, mCoordinator.getCompositorView());
        assertEquals(mHolder, mCoordinator.getHolder());
    }

    @Test
    public void testDispose_DisposesHolder() {
        mCoordinator.show();
        mCoordinator.dispose();

        verify(mHolder).dispose();
    }

    @Test
    public void testSetInteractable() {
        mCoordinator.show();
        mCoordinator.setInteractable(false);

        verify(mInteractableComponent).setInteractable(false);
    }

    private static class TestImmersiveVideoPlayerCoordinator
            extends ImmersiveVideoPlayerCoordinator {
        private final CompositorView mMockCompositorView;

        public TestImmersiveVideoPlayerCoordinator(
                Activity activity,
                WindowAndroid windowAndroid,
                XrSceneCoreSessionManager sessionManager,
                Delegate delegate,
                CompositorView mockCompositorView) {
            super(activity, windowAndroid, sessionManager, delegate);
            mMockCompositorView = mockCompositorView;
        }

        @Override
        protected CompositorView createCompositorView(
                Activity activity,
                WindowAndroid windowAndroid,
                XrSceneCoreSessionManager sessionManager) {
            return mMockCompositorView;
        }
    }
}
