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
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl;
import org.chromium.chrome.browser.xr.scenecore.XrPixelDensityImpl;
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
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();

        when(mCompositorView.getView()).thenReturn(mSurfaceEntityView);
        when(mSurfaceEntityView.getHolder()).thenReturn(mHolder);
        when(mHolder.getInteractableComponent()).thenReturn(mInteractableComponent);
        when(mHolder.getResizableComponent()).thenReturn(mResizableComponent);
        when(mHolder.getMovableComponent()).thenReturn(mMovableComponent);
        when(mSessionManager.getMainPanelEntity()).thenReturn(mMainPanelEntity);
        when(mSessionManager.getPixelDensity())
                .thenReturn(XrPixelDensityImpl.createForTesting(1000f, 1000f));

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

    @Test
    public void testShow_SetsAccessibilityPropertiesAndDelegate() {
        mCoordinator.show();

        verify(mSurfaceEntityView).setFocusable(true);
        verify(mSurfaceEntityView).setFocusableInTouchMode(true);
        verify(mSurfaceEntityView)
                .setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        verify(mSurfaceEntityView)
                .setContentDescription(
                        mActivity.getString(
                                org.chromium.chrome.R.string.accessibility_video_player));
        verify(mSurfaceEntityView).setBackgroundColor(android.graphics.Color.TRANSPARENT);
        verify(mSurfaceEntityView).setAccessibilityDelegate(any());
        verify(mSurfaceEntityView).requestFocus();
    }

    @Test
    public void testAccessibilityClick_NotifiesDelegate() {
        mCoordinator.show();

        ArgumentCaptor<View.AccessibilityDelegate> delegateCaptor =
                ArgumentCaptor.forClass(View.AccessibilityDelegate.class);
        verify(mSurfaceEntityView).setAccessibilityDelegate(delegateCaptor.capture());
        View.AccessibilityDelegate accessibilityDelegate = delegateCaptor.getValue();
        assertNotNull(accessibilityDelegate);

        AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
        accessibilityDelegate.onInitializeAccessibilityNodeInfo(mSurfaceEntityView, nodeInfo);
        assertEquals(true, nodeInfo.isClickable());

        accessibilityDelegate.performAccessibilityAction(
                mSurfaceEntityView, AccessibilityNodeInfo.ACTION_CLICK, null);
        verify(mDelegate).onPlayerPanelClicked();

        nodeInfo.recycle();
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
