// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.IBinder;
import android.util.Size;
import android.view.ContextThemeWrapper;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.Window;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.InputHintChecker;
import org.chromium.base.InputHintCheckerJni;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.EventFilter.EventType;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
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
import org.chromium.ui.base.ApplicationViewportInsetTracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
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

    private static final int SIDE_UI_START_WIDTH = 60;
    private static final int SIDE_UI_END_WIDTH = 70;

    private static final long TOUCH_TIME = 0;
    private static final MotionEvent MOTION_EVENT_DOWN =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_DOWN, 1, 1, 0);
    private static final MotionEvent MOTION_EVENT_UP =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_UP, 1, 1, 0);

    private static final MotionEvent MOTION_ACTION_HOVER_ENTER =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_HOVER_ENTER, 1, 1, 0);

    private static final MotionEvent MOTION_ACTION_BUTTON_RELEASE_MOUSE;

    private static final WindowInsetsCompat VISIBLE_SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.of(0, 100, 0, 100))
                    .build();

    static {
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = 1f;
        coords[0].y = 1f;

        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MOTION_ACTION_BUTTON_RELEASE_MOUSE =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_BUTTON_RELEASE,
                        1,
                        properties,
                        coords,
                        0,
                        0,
                        1f,
                        1f,
                        0,
                        0,
                        InputDevice.SOURCE_CLASS_POINTER,
                        0);
    }

    private static final class EventSource {
        static final int IN_MOTION = 0;
        static final int TOUCH_EVENT_OBSERVER = 1;

        private EventSource() {}
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private ToolbarControlContainer mControlContainer;
    @Mock private View mContainerView;
    @Mock private Resources mResources;
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
    @Mock private InputHintChecker.Natives mInputHintCheckerJni;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private InsetObserver mInsetObserver;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private SideUiStateProvider mSideUiStateProvider;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private Context mContext;
    private MockTabModelSelector mTabModelSelector;
    private Tab mTab;
    private CompositorViewHolder mCompositorViewHolder;
    private BrowserControlsManager mBrowserControlsManager;
    private ApplicationViewportInsetTracker mViewportInsets;
    private SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier;
    private SettableNonNullObservableSupplier<Integer> mKeyboardAccessoryInsetSupplier;
    private OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier;
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        OnscreenContentProviderJni.setInstanceForTesting(mOnscreenContentProviderJni);
        ContentCaptureFeaturesJni.setInstanceForTesting(mContentCaptureFeaturesJni);
        InputHintCheckerJni.setInstanceForTesting(mInputHintCheckerJni);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        // Setup the mock keyboard.
        KeyboardVisibilityDelegate.setInstanceForTesting(mMockKeyboard);

        mViewportInsets = ApplicationViewportInsetTracker.createForTests();
        when(mInsetObserver.isKeyboardInOverlayMode()).thenReturn(false);
        mViewportInsets.setInsetObserver(mInsetObserver);

        mKeyboardInsetSupplier = ObservableSuppliers.createNonNull(0);
        mViewportInsets.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardAccessoryInsetSupplier = ObservableSuppliers.createNonNull(0);
        mViewportInsets.setKeyboardAccessoryInsetSupplier(mKeyboardAccessoryInsetSupplier);
        mSideUiStateProviderSupplier = new OneshotSupplierImpl<>();

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
                        mActivity,
                        BrowserControlsStateProvider.ControlsPosition.TOP,
                        mMultiWindowModeStateDispatcher);
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

        mCompositorViewHolder.setTopUiThemeColorProvider(mTopUiThemeColorProvider);
        mCompositorViewHolder.setLayoutManager(mLayoutManager);
        mCompositorViewHolder.setControlContainer(mControlContainer);
        mCompositorViewHolder.setCompositorViewForTesting(mCompositorView);
        mCompositorViewHolder.setBrowserControlsManager(mBrowserControlsManager);
        mCompositorViewHolder.setApplicationViewportInsetSupplier(mViewportInsets);
        mCompositorViewHolder.onFinishNativeInitialization(
                mTabModelSelector, null, ObservableSuppliers.alwaysZero());
        mCompositorViewHolder.setSideUiStateProviderSupplier(mSideUiStateProviderSupplier);
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

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
    }

    private List<Integer> observeTouchAndMotionEvents() {
        List<Integer> eventSequence = new ArrayList<>();
        mCompositorViewHolder
                .getInMotionSupplier()
                .addSyncObserverAndPostIfNonNull(
                        (inMotion) -> eventSequence.add(EventSource.IN_MOTION));
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

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
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
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
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
    public void testWebContentResizeTriggeredDueToKeyboardTransition_hasNoTransientOvershoot() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);

        // Establish the baseline viewport size before keyboard insets change.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        reset(mWebContents);

        // Keyboard show: inset is updated before layout applies the reduced view height.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // After layout, view height is reduced and compensation should still keep size stable.
        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Keyboard hide: inset clears before layout restores the full view height.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);
        mKeyboardInsetSupplier.set(0);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // After layout restoration, size should remain stable.
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        verify(mWebContents, atLeast(1)).setSize(fullViewportWidth, fullViewportHeight);
        verify(mWebContents, never())
                .setSize(fullViewportWidth, fullViewportHeight + KEYBOARD_HEIGHT);
        verify(mWebContents, never()).setSize(fullViewportWidth, adjustedHeight);
    }

    @Test
    public void
            testWebContentResizeTriggeredDueToKeyboardTransition_hasNoIntermediateInsetOvershoot() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;
        int intermediateViewportHeight = fullViewportHeight - 16;

        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);

        // Establish baseline before keyboard transition starts.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        reset(mWebContents);

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);

        // Small keyboard inset step arrives before the view height update.
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mKeyboardInsetSupplier.set(16);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Next inset step arrives after the view has partially resized but before the previous
        // viewport-height update was observed by updateWebContentsSize().
        when(mCompositorViewHolder.getHeight()).thenReturn(intermediateViewportHeight);
        mKeyboardInsetSupplier.set(32);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        ArgumentCaptor<Integer> resizedHeightCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mWebContents, atLeast(1))
                .setSize(eq(fullViewportWidth), resizedHeightCaptor.capture());
        for (int observedHeight : resizedHeightCaptor.getAllValues()) {
            Assert.assertTrue(
                    "Unexpected transient overshoot: "
                            + observedHeight
                            + " > "
                            + fullViewportHeight,
                    observedHeight <= fullViewportHeight);
        }
    }

    @Test
    @DisableFeatures(ChromeFeatureList.VIRTUAL_KEYBOARD_TRANSIENT_INNER_HEIGHT_FIX)
    public void
            testWebContentResizeTriggeredDueToKeyboardTransition_withKillSwitch_usesLegacySizing() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        when(mCompositorViewHolder.getHeight()).thenReturn(adjustedHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        verify(mWebContents, atLeast(1))
                .setSize(fullViewportWidth, fullViewportHeight + KEYBOARD_HEIGHT);
    }

    // Keyboard resize tests for geometrychange event fired to JS.
    @Test
    public void testWebContentResizeTriggeredDueToKeyboardShow_keyboardInOverlayMode() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        // Viewport dimensions when keyboard is hidden.
        int fullViewportHeight = 941;
        int fullViewportWidth = 1080;

        // adjustedHeight is the height of the CompositorViewHolder from Android View layout
        // after showing the keyboard. This simulates a reduced layout height from the keyboard
        // taking up the bottom space.
        int adjustedHeight = fullViewportHeight - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        // The CompositorViewHolder does not account for the keyboard since the keyboard inset has
        // been consumed by an inset consumer, which deliberately did not update the CVH.
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        when(mInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);

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
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        when(mCompositorViewHolder.getWidth()).thenReturn(fullViewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(fullViewportHeight);
        when(mInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);
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
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
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

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
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

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
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
        SettableMonotonicObservableSupplier<Integer> bottomSheetInsetSupplier =
                ObservableSuppliers.createMonotonic();
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

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
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
    @DisableFeatures({
        ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END,
        ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX
    })
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
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
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
    @EnableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
    public void testInMotionSupplier_OnTouch() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getAllValues().forEach((obs) -> obs.onTouchDown());
        Assert.assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        mTabObserverCaptor.getAllValues().forEach((obs) -> obs.onTouchUp());
        Assert.assertFalse(mCompositorViewHolder.getInMotionSupplier().get());
    }

    @Test
    public void testOnInterceptHoverEvent() {
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
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
    public void testDispatchGenericMotionEvent() {
        when(mLayoutManager.dispatchGenericMotionEvent(MOTION_ACTION_BUTTON_RELEASE_MOUSE))
                .thenReturn(true);
        boolean consumed =
                mCompositorViewHolder.dispatchGenericMotionEvent(
                        MOTION_ACTION_BUTTON_RELEASE_MOUSE);
        verify(mLayoutManager).dispatchGenericMotionEvent(MOTION_ACTION_BUTTON_RELEASE_MOUSE);
        Assert.assertTrue(
                "#dispatchGenericMotionEvent should return true if the LayoutManager consumes the"
                        + " event.",
                consumed);
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX,
        ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END
    })
    public void testInMotionOrdering() {
        // With the 'defer in motion' experiment enabled, touch events are routed to android UI
        // after being sent to native/web content.
        List<Integer> eventSequence = observeTouchAndMotionEvents();
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_DOWN);
        assertEquals(
                Arrays.asList(EventSource.TOUCH_EVENT_OBSERVER, EventSource.IN_MOTION),
                eventSequence);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testSetBackgroundRunnable() {
        // Trigger a compositor layout. Verify the background has not yet been removed.
        mCompositorViewHolder.onCompositorLayout();
        verifyBackgroundNotRemoved();

        // Mark that a frame has swapped, but the buffer has not yet swapped. Verify the background
        // has not yet been removed.
        int pendingFrameCount = 0;
        mCompositorViewHolder.didSwapFrame(pendingFrameCount);
        verifyBackgroundNotRemoved();

        // Mark that the buffer has swapped. Verify the background has now been removed
        boolean swappedCurrentSize = true;
        int framesUntilHideBackground = 1;
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundRemoved();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testSetBackgroundRunnable_Timeout() {
        // Run delayed tasks (timing out the background runnable), then verify the background has
        // been removed.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyBackgroundRemoved();
    }

    @Test
    public void testFocusOnWebContent_resetsKeyboardFocus() {
        mCompositorViewHolder.setFocusOnFirstContentViewItem();
        verify(mCompositorViewHolder).resetKeyboardFocus();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOOLBAR_STALE_CAPTURE_BUG_FIX)
    public void testOnControlsOffsetChanged_NoRequestRenderIfScrolling() {
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_DOWN);
        mCompositorViewHolder.onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        verify(mCompositorView, never()).requestRender();
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_UP);

        mCompositorViewHolder.setContentViewScrollingStateForTesting(true);
        mCompositorViewHolder.onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        verify(mCompositorView, never()).requestRender();
        mCompositorViewHolder.setContentViewScrollingStateForTesting(false);
    }

    @Test
    public void testOnControlsOffsetChanged_RequestRender() {
        mCompositorViewHolder.onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        verify(mCompositorView, times(1)).requestRender();
    }

    @Test
    public void testActiveTouchInterceptors() {
        TouchEventObserver observer = mock(TouchEventObserver.class);
        when(observer.mayInterceptTouchSequenceInWebContents()).thenReturn(true);

        mCompositorViewHolder.addTouchEventObserver(observer);
        verify(mCompositorView).setHasActiveTouchInterceptors(eq(true));
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(observer);
        verify(mCompositorView).setHasActiveTouchInterceptors(eq(false));
    }

    @Test
    public void testAsymmetricTouchInterceptors() {
        TouchEventObserver observer = mock(TouchEventObserver.class);
        when(observer.mayInterceptTouchSequenceInWebContents()).thenReturn(true);

        mCompositorViewHolder.addTouchEventObserver(null);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.addTouchEventObserver(observer);
        verify(mCompositorView).setHasActiveTouchInterceptors(eq(true));
        reset(mCompositorView);

        mCompositorViewHolder.addTouchEventObserver(observer);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(null);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(observer);
        verify(mCompositorView).setHasActiveTouchInterceptors(eq(false));
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(observer);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);
    }

    @Test
    public void testMultipleActiveTouchInterceptors() {
        TouchEventObserver observer1 = mock(TouchEventObserver.class);
        when(observer1.mayInterceptTouchSequenceInWebContents()).thenReturn(true);
        TouchEventObserver observer2 = mock(TouchEventObserver.class);
        when(observer2.mayInterceptTouchSequenceInWebContents()).thenReturn(true);
        TouchEventObserver observer3 = mock(TouchEventObserver.class);
        when(observer3.mayInterceptTouchSequenceInWebContents()).thenReturn(false);

        mCompositorViewHolder.addTouchEventObserver(observer1);
        verify(mCompositorView).setHasActiveTouchInterceptors(eq(true));
        reset(mCompositorView);

        mCompositorViewHolder.addTouchEventObserver(observer2);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.addTouchEventObserver(observer3);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(observer3);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(observer2);
        verify(mCompositorView, never()).setHasActiveTouchInterceptors(anyBoolean());
        reset(mCompositorView);

        mCompositorViewHolder.removeTouchEventObserver(observer1);
        verify(mCompositorView).setHasActiveTouchInterceptors(eq(false));
    }

    private static void runCurrentTasks() {
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void verifyBackgroundNotRemoved() {
        runCurrentTasks();
        verify(mCompositorView, never()).setBackgroundResource(anyInt());
    }

    private void verifyBackgroundRemoved() {
        runCurrentTasks();
        verify(mCompositorView, times(1)).setBackgroundResource(anyInt());
    }

    @Test
    public void testAccessibilityNode_boundsAreCorrect() {
        mContext.getResources().getDisplayMetrics().density = 1.375f;

        var virtualView = mock(VirtualView.class);
        // Values in this test case are real numbers captured from clank running
        // in a maximized window.
        RectF dpRect = new RectF(100.36364f, 2.18182f, 337.36365f, 42.18182f);
        doAnswer(
                        invocation -> {
                            ((RectF) invocation.getArgument(0)).set(dpRect);
                            return null;
                        })
                .when(virtualView)
                .getTouchTarget(any(RectF.class));
        when(virtualView.getAccessibilityDescription()).thenReturn("test-node");
        doAnswer(
                        invocation -> {
                            List<VirtualView> list = invocation.getArgument(0);
                            list.add(virtualView);
                            return null;
                        })
                .when(mLayoutManager)
                .getVirtualViews(anyList());

        mCompositorViewHolder.onAccessibilityModeChanged(true);
        assertNotNull(mCompositorViewHolder.mAccessibilityView);

        mCompositorViewHolder.mAccessibilityView.createAccessibilityNodeInfo();
        var accessibilityProvider =
                mCompositorViewHolder.mAccessibilityView.getAccessibilityNodeProvider();
        assertNotNull(accessibilityProvider);
        var nodeInfo = accessibilityProvider.createAccessibilityNodeInfo(0);
        Rect actualRect = new Rect();
        nodeInfo.getBoundsInScreen(actualRect);
        // Top coordinate: 2.18182 * 1.375 = 3.0000025. Should be floored or
        // rounded to 3.
        Rect expectedRect = new Rect(138, 3, 464, 59);
        assertEquals(expectedRect, actualRect);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testSetSideUiStateProviderSupplier() {
        when(mSideUiStateProvider.measureSideUiSpecs()).thenReturn(SideUiSpecs.EMPTY_SIDE_UI_SPECS);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        verify(mSideUiStateProvider).addObserver(mCompositorViewHolder);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testOnSideUiSpecsChanged_updateWebContentsSize() {
        // Setup.
        reset(mWebContents);

        // Viewport dimensions when keyboard is hidden.
        int viewportHeight = 941;
        int viewportWidth = 1080;
        when(mCompositorViewHolder.getWidth()).thenReturn(viewportWidth);
        when(mCompositorViewHolder.getHeight()).thenReturn(viewportHeight);

        // Arbitrary Side UI width.
        int startContainerWidth = 100;
        int endContainerWidth = 200;
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(startContainerWidth, endContainerWidth);
        when(mSideUiStateProvider.measureSideUiSpecs()).thenReturn(currentSideUiSpecs);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Act. Pass empty specs, as the CompositorViewHolder is expected to instead query from
        // the set SideUiStateProvider.
        mCompositorViewHolder.onSideUiSpecsChanged(SideUiSpecs.EMPTY_SIDE_UI_SPECS);

        // Verify.
        verify(mWebContents)
                .setSize(viewportWidth - (startContainerWidth + endContainerWidth), viewportHeight);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testOnSideUiSpecsChanged_updateContentOffsetX() {
        doTestOnSideUiSpecsChanged_updateContentOffsetX(/* shouldBeRtl= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testOnSideUiSpecsChanged_updateContentOffsetX_rtl() {
        doTestOnSideUiSpecsChanged_updateContentOffsetX(/* shouldBeRtl= */ true);
    }

    private void doTestOnSideUiSpecsChanged_updateContentOffsetX(boolean shouldBeRtl) {
        // Setup.
        LocalizationUtils.setRtlForTesting(shouldBeRtl);
        reset(mWebContents);

        // Arbitrary Side UI width.
        int startContainerWidth = 50;
        int endContainerWidth = 150;
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(startContainerWidth, endContainerWidth);
        when(mSideUiStateProvider.measureSideUiSpecs()).thenReturn(currentSideUiSpecs);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Act.
        mCompositorViewHolder.onSideUiSpecsChanged(currentSideUiSpecs);

        // Verify.
        int expectedContentOffsetX = shouldBeRtl ? endContainerWidth : startContainerWidth;
        verify(mLayoutManager).setContentOffsetX(expectedContentOffsetX);
    }

    @Test
    public void testOnSideUiSpecsChanged_contentViewMarginsNotUpdated() {
        // Setup content view.
        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(0, 0);
        when(mContentView.getLayoutParams()).thenReturn(marginLayoutParams);

        doTestSideUiSpecsChanged_updateMargins(
                /* expectedStartMargin= */ 0, /* expectedEndMargin= */ 0);
    }

    @Test
    public void testOnSideUiSpecsChanged_customViewMarginsUpdated() {
        // Setup custom view.
        View customView = new View(mContext);
        when(mTab.isShowingCustomView()).thenReturn(true);
        when(mTab.getView()).thenReturn(customView);

        doTestSideUiSpecsChanged_updateMargins(SIDE_UI_START_WIDTH, SIDE_UI_END_WIDTH);
    }

    private void doTestSideUiSpecsChanged_updateMargins(
            int expectedStartMargin, int expectedEndMargin) {
        // Notify content changed.
        reset(mWebContents);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();

        // Arbitrary Side UI width.
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(SIDE_UI_START_WIDTH, SIDE_UI_END_WIDTH);
        when(mSideUiStateProvider.measureSideUiSpecs()).thenReturn(currentSideUiSpecs);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();
        mCompositorViewHolder.onSideUiSpecsChanged(currentSideUiSpecs);

        // Verify layout params.
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTab.getView().getLayoutParams();
        assertEquals(
                "Unexpected start margin.", expectedStartMargin, layoutParams.getMarginStart());
        assertEquals("Unexpected end margin.", expectedEndMargin, layoutParams.getMarginEnd());
    }

    @Test
    public void testGetLastNormalSize() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(mWindowAndroid, null, mPrefService);

        assertEquals(new Size(0, 0), mCompositorViewHolder.getLastNormalSize());

        mCompositorViewHolder.layout(0, 0, 100, 200);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        assertEquals(new Size(100, 200), mCompositorViewHolder.getLastNormalSize());

        when(mActivity.isInPictureInPictureMode()).thenReturn(true);
        mCompositorViewHolder.layout(0, 0, 50, 50);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        assertEquals(new Size(100, 200), mCompositorViewHolder.getLastNormalSize());

        when(mActivity.isInPictureInPictureMode()).thenReturn(false);
        mCompositorViewHolder.layout(0, 0, 300, 400);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        assertEquals(new Size(300, 400), mCompositorViewHolder.getLastNormalSize());
    }
}
