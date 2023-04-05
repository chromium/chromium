// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;

import android.content.res.Configuration;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ListView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.quick_delete.QuickDeleteMetricsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils.UseDesktopUserAgentCaller;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests tabbed mode app menu popup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabbedAppMenuTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_APP_MENU)
                    .build();

    private static final String TEST_URL = UrlUtils.encodeHtmlDataUri("<html>foo</html>");
    private static final String TEST_URL2 = UrlUtils.encodeHtmlDataUri("<html>bar</html>");

    private AppMenuHandler mAppMenuHandler;

    int mLastSelectedItemId = -1;
    private Callback<Integer> mItemSelectedCallback = (itemId) -> mLastSelectedItemId = itemId;

    @Before
    public void setUp() {
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);

        // We need list selection; ensure we are not in touch mode.
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);

        CompositorAnimationHandler.setTestingMode(true);

        mActivityTestRule.startMainActivityWithURL(TEST_URL);

        AppMenuTestSupport.overrideOnOptionItemSelectedListener(
                mActivityTestRule.getAppMenuCoordinator(), mItemSelectedCallback);
        mAppMenuHandler = mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler();

        showAppMenuAndAssertMenuShown();

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> getListView().setSelection(0));
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getCurrentFocusedRow(), Matchers.is(0)));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());

        CompositorAnimationHandler.setTestingMode(false);
        ShoppingFeatures.setShoppingListEligibleForTesting(null);
    }

    /**
     * Verify opening a new tab from the menu.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testMenuNewTab() {
        final int tabCountBefore = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mActivityTestRule.getActivity());
        final int tabCountAfter = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        Assert.assertTrue("Expected: " + (tabCountBefore + 1) + " Got: " + tabCountAfter,
                tabCountBefore + 1 == tabCountAfter);
    }

    /**
     * Test bounds when accessing the menu through the keyboard.
     * Make sure that the menu stays open when trying to move past the first and last items.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuBoundaries() {
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
        moveToBoundary(true, true);
        assertEquals(0, getCurrentFocusedRow());
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
    }

    /**
     * Test that typing ENTER immediately opening the menu works.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnOpen() {
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the top item doesn't crash Chrome.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastTopItem() {
        moveToBoundary(true, true);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER past the bottom item doesn't crash Chrome.
     * Catches regressions for http://crbug.com/181067
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardEnterAfterMovePastBottomItem() {
        moveToBoundary(false, true);
        assertEquals(getCount() - 1, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item actually triggers the top item.
     * Catches regressions for https://crbug.com/191239 for shrunken menus.
     */
    @SmallTest
    @Feature({"Browser", "Main"})
    @Test
    public void testKeyboardMenuEnterOnTopItemLandscape() {
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    /**
     * Test that hitting ENTER on the top item doesn't crash Chrome.
     */
    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testKeyboardMenuEnterOnTopItemPortrait() {
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        showAppMenuAndAssertMenuShown();
        moveToBoundary(true, false);
        assertEquals(0, getCurrentFocusedRow());
        hitEnterAndAssertAppMenuDismissed();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testHideMenuOnToggleOverview() throws TimeoutException {
        // App menu is shown during setup.
        Assert.assertTrue("App menu should be showing.", mAppMenuHandler.isAppMenuShowing());
        Assert.assertFalse("Overview shouldn't be showing.",
                mActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                        LayoutType.TAB_SWITCHER));

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        Assert.assertTrue("Overview should be showing.",
                mActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                        LayoutType.TAB_SWITCHER));
        Assert.assertFalse("App menu shouldn't be showing.", mAppMenuHandler.isAppMenuShowing());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("App menu should be allowed to show.",
                    AppMenuTestSupport.shouldShowAppMenu(
                            mActivityTestRule.getAppMenuCoordinator()));
        });
        showAppMenuAndAssertMenuShown();

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING, false);
        Assert.assertFalse("Overview shouldn't be showing.",
                mActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                        LayoutType.TAB_SWITCHER));
        CriteriaHelper.pollUiThread(
                () -> !mAppMenuHandler.isAppMenuShowing(), "App menu shouldn't be showing.");
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "Bookmark", "RenderTest"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testBookmarkMenuItem() throws IOException {
        PropertyModel bookmarkStarPropertyModel = AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.bookmark_this_page_id);
        Assert.assertFalse("Bookmark item should not be checked.",
                bookmarkStarPropertyModel.get(AppMenuItemProperties.CHECKED));
        assertEquals("Incorrect content description.",
                mActivityTestRule.getActivity().getString(R.string.menu_bookmark),
                bookmarkStarPropertyModel.get(AppMenuItemProperties.TITLE_CONDENSED));
        mRenderTestRule.render(getListView().getChildAt(0), "rounded_corner_icon_row");

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        AppMenuPropertiesDelegateImpl.setPageBookmarkedForTesting(true);
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        bookmarkStarPropertyModel = AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.bookmark_this_page_id);
        Assert.assertTrue("Bookmark item should be checked.",
                bookmarkStarPropertyModel.get(AppMenuItemProperties.CHECKED));
        assertEquals("Incorrect content description for bookmarked page.",
                mActivityTestRule.getActivity().getString(R.string.edit_bookmark),
                bookmarkStarPropertyModel.get(AppMenuItemProperties.TITLE_CONDENSED));
        mRenderTestRule.render(
                getListView().getChildAt(0), "rounded_corner_icon_row_page_bookmarked");

        AppMenuPropertiesDelegateImpl.setPageBookmarkedForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "RenderTest"})
    public void testDividerLineMenuItem() throws IOException {
        int firstDividerLineIndex = AppMenuTestSupport.findIndexOfMenuItemById(
                mActivityTestRule.getAppMenuCoordinator(), R.id.divider_line_id);
        Assert.assertTrue("No divider line found.", firstDividerLineIndex != -1);
        mRenderTestRule.render(getListView().getChildAt(firstDividerLineIndex), "divider_line");
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "RenderTest"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures(ChromeFeatureList.APP_MENU_MOBILE_SITE_OPTION)
    public void testRequestDesktopSiteMenuItem() throws IOException {
        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        boolean isRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        Assert.assertFalse("Default to request mobile site.", isRequestDesktopSite);

        int requestDesktopSiteIndex = AppMenuTestSupport.findIndexOfMenuItemById(
                mActivityTestRule.getAppMenuCoordinator(), R.id.request_desktop_site_row_menu_id);
        Assert.assertNotEquals("No request desktop site row found.", -1, requestDesktopSiteIndex);

        Callable<Boolean> isVisible = () -> {
            int visibleStart = getListView().getFirstVisiblePosition();
            int visibleEnd = visibleStart + getListView().getChildCount() - 1;
            return requestDesktopSiteIndex >= visibleStart && requestDesktopSiteIndex <= visibleEnd;
        };
        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);
        if (!TestThreadUtils.runOnUiThreadBlockingNoException(isVisible)) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> getListView().smoothScrollToPosition(requestDesktopSiteIndex));
            CriteriaHelper.pollUiThread(isVisible);
        }
        mRenderTestRule.render(getListView().getChildAt(requestDesktopSiteIndex
                                       - getListView().getFirstVisiblePosition()),
                "request_desktop_site");

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                                true /* useDesktop */, true /* reloadOnChange */,
                                UseDesktopUserAgentCaller.OTHER));
        ChromeTabUtils.waitForTabPageLoaded(tab, TEST_URL);
        isRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        Assert.assertTrue("Should request desktop site.", isRequestDesktopSite);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);
        if (!TestThreadUtils.runOnUiThreadBlockingNoException(isVisible)) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> getListView().smoothScrollToPosition(requestDesktopSiteIndex));
            CriteriaHelper.pollUiThread(isVisible);
        }
        mRenderTestRule.render(getListView().getChildAt(requestDesktopSiteIndex
                                       - getListView().getFirstVisiblePosition()),
                "request_mobile_site");
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "RenderTest"})
    @DisableFeatures(ChromeFeatureList.APP_MENU_MOBILE_SITE_OPTION)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testRequestDesktopSiteMenuItem_checkbox() throws IOException {
        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        boolean isRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        Assert.assertFalse("Default to request mobile site.", isRequestDesktopSite);

        int requestDesktopSiteIndex = AppMenuTestSupport.findIndexOfMenuItemById(
                mActivityTestRule.getAppMenuCoordinator(), R.id.request_desktop_site_row_menu_id);
        Assert.assertNotEquals("No request desktop site row found.", -1, requestDesktopSiteIndex);

        Callable<Boolean> isVisible = () -> {
            int visibleStart = getListView().getFirstVisiblePosition();
            int visibleEnd = visibleStart + getListView().getChildCount() - 1;
            return requestDesktopSiteIndex >= visibleStart && requestDesktopSiteIndex <= visibleEnd;
        };
        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);
        if (!TestThreadUtils.runOnUiThreadBlockingNoException(isVisible)) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> getListView().smoothScrollToPosition(requestDesktopSiteIndex));
            CriteriaHelper.pollUiThread(isVisible);
        }
        mRenderTestRule.render(getListView().getChildAt(requestDesktopSiteIndex
                                       - getListView().getFirstVisiblePosition()),
                "request_desktop_site_uncheck");

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                                true /* useDesktop */, true /* reloadOnChange */,
                                UseDesktopUserAgentCaller.OTHER));
        ChromeTabUtils.waitForTabPageLoaded(tab, TEST_URL);
        isRequestDesktopSite =
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        Assert.assertTrue("Should request desktop site.", isRequestDesktopSite);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(() -> getListView().getChildAt(0) != null);
        if (!TestThreadUtils.runOnUiThreadBlockingNoException(isVisible)) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> getListView().smoothScrollToPosition(requestDesktopSiteIndex));
            CriteriaHelper.pollUiThread(isVisible);
        }
        mRenderTestRule.render(getListView().getChildAt(requestDesktopSiteIndex
                                       - getListView().getFirstVisiblePosition()),
                "request_mobile_site_check");
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
    public void testAddBookmarkMenuItem() throws IOException {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        int addBookmark = AppMenuTestSupport.findIndexOfMenuItemById(
                mActivityTestRule.getAppMenuCoordinator(), R.id.add_bookmark_menu_id);
        Assert.assertNotEquals("No add bookmark found.", -1, addBookmark);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "RenderTest"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
    public void testEditBookmarkMenuItem() throws IOException {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        AppMenuPropertiesDelegateImpl.setPageBookmarkedForTesting(true);
        showAppMenuAndAssertMenuShown();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        PropertyModel bookmarkStarPropertyModel = AppMenuTestSupport.getMenuItemPropertyModel(
                mActivityTestRule.getAppMenuCoordinator(), R.id.edit_bookmark_menu_id);
        assertEquals("Add Bookmark item should be tint blue.",
                R.color.default_icon_color_accent1_tint_list,
                bookmarkStarPropertyModel.get(AppMenuItemProperties.ICON_COLOR_RES));

        int editBookmarkMenuItemIndex = AppMenuTestSupport.findIndexOfMenuItemById(
                mActivityTestRule.getAppMenuCoordinator(), R.id.edit_bookmark_menu_id);
        Assert.assertNotEquals("No add bookmark menu item found.", -1, editBookmarkMenuItemIndex);
        mRenderTestRule.render(
                getListView().getChildAt(editBookmarkMenuItemIndex), "edit_bookmark_list_item");

        AppMenuPropertiesDelegateImpl.setPageBookmarkedForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main", "QuickDelete", "RenderTest"})
    @EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testQuickDeleteMenu_Shown() throws IOException {
        showAppMenuAndAssertMenuShown();
        int quickDeletePosition = AppMenuTestSupport.findIndexOfMenuItemById(
                mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id);
        mRenderTestRule.render(getListView().getChildAt(quickDeletePosition), "quick_delete");
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main", "QuickDelete"})
    @EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testQuickDeleteMenu_entryFromMenuItemHistogram() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Privacy.QuickDelete",
                                QuickDeleteMetricsDelegate.QuickDeleteAction.MENU_ITEM_CLICKED);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.quick_delete_menu_id);

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main", "QuickDelete"})
    @EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testQuickDeleteMenu_NotShownInIncognito() {
        // Hide first any shown app menu as it can interfere with this test.
        hitEnterAndAssertAppMenuDismissed();

        mActivityTestRule.newIncognitoTabFromMenu();
        showAppMenuAndAssertMenuShown();
        assertEquals(-1,
                AppMenuTestSupport.findIndexOfMenuItemById(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id));
    }

    @Test
    @LargeTest
    @Feature({"Browser", "Main", "QuickDelete"})
    @DisableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
    public void testQuickDeleteMenu_NotShown() throws IOException {
        showAppMenuAndAssertMenuShown();
        assertEquals(-1,
                AppMenuTestSupport.findIndexOfMenuItemById(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id));
    }

    private void showAppMenuAndAssertMenuShown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
            Assert.assertTrue(mAppMenuHandler.isAppMenuShowing());
        });
    }

    private void hitEnterAndAssertAppMenuDismissed() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        pressKey(KeyEvent.KEYCODE_ENTER);
        CriteriaHelper.pollInstrumentationThread(
                () -> !mAppMenuHandler.isAppMenuShowing(), "AppMenu did not dismiss");
    }

    private void moveToBoundary(boolean towardsTop, boolean movePast) {
        // Move to the boundary.
        final int end = towardsTop ? 0 : getCount() - 1;
        int increment = towardsTop ? -1 : 1;
        for (int index = getCurrentFocusedRow(); index != end; index += increment) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            final int expectedPosition = index + increment;
            CriteriaHelper.pollInstrumentationThread(() -> {
                Criteria.checkThat(getCurrentFocusedRow(), Matchers.is(expectedPosition));
            });
        }

        // Try moving past it by one.
        if (movePast) {
            pressKey(towardsTop ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
            CriteriaHelper.pollInstrumentationThread(
                    () -> Criteria.checkThat(getCurrentFocusedRow(), Matchers.is(end)));
        }

        // The menu should stay open.
        Assert.assertTrue(mAppMenuHandler.isAppMenuShowing());
    }

    private void pressKey(final int keycode) {
        final View view = getListView();
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keycode));
            view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keycode));
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private int getCurrentFocusedRow() {
        ListView listView = getListView();
        if (listView == null) return ListView.INVALID_POSITION;
        return listView.getSelectedItemPosition();
    }

    private int getCount() {
        ListView listView = getListView();
        if (listView == null) return 0;
        return listView.getCount();
    }

    private ListView getListView() {
        return AppMenuTestSupport.getListView(mActivityTestRule.getAppMenuCoordinator());
    }
}
