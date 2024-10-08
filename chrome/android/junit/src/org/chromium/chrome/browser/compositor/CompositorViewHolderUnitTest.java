// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.os.IBinder;
import android.view.ContextThemeWrapper;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.EventFilter.EventType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.content_capture.ContentCaptureFeatures;
import org.chromium.components.content_capture.ContentCaptureFeaturesJni;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.content_capture.OnscreenContentProviderJni;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link CompositorViewHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END})
@DisableFeatures({
    ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION,
    ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE
})
public class CompositorViewHolderUnitTest {
    // Since these tests don't depend on the heights being pixels, we can use these as dpi directly.
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int KEYBOARD_HEIGHT = 741;

    private static final long TOUCH_TIME = 0;
    private static final MotionEvent MOTION_EVENT_DOWN =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_DOWN, 1, 1, 0);
    private static final MotionEvent MOTION_EVENT_UP =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_UP, 1, 1, 0);

    private static final MotionEvent MOTION_ACTION_HOVER_ENTER =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_HOVER_ENTER, 1, 1, 0);

    private static final WindowInsetsCompat VISIBLE_SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 100, 0, 100))
                    .build();

    enum EventSource {
        IN_MOTION,
        TOUCH_EVENT_OBSERVER;
    }

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private ToolbarControlContainer mControlContainer;
    @Mock private View mContainerView;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private android.content.res.Resources mResources;
    @Mock private WebContents mWebContents;
    @Mock private ContentView mContentView;
    @Mock private CompositorView mCompositorView;
    @Mock private ResourceManager mResourceManager;
    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private KeyboardVisibilityDelegate mMockKeyboard;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Window mWindow;
    @Mock private View mDecorView;
    @Mock private DynamicResourceLoader mDynamicResourceLoader;
    @Mock private PrefService mPrefService;
    @Mock private OnscreenContentProvider.Natives mOnscreenContentProviderJni;
    @Mock private ContentCaptureFeatures.Natives mContentCaptureFeaturesJni;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private Context mContext;
    private MockTabModelSelector mTabModelSelector;
    private Tab mTab;
    private CompositorViewHolder mCompositorViewHolder;
    private BrowserControlsManager mBrowserControlsManager;
    private ApplicationViewportInsetSupplier mViewportInsets;
    private ObservableSupplierImpl<Integer> mKeyboardInsetSupplier;
    private ObservableSupplierImpl<Integer> mKeyboardAccessoryInsetSupplier;
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(OnscreenContentProviderJni.TEST_HOOKS, mOnscreenContentProviderJni);
        mJniMocker.mock(ContentCaptureFeaturesJni.TEST_HOOKS, mContentCaptureFeaturesJni);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        // Setup the mock keyboard.
        KeyboardVisibilityDelegate.setInstance(mMockKeyboard);

        mViewportInsets = ApplicationViewportInsetSupplier.createForTests();
        mKeyboardInsetSupplier = new ObservableSupplierImpl<>();
        mViewportInsets.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardAccessoryInsetSupplier = new ObservableSupplierImpl<>();
        mViewportInsets.setKeyboardAccessoryInsetSupplier(mKeyboardAccessoryInsetSupplier);

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        // Setup the TabModelSelector.
        mTabModelSelector =
                new MockTabModelSelector(
                        mProfile,
                        mIncognitoProfile,
                        0,
                        0,
                        (id, incognito) ->
                                spy(new MockTab(id, incognito ? mIncognitoProfile : mProfile)));
        mTab = mTabModelSelector.addMockTab();
        mTabModelSelector.getModel(false).setIndex(0, TabSelectionType.FROM_NEW);

        // Setup for BrowserControlsManager which initiates content/control offset changes
        // for CompositorViewHolder.
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(R.dimen.control_container_height))
                .thenReturn(TOOLBAR_HEIGHT);
        when(mControlContainer.getView()).thenReturn(mContainerView);
        when(mTab.isUserInteractable()).thenReturn(true);

        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(
                        mActivity, BrowserControlsStateProvider.ControlsPosition.TOP);
        mBrowserControlsManager = spy(browserControlsManager);
        mBrowserControlsManager.initialize(
                mControlContainer,
                mActivityTabProvider,
                mTabModelSelector,
                R.dimen.control_container_height);
        when(mBrowserControlsManager.getTab()).thenReturn(mTab);

        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        when(mCompositorView.getResourceManager()).thenReturn(mResourceManager);
        when(mResourceManager.getDynamicResourceLoader()).thenReturn(mDynamicResourceLoader);

        mCompositorViewHolder = spy(new CompositorViewHolder(mContext, null));

        mCompositorViewHolder.setLayoutManager(mLayoutManager);
        mCompositorViewHolder.setControlContainer(mControlContainer);
        mCompositorViewHolder.setCompositorViewForTesting(mCompositorView);
        mCompositorViewHolder.setBrowserControlsManager(mBrowserControlsManager);
        mCompositorViewHolder.setApplicationViewportInsetSupplier(mViewportInsets);
        mCompositorViewHolder.onFinishNativeInitialization(mTabModelSelector, null);
        when(mCompositorViewHolder.getCurrentTab()).thenReturn(mTab);
        when(mCompositorViewHolder.getRootWindowInsets())
                .thenReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getContentView()).thenReturn(mContentView);
        when(mTab.getView()).thenReturn(mContentView);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mDecorView.getFitsSystemWindows()).thenReturn(true);

        IBinder windowToken = mock(IBinder.class);
        when(mContainerView.getWindowToken()).thenReturn(windowToken);
        when(mContentView.getWindowToken()).thenReturn(windowToken);
    }

    private List<EventSource> observeTouchAndMotionEvents() {
        List<EventSource> eventSequence = new ArrayList<>();
        mCompositorViewHolder
                .getInMotionSupplier()
                .addObserver((inMotion) -> eventSequence.add(EventSource.IN_MOTION));
        // This touch observer is used as a proxy for when ViewGroup#dispatchTouchEvent is called,
        // which is when the touch is propagated to children.
        mCompositorViewHolder.addTouchEventObserver(
                new TouchEventObserver() {
                    @Override
                    public boolean onInterceptTouchEvent(MotionEvent e) {
                        return false;
                    }

                    @Override
                    public boolean dispatchTouchEvent(MotionEvent e) {
                        eventSequence.add(EventSource.TOUCH_EVENT_OBSERVER);
                        return false;
                    }
                });
        return eventSequence;
    }

    // controlsResizeView tests ---
    // For these tests, we will simulate the scrolls assuming we either completely show or hide (or
    // scroll until the min-height) the controls and don't leave at in-between positions. The reason
    // is that CompositorViewHolder only flips the mControlsResizeView bit if the controls are
    // idle, meaning they're at the min-height or fully shown. Making sure the controls snap to
    // these two positions is not CVH's responsibility as it's handled in native code by compositor
    // or blink.

    @Test
    public void testControlsResizeViewChanges() {
        // Let's use simpler numbers for this test.
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be fully visible.
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll to fully hidden.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -100,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertTrue(
                "Browser controls aren't at min-height.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        // ControlsResizeView is true, but it should be false when the controls are hidden.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertFalse(
                "Browser controls are hidden when they should be fully visible.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // #controlsResizeView should be flipped back to true.
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);
    }

    // Test that a page opted in to view transitions gets an early resize event
    // on the controls starting to show.
    @Test
    @DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EARLY_RESIZE)
    public void testResizeViewOnWillShowControlsWithViewTransition() {
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be hidden.
        assertTrue(
                "Browser controls aren't fully hidden.",
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        // Simulate the browser issuing a "show browser controls" signal to the renderer.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ false);

        // This should must not cause the controls to start resizing the view yet.
        verify(mCompositorView, never()).onControlsResizeViewChanged(any(), anyBoolean());
        reset(mCompositorView);

        // Do the same but this time with the page having the view transition opt in.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ true);

        // This should cause the controls to start resizing the view.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);
    }

    // Test for the browser controls early resize flagged behavior.
    // https://crbug.com/332331777
    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EARLY_RESIZE)
    public void testResizeViewOnWillShowControls() {
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be hidden.
        assertTrue(
                "Browser controls aren't fully hidden.",
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        // Simulate the browser issuing a "show browser controls" signal to the renderer.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ false);

        // This should cause the controls to start resizing the view.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Simulating a show-animation partially updating the controls, this shouldn't cause another
        // resize.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight / 2,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ topHeight / 2,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        verify(mCompositorView, never()).onControlsResizeViewChanged(any(), anyBoolean());
        reset(mCompositorView);

        // The controls finished animating in. Since they already resized the view, this should also
        // be a no-op.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ topHeight,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);

        verify(mCompositorView, never()).onControlsResizeViewChanged(any(), anyBoolean());
        reset(mCompositorView);

        // The controls going back to hidden should resize the view as usual.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);
    }

    // TODO(bokan): Ensure disabling the flag-guard reverts to old behavior.
    // https://crbug.com/332331777
    @Test
    @DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EARLY_RESIZE)
    public void testResizeViewOnWillShowControlsFlagGuarded() {
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be hidden.
        assertTrue(
                "Browser controls aren't fully hidden.",
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        // Simulate the browser issuing a "show browser controls" signal to the renderer.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ false);

        // This should must not cause the controls to start resizing the view yet.
        verify(mCompositorView, never()).onControlsResizeViewChanged(any(), anyBoolean());
        reset(mCompositorView);

        // The controls finished animating in. Since they already resized the view, this should
        // cause the resize the occur.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ topHeight,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);

        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION,
        ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE
    })
    public void testHandleSystemUiVisibilityChangesWithUpdatedFullscreenApis() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        mCompositorViewHolder.onNativeLibraryReady(mWindowAndroid, null, null);
        mCompositorViewHolder.handleSystemUiVisibilityChange();
    }

    @Test
    public void testControlsResizeViewChangesWithMinHeight() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);
        mBrowserControlsManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be fully visible.
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -75,
                /* bottomControlsOffsetY= */ 60,
                /* contentOffsetY= */ 25,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertTrue(
                "Browser controls aren't at min-height.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        // ControlsResizeView is true but it should be false when the controls are at min-height.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertFalse(
                "Browser controls are at min-height when they should be fully visible.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // #controlsResizeView should be flipped back to true.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);
    }

    @Test
    public void testControlsResizeViewWhenControlsAreNotIdle() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);
        mBrowserControlsManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is false but it should be true when the controls are fully visible.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll a little hide the controls partially.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -25,
                /* bottomControlsOffsetY= */ 20,
                /* contentOffsetY= */ 75,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is false, but it should still be true. No-op updates won't trigger a
        // changed event.
        verify(mCompositorView, times(0)).onControlsResizeViewChanged(any(), eq(true));
        reset(mCompositorView);

        // Scroll controls all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -75,
                /* bottomControlsOffsetY= */ 60,
                /* contentOffsetY= */ 25,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is true but it should've flipped to false since the controls are idle
        // now.
        verify(mCompositorView).onControlsResizeViewChanged(any(), eq(false));
        reset(mCompositorView);

        // Scroll controls to show a little more.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -50,
                /* bottomControlsOffsetY= */ 40,
                /* contentOffsetY= */ 50,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is true, but it should still be false. No-op updates won't trigger a
        // changed event.
        verify(mCompositorView, times(0)).onControlsResizeViewChanged(any(), eq(false));
    }

    // --- controlsResizeView tests

    // Keyboard resize tests for geometrychange event fired to JS.
    @Test
    public void testWebContentResizeTriggeredDueToKeyboardShow() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        // Viewport dimensions when keyboard is hidden.
        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;

        // adjustedHeight is the height of the CompositorViewHolder from Android View layout
        // after showing the keyboard. This simulates a reduced layout height from the keyboard
        // taking up the bottom space.
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);

        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Expect fullViewportHeight since in OVERLAYS_CONTENT the keyboard doesn't cause a resize
        // to the WebContents.
        verify(mWebContents, times(1)).setSize(fullViewportWidth, fullViewportHeight);
        verify(mCompositorViewHolder, times(1))
                .notifyVirtualKeyboardOverlayRect(
                        mWebContents, 0, 0, fullViewportWidth, KEYBOARD_HEIGHT);

        reset(mWebContents);

        // Hide the keyboard.
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mKeyboardInsetSupplier.set(0);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        verify(mWebContents, times(1)).setSize(fullViewportWidth, fullViewportHeight);
        verify(mCompositorViewHolder, times(1))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testOverlayGeometryNotTriggeredDueToNoKeyboard() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int viewportHeight = 941;
        int viewportWidth = 1080;

        // Simulate the keyboard being hidden
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        when(mCompositorViewHolder.getWidth()).thenReturn(viewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(viewportHeight);
        mKeyboardInsetSupplier.set(0);

        // Ensure updating the WebContents size doesn't dispatch a keyboard geometry event to
        // web content. The updateWebContentsSize call simulates the Views layout that happens as a
        // result of the keyboard showing, which happens after the inset is set.
        mCompositorViewHolder.updateWebContentsSize(mTab);
        verify(mWebContents, times(1)).setSize(viewportWidth, viewportHeight);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeWhenInOSKResizesVisualMode() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);
        reset(mWebContents);

        // Viewport dimensions while keyboard is hidden.
        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from the keyboard taking up the bottom space.
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);

        mCompositorViewHolder.updateWebContentsSize(mTab);

        // In RESIZES_VISUAL mode, CompositorViewHolder ensures that size changes from the virtual
        // keyboard don't affect the WebContents' size.
        verify(mWebContents, times(1)).setSize(fullViewportWidth, fullViewportHeight);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeWhenInOSKResizesContentMode() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        reset(mWebContents);

        // Viewport dimensions while keyboard is hidden.
        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from the keyboard taking up the bottom space.
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);

        // In RESIZES_CONTENT mode, CompositorViewHolder resizes the WebContents by the keyboard
        // height.
        verify(mWebContents, times(1)).setSize(fullViewportWidth, adjustedHeight - TOOLBAR_HEIGHT);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeByBottomSheetInset() {
        var bottomSheetInsetSupplier = new ObservableSupplierImpl<Integer>();
        mViewportInsets.setBottomSheetInsetSupplier(bottomSheetInsetSupplier);
        reset(mWebContents);

        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;
        int bottomSheetOffset = 420;

        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        bottomSheetInsetSupplier.set(bottomSheetOffset);

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from bottom sheet taking up the space at the bottom.
        int adjustedHeight = fullViewportHeight - bottomSheetOffset;
        verify(mWebContents, times(1)).setSize(fullViewportWidth, adjustedHeight - TOOLBAR_HEIGHT);
    }

    @Test
    public void testOverlayGeometryWhenViewNotAttachedToWindow() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        when(mContentView.getWindowToken()).thenReturn(null);
        // Viewport dimensions while keyboard is hidden.
        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from the keyboard taking up the bottom space.
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);

        // Ensure updateWebContentsSize in OVERLAYS_CONTENT mode doesn't send keyboard geometry
        // events to content if the view is detached.
        mCompositorViewHolder.updateWebContentsSize(mTab);
        verify(mCompositorViewHolder, times(0))
                .notifyVirtualKeyboardOverlayRect(mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testAccessoryInsetsResizeWebContents() {
        int viewportHeight = 800;
        int viewportWidth = 300;
        int accessoryHeight = 500;

        when(mCompositorViewHolder.getWidth()).thenReturn(viewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(viewportHeight);

        // This is only relevant for RESIZES_CONTENT mode since in RESIZES_VISUAL or
        // OVERLAYS_CONTENT the WebContents does not need to be resized by keyboard-related insets.
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        // Updating the VirtualKeyboardMode will update the viewport size. The test is setup so the
        // browser controls are showing so they'll be subtracted from the viewport height.
        verify(mWebContents, times(1)).setSize(viewportWidth, viewportHeight - TOOLBAR_HEIGHT);

        reset(mWebContents);

        // Simulate showing a keyboard accessory of some kind. This should cause the WebContents to
        // be resized without any other action.
        mKeyboardAccessoryInsetSupplier.set(accessoryHeight);

        verify(mWebContents, times(1))
                .setSize(viewportWidth, viewportHeight - accessoryHeight - TOOLBAR_HEIGHT);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END)
    public void testInMotionSupplier() {
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_DOWN);
        mCompositorViewHolder.onInterceptTouchEvent(MOTION_EVENT_DOWN);
        Assert.assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_UP);
        mCompositorViewHolder.onInterceptTouchEvent(MOTION_EVENT_UP);
        Assert.assertFalse(mCompositorViewHolder.getInMotionSupplier().get());

        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_DOWN);
        mCompositorViewHolder.onInterceptTouchEvent(MOTION_EVENT_DOWN);
        Assert.assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        // Simulate a child handling a scroll, where they call requestDisallowInterceptTouchEvent
        // and then we no longer get onInterceptTouchEvent. The dispatchTouchEvent alone should
        // still cause our motion status to correctly update.
        mCompositorViewHolder.requestDisallowInterceptTouchEvent(true);
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_UP);
        Assert.assertFalse(mCompositorViewHolder.getInMotionSupplier().get());
    }

    @Test
    public void testGestureBeginEndInMotionSupplier() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);

        mCompositorViewHolder.onContentChanged();
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getAllValues().forEach((obs) -> obs.onGestureBegin());
        Assert.assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        mTabObserverCaptor.getAllValues().forEach((obs) -> obs.onGestureEnd());
        Assert.assertFalse(mCompositorViewHolder.getInMotionSupplier().get());
    }

    @Test
    public void testOnInterceptHoverEvent() {
        when(mMockKeyboard.isKeyboardShowing(any(), any())).thenReturn(false);
        when(mLayoutManager.onInterceptMotionEvent(
                        MOTION_ACTION_HOVER_ENTER, false, EventType.HOVER))
                .thenReturn(true);
        boolean intercepted =
                mCompositorViewHolder.onInterceptHoverEvent(MOTION_ACTION_HOVER_ENTER);
        verify(mLayoutManager)
                .onInterceptMotionEvent(MOTION_ACTION_HOVER_ENTER, false, EventType.HOVER);
        Assert.assertTrue(
                "#onInterceptHoverEvent should return true if the LayoutManager intercepts the"
                        + " event.",
                intercepted);
    }

    @Test
    public void testOnHoverEvent() {
        when(mLayoutManager.onHoverEvent(MOTION_ACTION_HOVER_ENTER)).thenReturn(true);
        boolean consumed = mCompositorViewHolder.onHoverEvent(MOTION_ACTION_HOVER_ENTER);
        verify(mLayoutManager).onHoverEvent(MOTION_ACTION_HOVER_ENTER);
        Assert.assertTrue(
                "#onHoverEvent should return true if the LayoutManager consumes the event.",
                consumed);
    }

    @Test
    public void testInMotionOrdering() {
        // With the 'defer in motion' experiment enabled, touch events are routed to android UI
        // after being sent to native/web content.
        List<EventSource> eventSequence = observeTouchAndMotionEvents();
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_DOWN);
        assertEquals(
                Arrays.asList(EventSource.TOUCH_EVENT_OBSERVER, EventSource.IN_MOTION),
                eventSequence);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures(ChromeFeatureList.DELAY_TEMP_STRIP_REMOVAL)
    public void testSetBackgroundRunnable_NoDelay() {
        int pendingFrameCount = 0;
        int framesUntilHideBackground = 1;
        boolean swappedCurrentSize = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabStrip.TimeToInitializeTabStateAfterBufferSwap");

        // Mark that a frame has swapped, and the buffer has swapped once (still waiting on one).
        mCompositorViewHolder.didSwapFrame(pendingFrameCount);
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundNotRemoved();

        // Mark that the buffer has swapped a second time (and we're no longer waiting on one).
        framesUntilHideBackground = 0;
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundRemoved();

        // Verify the relevant histogram is recorded.
        mTabModelSelector.markTabStateInitialized();
        histogramWatcher.assertExpected(
                "Should have recorded time to initialize tab state after buffer swap.");
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.DELAY_TEMP_STRIP_REMOVAL)
    public void testSetBackgroundRunnable_Delay_TabStateInitialized() {
        int pendingFrameCount = 0;
        int framesUntilHideBackground = 0;
        boolean swappedCurrentSize = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabStrip.TimeToInitializeTabStateAfterBufferSwap");

        // Mark a tab has restored, a frame has swapped, and the buffer has swapped enough times.
        notifyTabRestored();
        mCompositorViewHolder.didSwapFrame(pendingFrameCount);
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundNotRemoved();

        // Mark the tab state as initialized and verify that the temp background is now removed.
        mTabModelSelector.markTabStateInitialized();
        verifyBackgroundRemoved();

        // Verify the relevant histogram is recorded.
        histogramWatcher.assertExpected(
                "Should have recorded time to initialize tab state after buffer swap.");
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.DELAY_TEMP_STRIP_REMOVAL)
    public void testSetBackgroundRunnable_Delay_TimedOut() {
        int pendingFrameCount = 0;
        int framesUntilHideBackground = 0;
        boolean swappedCurrentSize = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabStrip.TimeToInitializeTabStateAfterBufferSwap");

        // Mark a tab has restored, a frame has swapped, and the buffer has swapped enough times.
        notifyTabRestored();
        mCompositorViewHolder.didSwapFrame(pendingFrameCount);
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundNotRemoved();

        // Fake the timeout and verify that the temp background is now removed.
        timeoutRunnable();
        verifyBackgroundRemoved();

        // Verify the relevant histogram is recorded.
        mTabModelSelector.markTabStateInitialized();
        histogramWatcher.assertExpected(
                "Should have recorded time to initialize tab state after buffer swap.");
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.DELAY_TEMP_STRIP_REMOVAL)
    public void testSetBackgroundRunnable_Delay_CompositorNotReady() {
        int pendingFrameCount = 0;
        int framesUntilHideBackground = 1;
        boolean swappedCurrentSize = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabStrip.TimeToBufferSwapAfterInitializeTabState");

        // Mark the tab state as initialized and one frame has been swapped.
        notifyTabRestored();
        mTabModelSelector.markTabStateInitialized();
        mCompositorViewHolder.didSwapFrame(pendingFrameCount);
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        timeoutRunnable();
        verifyBackgroundNotRemoved();

        // Mark the buffer has swapped enough times and verify the temp background is now removed.
        framesUntilHideBackground = 0;
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundRemoved();

        // Verify the relevant histogram is recorded.
        histogramWatcher.assertExpected(
                "Should have recorded time to buffer swap after initializing tab state.");
    }

    private void notifyTabRestored() {
        // To avoid some complexities, we don't actually add a tab to the MockTabModel(Selector) and
        // instead use the method called whenever the CompositorViewHolder is notified of a new tab.
        mCompositorViewHolder.maybeInitializeSetBackgroundRunnableTimeout();
    }

    private static void runCurrentTasks() {
        ShadowLooper.runUiThreadTasks();
    }

    private static void timeoutRunnable() {
        // The timeout is implemented as a delayed task.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    private void verifyBackgroundNotRemoved() {
        runCurrentTasks();
        verify(mCompositorView, never()).setBackgroundResource(anyInt());
    }

    private void verifyBackgroundRemoved() {
        runCurrentTasks();
        verify(mCompositorView, times(1)).setBackgroundResource(anyInt());
    }
}
