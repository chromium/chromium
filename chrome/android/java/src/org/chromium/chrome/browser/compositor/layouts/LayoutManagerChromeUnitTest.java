// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link LayoutManagerChrome}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LayoutManagerChromeUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private @Mock LayoutManagerHost mHost;
    private @Mock HubLayoutDependencyHolder mHubLayoutDependencyHolder;
    private @Mock ToolbarSwipeLayout mToolbarSwipeLayout;
    private @Mock Tab mTab;
    private @Mock WebContents mWebContents;
    private @Mock TabBrowserControlsConstraintsHelper mConstraintsHelper;
    private @Mock BrowserControlsManager mBrowserControlsManager;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    private @Mock StaticLayout mStaticLayout;
    private @Mock HubLayout mHubLayout;
    private @Mock TabModelSelector mTabModelSelector;

    private final ViewGroup mContentContainer =
            new FrameLayout(ContextUtils.getApplicationContext());
    private final SettableNullableObservableSupplier<TabSwitcher> mTabSwitcherSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<TabContentManager>
            mTabContentManagerSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<ToolbarThemeColorProvider>
            mToolbarThemeColorProvider = ObservableSuppliers.createNullable();

    @Before
    public void setUp() {
        mBrowserVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.createNonNull(false));
        when(mHost.getBrowserControlsManager()).thenReturn(mBrowserControlsManager);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserVisibilityDelegate);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (TestActivity activity) -> when(mHost.getContext()).thenReturn(activity));
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
    }

    @Test
    public void testShowAfterFoo() {
        LayoutManagerChrome layoutManagerChrome =
                new LayoutManagerChrome(
                        mHost,
                        mContentContainer,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        mTabContentManagerSupplier,
                        mToolbarThemeColorProvider,
                        mHubLayoutDependencyHolder);
        layoutManagerChrome.destroy();
        layoutManagerChrome.showLayout(LayoutType.HUB, /* animate= */ true);
        verifyNoInteractions(mHubLayoutDependencyHolder);
    }

    @Test
    public void testSwitchToTab_SameTabId_NoAnimation() {
        LayoutManagerChrome layoutManagerChrome =
                new LayoutManagerChrome(
                        mHost,
                        mContentContainer,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        mTabContentManagerSupplier,
                        mToolbarThemeColorProvider,
                        mHubLayoutDependencyHolder);
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(mTab.getId()).thenReturn(1);

        layoutManagerChrome.switchToTab(mTab, 1);

        verifyNoInteractions(mToolbarSwipeLayout);
    }

    @Test
    public void testSwitchToTab_DifferentTabId_TriggersAnimation() {
        LayoutManagerChrome layoutManagerChrome =
                new LayoutManagerChrome(
                        mHost,
                        mContentContainer,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        mTabContentManagerSupplier,
                        mToolbarThemeColorProvider,
                        mHubLayoutDependencyHolder);
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(mTab.getId()).thenReturn(2);

        layoutManagerChrome.switchToTab(mTab, 1);

        verify(mToolbarSwipeLayout).setSwitchToTab(2, 1);
        Assert.assertEquals(mToolbarSwipeLayout, layoutManagerChrome.getActiveLayout());
    }

    @Test
    public void testTabClosed_ActivityNotFinishing_TriggersHubLayoutCreation() {
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();

        layoutManagerChrome.tabClosed(1, Tab.INVALID_TAB_ID, false, false);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testTabClosed_ActivityFinishing_NoHubLayoutCreation() {
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();

        mActivityScenarioRule.getScenario().onActivity(activity -> activity.finish());

        layoutManagerChrome.tabClosed(1, Tab.INVALID_TAB_ID, false, false);

        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testTabsAllClosing_ActivityNotFinishing_TriggersHubLayoutCreation() {
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        layoutManagerChrome.tabsAllClosing(false);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testTabsAllClosing_ActivityFinishing_NoHubLayoutCreation() {
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        mActivityScenarioRule.getScenario().onActivity(activity -> activity.finish());

        layoutManagerChrome.tabsAllClosing(false);

        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testSetContentOffsetX_UpdatesStaticLayoutAndHubLayout() {
        LayoutManagerChrome layoutManagerChrome =
                new LayoutManagerChrome(
                        mHost,
                        mContentContainer,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        mTabContentManagerSupplier,
                        mToolbarThemeColorProvider,
                        mHubLayoutDependencyHolder);
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mHubLayout = mHubLayout;

        layoutManagerChrome.setContentOffsetX(120);

        verify(mStaticLayout).setContentOffsetX(120);
        verify(mHubLayout).setContentOffsetX(120);
        Assert.assertEquals(120, layoutManagerChrome.getContentOffsetXForTesting());
    }

    @Test
    public void testSetContentOffsetX_BeforeLayoutsInitialized() {
        LayoutManagerChrome layoutManagerChrome =
                new LayoutManagerChrome(
                        mHost,
                        mContentContainer,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        mTabContentManagerSupplier,
                        mToolbarThemeColorProvider,
                        mHubLayoutDependencyHolder);
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.setContentOffsetX(75);

        Assert.assertEquals(75, layoutManagerChrome.getContentOffsetXForTesting());
    }

    @Test
    public void testWillAddedTabBeSelected() {
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        when(layoutManagerChrome.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);

        LayoutManagerImpl.LayoutManagerTabModelObserver observer =
                layoutManagerChrome.createTabModelObserver();

        // 1. Foreground launch types should return true (added tab will be selected/activated).
        Assert.assertTrue(
                observer.willAddedTabBeSelected(TabLaunchType.FROM_LINK, /* incognito= */ false));

        // 2. Background launch types should return false (added tab will remain
        // background/inactive).
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_TAB_LIST_INTERFACE_BACKGROUND, /* incognito= */ false));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND, /* incognito= */ false));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND, /* incognito= */ false));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_REPARENTING_BACKGROUND, /* incognito= */ false));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_RESTORE, /* incognito= */ false));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(TabLaunchType.FROM_RESTORE, /* incognito= */ true));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_RESTORE_TABS_UI, /* incognito= */ false));
        Assert.assertTrue(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_RESTORE_TABS_UI, /* incognito= */ true));
        Assert.assertFalse(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_BROWSER_ACTIONS, /* incognito= */ false));
        Assert.assertTrue(
                observer.willAddedTabBeSelected(
                        TabLaunchType.FROM_BROWSER_ACTIONS, /* incognito= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testToolbarSwipe_isSwipeEnabled_disabledOnDesktop() {
        DeviceInfo.setIsDesktopForTesting(true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ !TabSwitcherUtils
                                .isGridTabSwitcherDisabled());
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        // Vertical swipe (to show tab switcher) should be disabled.
        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, event));
        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.UP, event));

        // Horizontal swipe (to switch tabs) should remain enabled.
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.LEFT, event));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.RIGHT, event));
    }

    @Test
    public void testToolbarSwipe_isSwipeEnabled_enabledOnPhone() {
        DeviceInfo.setIsDesktopForTesting(false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ !TabSwitcherUtils
                                .isGridTabSwitcherDisabled());
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        // Vertical swipe (DOWN for top toolbar) should be enabled.
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, event));

        // Horizontal swipe should also be enabled.
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.LEFT, event));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.RIGHT, event));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testToolbarSwipe_onSwipeUpdated_disabledOnDesktop_doesNotShowHub() {
        DeviceInfo.setIsDesktopForTesting(true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ !TabSwitcherUtils
                                .isGridTabSwitcherDisabled());
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.UNKNOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, 0, 0, 100);

        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testToolbarSwipe_onSwipeUpdated_enabledOnPhone_showsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.UNKNOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, 0, 0, 100);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testTabClosed_disabledOnDesktop_doesNotShowHub() {
        DeviceInfo.setIsDesktopForTesting(true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();

        layoutManagerChrome.tabClosed(1, Tab.INVALID_TAB_ID, false, false);

        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testTabClosed_enabledOnPhone_showsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();

        layoutManagerChrome.tabClosed(1, Tab.INVALID_TAB_ID, false, false);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testTabsAllClosing_disabledOnDesktop_doesNotShowHub() {
        DeviceInfo.setIsDesktopForTesting(true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        layoutManagerChrome.tabsAllClosing(false);

        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testTabsAllClosing_enabledOnPhone_showsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        layoutManagerChrome.tabsAllClosing(false);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_GRID_TAB_SWITCHER)
    public void testTabModelSwitched_zeroTabs_disabledOnDesktop_doesNotShowHub() {
        DeviceInfo.setIsDesktopForTesting(true);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        TabModel normalTabModel = mock(TabModel.class);
        when(normalTabModel.getCount()).thenReturn(0);
        when(mTabModelSelector.getModel(false)).thenReturn(normalTabModel);

        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        doReturn(mTabModelSelector).when(layoutManagerChrome).getTabModelSelector();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        layoutManagerChrome.tabModelSwitched(false);

        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    public void testTabModelSwitched_zeroTabs_enabledOnPhone_showsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        TabModel normalTabModel = mock(TabModel.class);
        when(normalTabModel.getCount()).thenReturn(0);
        when(mTabModelSelector.getModel(false)).thenReturn(normalTabModel);

        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        doReturn(mTabModelSelector).when(layoutManagerChrome).getTabModelSelector();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);

        layoutManagerChrome.tabModelSwitched(false);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_topToolbar_swipeUpHidesBrowserControls() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.UP, startEvent));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, startEvent));

        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        // dy = -100 (upward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);
        swipeHandler.onSwipeFinished();

        verify(mConstraintsHelper).update(BrowserControlsState.HIDDEN, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_bottomToolbar_swipeDownHidesBrowserControls() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, startEvent));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.UP, startEvent));

        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, 100, 0, 100);
        swipeHandler.onSwipeFinished();

        verify(mConstraintsHelper).update(BrowserControlsState.HIDDEN, true);
        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_topToolbar_swipeDownShowsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, 100, 0, 100);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
        verify(mToolbarSwipeLayout, never())
                .swipeStarted(anyLong(), anyInt(), anyFloat(), anyFloat());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_bottomToolbar_swipeUpShowsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        // dy = -100 (upward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
        verify(mToolbarSwipeLayout, never())
                .swipeStarted(anyLong(), anyInt(), anyFloat(), anyFloat());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_swipeToHideControls_disabledWhenFlagDisabled() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.UP, startEvent));

        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);

        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_swipeToHideControls_preconditions() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);

        // 1. Native page should not allow swipe-to-hide.
        when(mTab.isNativePage()).thenReturn(true);
        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.UP, event));

        // 2. Null WebContents should not allow swipe-to-hide.
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(null);
        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.UP, event));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_runtimeTopOverrideWhenPrefIsBottom_usesLiveControlsPosition() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ false);
        when(mBrowserControlsManager.getControlsPosition()).thenReturn(ControlsPosition.TOP);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveDownEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveDownEvent, 0, 100, 0, 100);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());

        SwipeHandler swipeHandler2 =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent moveUpEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 0, 0);
        swipeHandler2.onSwipeStarted(ScrollDirection.UP, startEvent);
        // dy = -100 (upward swipe)
        swipeHandler2.onSwipeUpdated(moveUpEvent, 0, -100, 0, -100);
        swipeHandler2.onSwipeFinished();

        verify(mConstraintsHelper).update(BrowserControlsState.HIDDEN, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_topToolbar_lockedTopControls_swipeUpDoesNotHide() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        // Locked controls (e.g. on large tablets) have a min height equal to their height.
        when(mBrowserControlsManager.getTopControlsMinHeight()).thenReturn(100);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.UP, startEvent));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, startEvent));

        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        // dy = -100 (upward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);

        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_bottomToolbar_lockedBottomControls_swipeDownDoesNotHide() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ false);
        when(mBrowserControlsManager.getBottomControlsMinHeight()).thenReturn(100);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, startEvent));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.UP, startEvent));

        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, 100, 0, 100);

        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_flagDisabled_verticalSwipe_startsAndUpdatesSwipeLayout() {
        DeviceInfo.setIsDesktopForTesting(false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 200, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        // dy = 100 (downward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, 100, 0, 100);
        swipeHandler.onSwipeUpdated(moveEvent, 0, 100, 0, 100);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
        verify(mToolbarSwipeLayout)
                .swipeStarted(anyLong(), eq(ScrollDirection.DOWN), anyFloat(), anyFloat());
        // The swipe layout isn't active, but without the flag it still gets the updates.
        verify(mToolbarSwipeLayout)
                .swipeUpdated(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_swipeToHideControls_doesNotStartOrUpdateSwipeLayout() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        // dy = -100 (upward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);

        // The swipe layout never becomes active, so it isn't started or updated.
        verify(mConstraintsHelper).update(BrowserControlsState.HIDDEN, true);
        verify(mToolbarSwipeLayout, never())
                .swipeStarted(anyLong(), anyInt(), anyFloat(), anyFloat());
        verify(mToolbarSwipeLayout, never())
                .swipeUpdated(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_bottomToolbar_swipeDownWithUpwardJitter_hidesBrowserControls() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent jitterEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 98, 0);

        // The gesture listener approves the swipe from its total displacement (downward), but the
        // first delta it reports can point the other way.
        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        // dy = -2 (upward jitter)
        swipeHandler.onSwipeUpdated(jitterEvent, 0, -2, 0, -2);

        verify(mConstraintsHelper).update(BrowserControlsState.HIDDEN, true);
        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_bottomToolbar_swipeUpWithDownwardJitter_showsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent jitterEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 202, 0);

        // The gesture listener approves the swipe from its total displacement (upward), but the
        // first delta it reports can point the other way.
        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        // dy = 2 (downward jitter)
        swipeHandler.onSwipeUpdated(jitterEvent, 0, 2, 0, 2);

        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_topToolbar_noCurrentTab_onlySwipeDownIsEnabled() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);
        when(mTabModelSelector.getCurrentTab()).thenReturn(null);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);

        // There is nothing to hide without a current tab, but the Hub swipe stays enabled.
        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.UP, event));
        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, event));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_swipeToHideControls_doesNotRequireTabSwitcherSupport() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ false);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);

        Assert.assertTrue(swipeHandler.isSwipeEnabled(ScrollDirection.UP, event));
        Assert.assertFalse(swipeHandler.isSwipeEnabled(ScrollDirection.DOWN, event));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_flagEnabled_horizontalSwipe_startsAndUpdatesSwipeLayout() {
        DeviceInfo.setIsDesktopForTesting(false);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        doNothing().when(layoutManagerChrome).startShowing(eq(mToolbarSwipeLayout), anyBoolean());
        when(mToolbarSwipeLayout.isActive()).thenReturn(true);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 200, 100, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        swipeHandler.onSwipeStarted(ScrollDirection.LEFT, startEvent);
        // dx = -100 (leftward swipe)
        swipeHandler.onSwipeUpdated(moveEvent, -100, 0, -100, 0);
        swipeHandler.onSwipeUpdated(moveEvent, -100, 0, -100, 0);

        verify(layoutManagerChrome).startShowing(mToolbarSwipeLayout, true);
        verify(mToolbarSwipeLayout)
                .swipeStarted(anyLong(), eq(ScrollDirection.LEFT), anyFloat(), anyFloat());
        // The swipe layout is active, so it gets the update that follows the one that started it.
        verify(mToolbarSwipeLayout)
                .swipeUpdated(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void
            testToolbarSwipe_approvedHorizontalSwipe_withVerticalJitter_startsAndUpdatesSwipeLayout() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        doNothing().when(layoutManagerChrome).startShowing(eq(mToolbarSwipeLayout), anyBoolean());
        when(mToolbarSwipeLayout.isActive()).thenReturn(true);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 200, 100, 0);
        MotionEvent jitterEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 200, 98, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        // An approved horizontal swipe (leftward) has vertical jitter on its first move delta (dy =
        // -2, upward).
        swipeHandler.onSwipeStarted(ScrollDirection.LEFT, startEvent);
        swipeHandler.onSwipeUpdated(jitterEvent, 0, -2, 0, -2);

        verify(layoutManagerChrome).startShowing(eq(mToolbarSwipeLayout), eq(true));
        verify(mToolbarSwipeLayout)
                .swipeStarted(anyLong(), eq(ScrollDirection.LEFT), anyFloat(), anyFloat());
        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());

        // Subsequent move event forwards to active toolbar swipe layout.
        swipeHandler.onSwipeUpdated(moveEvent, -100, 0, -100, 0);
        verify(mToolbarSwipeLayout)
                .swipeUpdated(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void
            testToolbarSwipe_approvedVerticalSwipe_withHorizontalJitter_doesNotStartToolbarSwipeLayout() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 200, 0);
        MotionEvent jitterEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 102, 200, 0);
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 100, 100, 0);

        // An approved vertical swipe (upward to hide controls) has horizontal jitter on its first
        // move delta (dx = 2, rightward).
        swipeHandler.onSwipeStarted(ScrollDirection.UP, startEvent);
        swipeHandler.onSwipeUpdated(jitterEvent, 2, 0, 2, 0);

        verify(layoutManagerChrome, never()).startShowing(eq(mToolbarSwipeLayout), anyBoolean());
        verify(mToolbarSwipeLayout, never())
                .swipeStarted(anyLong(), anyInt(), anyFloat(), anyFloat());
        verify(layoutManagerChrome, never()).showLayout(eq(LayoutType.HUB), anyBoolean());
        verify(mConstraintsHelper).update(BrowserControlsState.HIDDEN, true);

        // Subsequent move event should not update mToolbarSwipeLayout because it is not active.
        swipeHandler.onSwipeUpdated(moveEvent, 0, -100, 0, -100);
        verify(mToolbarSwipeLayout, never())
                .swipeUpdated(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EXTRA_HIDE_GESTURES)
    public void testToolbarSwipe_topToolbar_swipeDownWithHorizontalJitter_showsHub() {
        DeviceInfo.setIsDesktopForTesting(false);
        setToolbarPosition(/* onTop= */ true);
        LayoutManagerChrome layoutManagerChrome = createLayoutManagerChromeSpy();
        layoutManagerChrome.mStaticLayout = mStaticLayout;
        layoutManagerChrome.mToolbarSwipeLayout = mToolbarSwipeLayout;
        when(layoutManagerChrome.getActiveLayout()).thenReturn(mStaticLayout);
        setupTabForSwipeToHide(layoutManagerChrome);

        SwipeHandler swipeHandler =
                layoutManagerChrome.createToolbarSwipeHandler(
                        /* supportsSwipeToShowTabSwitcher= */ true);
        MotionEvent startEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        MotionEvent jitterEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 102, 100, 0);

        // An approved vertical swipe (downward to show Hub) has horizontal jitter on its first move
        // delta (dx = 2, rightward).
        swipeHandler.onSwipeStarted(ScrollDirection.DOWN, startEvent);
        swipeHandler.onSwipeUpdated(jitterEvent, 2, 0, 2, 0);

        verify(layoutManagerChrome, never()).startShowing(eq(mToolbarSwipeLayout), anyBoolean());
        verify(mToolbarSwipeLayout, never())
                .swipeStarted(anyLong(), anyInt(), anyFloat(), anyFloat());
        verify(layoutManagerChrome).showLayout(eq(LayoutType.HUB), eq(true));
        verify(mConstraintsHelper, never()).update(eq(BrowserControlsState.HIDDEN), anyBoolean());
    }

    private void setToolbarPosition(boolean onTop) {
        if (ChromeFeatureList.sBrowserControlsExtraHideGestures.isEnabled()) {
            when(mBrowserControlsManager.getControlsPosition())
                    .thenReturn(onTop ? ControlsPosition.TOP : ControlsPosition.BOTTOM);
            // Unlocked controls: they can scroll off screen.
            when(mBrowserControlsManager.getTopControlsHeight()).thenReturn(100);
            when(mBrowserControlsManager.getBottomControlsHeight()).thenReturn(100);
        }
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                        onTop
                                ? ToolbarPositionAndSource.TOP_SETTINGS
                                : ToolbarPositionAndSource.BOTTOM_SETTINGS);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
    }

    private void setupTabForSwipeToHide(LayoutManagerChrome layoutManagerChrome) {
        doReturn(mTabModelSelector).when(layoutManagerChrome).getTabModelSelector();
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        UserDataHost userDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(userDataHost);
        TabBrowserControlsConstraintsHelper.setForTesting(mTab, mConstraintsHelper);
    }

    private LayoutManagerChrome createLayoutManagerChromeSpy() {
        LayoutManagerChrome layoutManagerChrome =
                spy(
                        new LayoutManagerChrome(
                                mHost,
                                mContentContainer,
                                mTabSwitcherSupplier,
                                mTabModelSelectorSupplier,
                                mTabContentManagerSupplier,
                                mToolbarThemeColorProvider,
                                mHubLayoutDependencyHolder));
        doNothing().when(layoutManagerChrome).showLayout(eq(LayoutType.HUB), anyBoolean());
        return layoutManagerChrome;
    }
}
