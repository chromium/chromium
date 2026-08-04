// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.util.SizeF;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.test.annotation.UiThreadTest;

import com.google.android.material.slider.Slider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoControlAutoHideManager;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoControlCoordinator;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoControlView;
import org.chromium.chrome.browser.media.immersive_playback.components.ImmersiveVideoPlayerCoordinator;
import org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;

import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Tests for {@link ImmersiveVideoPlaybackCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoPlaybackCoordinatorTest {
    @Mock private ImmersiveVideoControlCoordinator.Delegate mVideoControlDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private XrSceneCoreSessionManager mXrSceneCoreSessionManager;
    @Mock private CompositorView mCompositorView;
    @Mock private XrMovableComponent mControlPanelMovableComponent;
    @Mock private XrSurfaceEntityView mSurfaceEntityView;
    @Mock private XrResizableComponent mResizableComponent;
    @Mock private XrMovableComponent mSurfaceMovableComponent;
    @Mock private XrCurvedSurfaceEntityHolder mSurfaceEntityHolder;
    @Mock private XrPanelEntityHolder mMainPanelEntity;
    @Mock private XrPanelEntityHolder mControlPanelHolder;
    @Mock private XrInteractableComponent mInteractableComponent;
    @Mock private XrEntityHolder mActivitySpaceEntity;

    private ImmersiveVideoPlaybackCoordinator mCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() throws Exception {
        XrModuleProviderImpl.initialize();
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_Light);

        when(mSurfaceEntityHolder.getMovableComponent()).thenReturn(mSurfaceMovableComponent);
        when(mSurfaceEntityHolder.getResizableComponent()).thenReturn(mResizableComponent);
        when(mSurfaceEntityHolder.getInteractableComponent()).thenReturn(mInteractableComponent);
        when(mSurfaceEntityHolder.getEntitySize()).thenReturn(new SizeF(1f, 1f));
        when(mControlPanelHolder.getMovableComponent()).thenReturn(mControlPanelMovableComponent);
        when(mControlPanelHolder.getEntitySize()).thenReturn(new SizeF(1f, 1f));
        when(mControlPanelHolder.getParent()).thenReturn(mActivitySpaceEntity);
        when(mXrSceneCoreSessionManager.getMainPanelEntity()).thenReturn(mMainPanelEntity);
        when(mXrSceneCoreSessionManager.getActivitySpaceEntity()).thenReturn(mActivitySpaceEntity);
        when(mXrSceneCoreSessionManager.createPanelEntity(any(), any()))
                .thenReturn(mControlPanelHolder);
        when(mCompositorView.getView()).thenReturn(mSurfaceEntityView);
        when(mSurfaceEntityView.getHolder()).thenReturn(mSurfaceEntityHolder);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new TestImmersiveVideoPlaybackCoordinator(
                                    mActivity,
                                    mWindowAndroid,
                                    mVideoControlDelegate,
                                    mXrSceneCoreSessionManager,
                                    mCompositorView);

                    mCoordinator.show();
                });

        ResettersForTesting.register(
                () -> {
                    if (mCoordinator != null) {
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    mCoordinator.dispose();
                                    ShadowLooper.idleMainLooper();
                                });
                        mCoordinator = null;
                    }
                });
    }

    /** Tests that updateMediaPosition correctly updates the seek bar. */
    @Test
    @UiThreadTest
    public void testUpdateMediaPosition() {
        int durationSeconds = 10;
        int positionSeconds = 5;

        mCoordinator.updateMediaPosition(
                /* durationMs= */ durationSeconds * 1000,
                /* positionMs= */ positionSeconds * 1000,
                /* playbackRate= */ 0.0f);

        ImmersiveVideoControlView panel =
                assumeNonNull(
                        mCoordinator.getControlCoordinatorForTesting().getControlPanelForTesting());
        Slider slider = panel.getSeekBarForTesting();
        assertEquals(durationSeconds * 1000, (int) slider.getValueTo());
        assertEquals(positionSeconds * 1000, (int) slider.getValue());
    }

    /** Tests that updatePlaybackState correctly updates the playback state in the panel. */
    @Test
    @UiThreadTest
    public void testUpdatePlaybackState() {
        ImmersiveVideoControlView panel =
                assumeNonNull(
                        mCoordinator.getControlCoordinatorForTesting().getControlPanelForTesting());

        mCoordinator.updatePlaybackState(true);
        assertTrue(panel.isPlayingForTesting());

        mCoordinator.updatePlaybackState(false);
        assertFalse(panel.isPlayingForTesting());
    }

    /** Tests that updatePlayerSize forwards the call to the surface holder. */
    @Test
    @UiThreadTest
    public void testUpdatePlayerSize() {
        int width = 1920;
        int height = 1080;

        mCoordinator.updatePlayerSize(width, height);
        verify(mSurfaceEntityHolder).setSurfacePixelDimensions(width, height);
    }

    /** Tests that onFormatSelected updates shape and stereo mode. */
    @Test
    @UiThreadTest
    public void testVideoLayoutUpdates() {
        clearInvocations(mSurfaceMovableComponent);
        clearInvocations(mControlPanelMovableComponent);
        clearInvocations(mSurfaceEntityHolder);

        // Switch to HEMISPHERE and SIDE_BY_SIDE.
        mCoordinator.onFormatSelected(
                ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.HEMISPHERE);
        ShadowLooper.idleMainLooper();

        verify(mSurfaceEntityHolder).setSurfaceShape(XrSurfaceEntityShape.HEMISPHERE);
        verify(mSurfaceEntityHolder).setSurfaceStereoMode(XrSurfaceEntityStereoMode.SIDE_BY_SIDE);

        verify(mSurfaceMovableComponent).setMovable(false, false);
        verify(mControlPanelMovableComponent).setMovable(true, false);

        clearInvocations(mSurfaceMovableComponent);
        clearInvocations(mControlPanelMovableComponent);
        clearInvocations(mSurfaceEntityHolder);

        // Switch to SPHERE and TOP_BOTTOM.
        mCoordinator.onFormatSelected(
                ImmersiveStereoMode.TOP_BOTTOM, ImmersiveProjectionType.SPHERE);
        ShadowLooper.idleMainLooper();

        verify(mSurfaceEntityHolder).setSurfaceShape(XrSurfaceEntityShape.SPHERE);
        verify(mSurfaceEntityHolder).setSurfaceStereoMode(XrSurfaceEntityStereoMode.TOP_BOTTOM);

        verify(mSurfaceMovableComponent).setMovable(false, false);
        verify(mControlPanelMovableComponent).setMovable(false, false);

        clearInvocations(mSurfaceMovableComponent);
        clearInvocations(mControlPanelMovableComponent);
        clearInvocations(mSurfaceEntityHolder);

        // Switch to QUAD and MONO (default state).
        mCoordinator.onFormatSelected(ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        ShadowLooper.idleMainLooper();

        verify(mSurfaceEntityHolder).setSurfaceShape(XrSurfaceEntityShape.QUAD);
        verify(mSurfaceEntityHolder).setSurfaceStereoMode(XrSurfaceEntityStereoMode.MONO);

        verify(mSurfaceMovableComponent).setMovable(true, false);
        verify(mControlPanelMovableComponent, never()).setMovable(anyBoolean(), anyBoolean());
    }

    /** Tests that the control panel is automatically hidden after a period of inactivity. */
    @Test
    @UiThreadTest
    public void testControlPanelAutoHide() {
        clearInvocations(mControlPanelHolder);

        // Warp time forward by 5 seconds (AUTO_HIDE_DELAY_MS)
        ShadowLooper.idleMainLooper(
                ImmersiveVideoControlAutoHideManager.AUTO_HIDE_DELAY_MS, TimeUnit.MILLISECONDS);

        // Verify control panel autohides
        verify(mControlPanelHolder).setEntityEnabled(false);
    }

    /** Tests that hovering/pointing at the panel prevents the autohide timer from firing. */
    @Test
    @UiThreadTest
    public void testControlPanelHoverPreventsAutoHide() {
        clearInvocations(mControlPanelHolder);

        var controlCoordinator = mCoordinator.getControlCoordinatorForTesting();
        var hoverListener =
                assumeNonNull(
                        controlCoordinator
                                .getControlPanelForTesting()
                                .getHoverListenerForTesting());

        // Simulate hover enter via listener
        hoverListener.onHoverChanged(true);

        // Warp time forward by AUTO_HIDE_DELAY_MS
        ShadowLooper.idleMainLooper(
                ImmersiveVideoControlAutoHideManager.AUTO_HIDE_DELAY_MS, TimeUnit.MILLISECONDS);

        // Verify control panel remains visible
        verify(mControlPanelHolder, never()).setEntityEnabled(false);
    }

    /** Tests that exiting hover correctly restarts the autohide timer. */
    @Test
    @UiThreadTest
    public void testControlPanelHoverExitRestartsAutoHide() {
        clearInvocations(mControlPanelHolder);

        var controlCoordinator = mCoordinator.getControlCoordinatorForTesting();
        var hoverListener =
                assumeNonNull(
                        controlCoordinator
                                .getControlPanelForTesting()
                                .getHoverListenerForTesting());

        // 1. Hover enter via listener
        hoverListener.onHoverChanged(true);
        ShadowLooper.idleMainLooper(
                ImmersiveVideoControlAutoHideManager.AUTO_HIDE_DELAY_MS / 2, TimeUnit.MILLISECONDS);

        // 2. Hover exit via listener
        hoverListener.onHoverChanged(false);

        // 3. Warp time forward by AUTO_HIDE_DELAY_MS - 100ms (should still be visible)
        ShadowLooper.idleMainLooper(
                ImmersiveVideoControlAutoHideManager.AUTO_HIDE_DELAY_MS - 100,
                TimeUnit.MILLISECONDS);
        verify(mControlPanelHolder, never()).setEntityEnabled(false);

        // 4. Warp time forward the final 100 milliseconds (should autohide)
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        verify(mControlPanelHolder).setEntityEnabled(false);
    }

    /** Tests that toggling the format selection panel updates format button selected state. */
    @Test
    @UiThreadTest
    public void testToggleFormatPanelUpdatesButtonSelectionState() {
        clearInvocations(mControlPanelHolder);
        ImmersiveVideoControlView panel =
                assumeNonNull(
                        mCoordinator.getControlCoordinatorForTesting().getControlPanelForTesting());

        mCoordinator.onFormatClicked();
        ShadowLooper.idleMainLooper(); // Flush binder updates

        verify(mControlPanelHolder).setEntityEnabled(true);
        assertTrue(panel.isFormatButtonSelectedForTesting());

        clearInvocations(mControlPanelHolder);

        // Toggle close format panel
        mCoordinator.onFormatClicked();
        ShadowLooper.idleMainLooper();

        verify(mControlPanelHolder).setEntityEnabled(false);
        assertFalse(panel.isFormatButtonSelectedForTesting());
    }

    /**
     * Tests that all components in the hierarchy are disposed strictly bottom-up (children before
     * parents).
     */
    @Test
    @UiThreadTest
    public void testDisposalHierarchyOrder() {
        XrPanelEntityHolder formatPanelHolder = mock(XrPanelEntityHolder.class);
        when(formatPanelHolder.getEntitySize()).thenReturn(new SizeF(1f, 1f));
        when(mXrSceneCoreSessionManager.createPanelEntity(any(), any()))
                .thenReturn(formatPanelHolder);

        mCoordinator.onFormatClicked();

        Consumer<XrPanelEntityHolder> markAsDisposed =
                holder ->
                        doThrow(new IllegalStateException("Entity is already disposed"))
                                .when(holder)
                                .setEntityEnabled(anyBoolean());

        // 1. Disposing Player surface entity disposes its children (Control Panel and Format
        // Panel).
        doAnswer(
                        invocation -> {
                            markAsDisposed.accept(mControlPanelHolder);
                            markAsDisposed.accept(formatPanelHolder);
                            return null;
                        })
                .when(mSurfaceEntityHolder)
                .dispose();

        // 2. Disposing Control Panel entity disposes its child (Format Panel).
        doAnswer(
                        invocation -> {
                            markAsDisposed.accept(formatPanelHolder);
                            return null;
                        })
                .when(mControlPanelHolder)
                .dispose();

        mCoordinator.dispose();
        verify(formatPanelHolder).dispose();
        verify(mControlPanelHolder).dispose();
        verify(mSurfaceEntityHolder).dispose();
    }

    /** Tests that recommended options are only populated when provided by native. */
    @Test
    @UiThreadTest
    public void testRecommendedOptionOnlyProvidedByNative() {
        var formatCoordinator = mCoordinator.getFormatCoordinatorForTesting();

        // 1. User selects a format via UI.
        mCoordinator.onFormatSelected(
                ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.HEMISPHERE);
        assertNull(formatCoordinator.getRecommendedStereoModeForTesting());
        assertNull(formatCoordinator.getRecommendedProjectionTypeForTesting());

        // 2. Native side sets immersive video options with isRecommended = false.
        mCoordinator.setImmersiveVideoOptions(
                ImmersiveStereoMode.TOP_BOTTOM, ImmersiveProjectionType.SPHERE, false);
        assertNull(formatCoordinator.getRecommendedStereoModeForTesting());
        assertNull(formatCoordinator.getRecommendedProjectionTypeForTesting());

        // 3. Native side sets immersive video options with isRecommended = true.
        mCoordinator.setImmersiveVideoOptions(
                ImmersiveStereoMode.TOP_BOTTOM, ImmersiveProjectionType.SPHERE, true);
        assertEquals(
                ImmersiveStereoMode.TOP_BOTTOM,
                (int) formatCoordinator.getRecommendedStereoModeForTesting());
        assertEquals(
                ImmersiveProjectionType.SPHERE,
                (int) formatCoordinator.getRecommendedProjectionTypeForTesting());
    }

    /** Tests that seekBar accessibility events are blocked during programmatic updates. */
    @Test
    @UiThreadTest
    public void testSeekBarAccessibilityEventsBlockedDuringPlayback() throws Exception {
        // 1. Create a visible activity and coordinator to ensure view attachment.
        Activity visibleActivity =
                Robolectric.buildActivity(org.chromium.ui.base.TestActivity.class)
                        .create()
                        .start()
                        .resume()
                        .visible()
                        .get();
        visibleActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_Light);

        ImmersiveVideoPlaybackCoordinator visibleCoordinator =
                new TestImmersiveVideoPlaybackCoordinator(
                        visibleActivity,
                        mWindowAndroid,
                        mVideoControlDelegate,
                        mXrSceneCoreSessionManager,
                        mCompositorView);
        visibleCoordinator.show();

        ImmersiveVideoControlView panel =
                assumeNonNull(
                        visibleCoordinator
                                .getControlCoordinatorForTesting()
                                .getControlPanelForTesting());
        Slider originalSlider = panel.getSeekBarForTesting();

        // 2. Enable accessibility in Robolectric.
        var shadowManager =
                (org.robolectric.shadows.ShadowAccessibilityManager)
                        org.robolectric.shadow.api.Shadow.extract(
                                visibleActivity.getSystemService("accessibility"));
        shadowManager.setEnabled(true);
        android.accessibilityservice.AccessibilityServiceInfo serviceInfo =
                new android.accessibilityservice.AccessibilityServiceInfo();
        serviceInfo.eventTypes = AccessibilityEvent.TYPES_ALL_MASK;
        shadowManager.setEnabledAccessibilityServiceList(List.of(serviceInfo));
        assertTrue(AccessibilityState.isAccessibilityManagerEnabled());

        // 3. Remove original slider from panel.
        ViewGroup originalParent = (ViewGroup) originalSlider.getParent();
        int index = originalParent.indexOfChild(originalSlider);
        originalParent.removeView(originalSlider);

        // 4. Create a TestSlider and replace the private field in panel.
        TestSlider testSlider = new TestSlider(visibleActivity);
        java.lang.reflect.Field seekBarField =
                ImmersiveVideoControlView.class.getDeclaredField("mSeekBar");
        seekBarField.setAccessible(true);
        seekBarField.set(panel, testSlider);

        // Add testSlider back to panel at the same index.
        originalParent.addView(testSlider, index);

        // 5. Add panel to a TestFrameLayout parent to monitor events.
        if (panel.getParent() != null) {
            ((ViewGroup) panel.getParent()).removeView(panel);
        }
        TestFrameLayout testParent = new TestFrameLayout(visibleActivity);
        testParent.addView(panel);
        visibleActivity.setContentView(testParent);

        // Allow Robolectric to process layout and attach events so accessibility is initialized.
        ShadowLooper.idleMainLooper();

        // Verify attachment
        assertTrue(testSlider.isAttachedToWindow());
        assertTrue(panel.isAttachedToWindow());

        // 6. Programmatic update during active playback (playing): setProgress and position label
        // updates should block accessibility events.
        panel.setPlaybackState(true);
        panel.setProgress(50);
        panel.getPositionLabelForTesting()
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        ShadowLooper.idleMainLooper(1000, java.util.concurrent.TimeUnit.MILLISECONDS);
        // Verify that parent did NOT receive any accessibility events.
        assertTrue(testParent.getReceivedEvents().isEmpty());

        // 6b. Programmatic update when paused: setProgress and position label updates should NOT
        // block accessibility events.
        panel.setPlaybackState(false);
        testParent.clearEvents();
        panel.setProgress(75);
        panel.getPositionLabelForTesting()
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        ShadowLooper.idleMainLooper(1000, java.util.concurrent.TimeUnit.MILLISECONDS);
        // Verify that parent DID receive the accessibility event because playback is paused.
        assertFalse(testParent.getReceivedEvents().isEmpty());

        // 7. Directly sending accessibility event (simulating user interaction or focus) should
        // NOT be blocked.
        testParent.clearEvents();
        testSlider.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SELECTED);
        ShadowLooper.idleMainLooper();
        // Verify that parent DID receive the accessibility event.
        assertEquals(1, testParent.getReceivedEvents().size());
        assertEquals(panel, testParent.getReceivedChild());
    }

    /** Test subclass that allows injecting mocked dependencies by overriding protected methods. */
    private static class TestImmersiveVideoPlaybackCoordinator
            extends ImmersiveVideoPlaybackCoordinator {
        private final CompositorView mMockCompositorView;

        public TestImmersiveVideoPlaybackCoordinator(
                Activity activity,
                WindowAndroid windowAndroid,
                ImmersiveVideoControlCoordinator.Delegate videoControlDelegate,
                XrSceneCoreSessionManager mockManager,
                CompositorView mockCompositorView) {
            super(activity, windowAndroid, videoControlDelegate, mockManager);
            mMockCompositorView = mockCompositorView;
        }

        @Override
        protected ImmersiveVideoPlayerCoordinator createPlayerCoordinator(
                Activity activity,
                WindowAndroid windowAndroid,
                XrSceneCoreSessionManager sessionManager) {
            return new ImmersiveVideoPlayerCoordinator(
                    activity, windowAndroid, sessionManager, this) {
                @Override
                protected CompositorView createCompositorView(
                        Activity activity,
                        WindowAndroid windowAndroid,
                        XrSceneCoreSessionManager sessionManager) {
                    return mMockCompositorView;
                }
            };
        }
    }

    private static class TestSlider extends Slider {
        public TestSlider(android.content.Context context) {
            super(context);
        }

        @Override
        public boolean isShown() {
            return true;
        }
    }

    private static class TestFrameLayout extends FrameLayout {
        private final List<AccessibilityEvent> mReceivedEvents = new java.util.ArrayList<>();
        private View mReceivedChild;

        public TestFrameLayout(android.content.Context context) {
            super(context);
        }

        @Override
        public boolean requestSendAccessibilityEvent(View child, AccessibilityEvent event) {
            mReceivedChild = child;
            mReceivedEvents.add(AccessibilityEvent.obtain(event));
            return super.requestSendAccessibilityEvent(child, event);
        }

        public List<AccessibilityEvent> getReceivedEvents() {
            return mReceivedEvents;
        }

        public View getReceivedChild() {
            return mReceivedChild;
        }

        public void clearEvents() {
            mReceivedEvents.clear();
            mReceivedChild = null;
        }
    }
}
