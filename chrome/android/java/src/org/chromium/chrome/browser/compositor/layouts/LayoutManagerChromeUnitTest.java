// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.mockito.ArgumentMatchers.anyBoolean;
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

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link LayoutManagerChrome}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LayoutManagerChromeUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private @Mock LayoutManagerHost mHost;
    private @Mock ViewGroup mContentContainer;
    private @Mock HubLayoutDependencyHolder mHubLayoutDependencyHolder;
    private @Mock ToolbarSwipeLayout mToolbarSwipeLayout;
    private @Mock Tab mTab;
    private @Mock StaticLayout mStaticLayout;
    private @Mock HubLayout mHubLayout;
    private @Mock TabModelSelector mTabModelSelector;

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
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (TestActivity activity) -> when(mHost.getContext()).thenReturn(activity));
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
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
