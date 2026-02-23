// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewFinder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Instrumentation tests for the tab strip empty space long press context menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.TAB_STRIP_EMPTY_SPACE_CONTEXT_MENU_ANDROID)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class TabStripContextMenuTest {
    private static final String TEST_URL = "/chrome/test/data/android/google.html";
    private static final String TEST_URL_2 = "/chrome/test/data/android/test.html";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private StripLayoutHelper mStripLayoutHelper;
    private ModalDialogManager mModalDialogManager;
    private WebPageStation mPage;
    private ChromeTabbedActivity mInitialRegularActivity;

    @Before
    public void setUp() {
        mInitialRegularActivity = mActivityTestRule.getActivityTestRule().getActivity();
        mPage = mActivityTestRule.startOnBlankPage();
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        mModalDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
        clearRecentlyClosedEntries();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mStripLayoutHelper.dismissContextMenu());
        mActivityTestRule.getActivityTestRule().setActivity(mInitialRegularActivity);
    }

    @Test
    @LargeTest
    public void testNewTab_Regular() {
        // Start with 1 regular tab and show menu.
        showMenu();

        // Verify and click "New tab".
        onView(withText(R.string.menu_new_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_new_tab)).perform(click());

        // Verify the menu is closed and there are 2 regular tabs.
        onView(withText(R.string.menu_new_tab)).check(doesNotExist());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityTestRule.tabsCount(false), Matchers.is(2));
                });
    }

    @Test
    @LargeTest
    public void testNewTab_Incognito() {
        // Start with 1 incognito tab and show menu.
        IncognitoNewTabPageStation incognitoNtp = mPage.openNewIncognitoTabOrWindowFast();
        ChromeTabbedActivity cta = incognitoNtp.getActivity();
        if (cta.isIncognitoWindow()) {
            mActivityTestRule
                    .getActivityTestRule()
                    .setActivity(
                            (ChromeTabbedActivity)
                                    ApplicationStatus.getLastTrackedFocusedActivity());
        }
        showMenu();

        // Verify and click "New tab".
        onView(withText(R.string.menu_new_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_new_tab)).perform(click());

        // Verify the menu is closed and there are 2 incognito tabs.
        onView(withText(R.string.menu_new_tab)).check(doesNotExist());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityTestRule.tabsCount(true), Matchers.is(2));
                });
    }

    @Test
    @LargeTest
    public void testReopenClosedEntry_Tab() {
        // Start with 2 regular tabs.
        mPage.openFakeLinkToWebPage(TEST_URL);

        // Close a web page.
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        // Show menu.
        showMenu();

        // Verify and click "Reopen closed tab".
        onView(withText(R.string.menu_reopen_closed_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_reopen_closed_tab)).perform(click());

        // Verify the menu is closed and the web page is restored.
        onView(withText(R.string.menu_reopen_closed_tab)).check(doesNotExist());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityTestRule.tabsCount(false), Matchers.is(2));
                    Criteria.checkThat(
                            ChromeTabUtils.getCurrentTabUrlOnUiThread(
                                    mActivityTestRule.getActivity()),
                            Matchers.is("file://" + TEST_URL));
                });
    }

    @Test
    @LargeTest
    public void testReopenClosedEntry_Bulk() {
        // Start with 3 regular tabs.
        WebPageStation webPage = mPage.openFakeLinkToWebPage(TEST_URL);
        webPage.openFakeLinkToWebPage(TEST_URL_2);

        // Close all tabs.
        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().getTabModelSelectorSupplier());

        // Open a regular tab.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getTotalTabCount(),
                            Matchers.is(0));
                });
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityTestRule.tabsCount(false), Matchers.is(1));
                });

        // Show menu.
        showMenu();

        // Verify and click "Reopen closed tabs".
        onView(withText(R.string.menu_reopen_closed_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_reopen_closed_tabs)).perform(click());

        // Verify the menu is closed and the tabs are restored.
        onView(withText(R.string.menu_reopen_closed_tabs)).check(doesNotExist());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityTestRule.tabsCount(false), Matchers.is(4));

                    TabModel tabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Tab tabAtIndex2 = tabModel.getTabAt(2);
                    Tab tabAtIndex3 = tabModel.getTabAt(3);

                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(tabAtIndex2),
                            Matchers.is("file://" + TEST_URL));
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(tabAtIndex3),
                            Matchers.is("file://" + TEST_URL_2));
                });
    }

    @Test
    @LargeTest
    public void testReopenClosedEntry_Group() {
        // Start with 3 regular tabs.
        WebPageStation webPage = mPage.openFakeLinkToWebPage(TEST_URL);
        webPage.openNewTabFast();

        // Group the first 2 tabs.
        TabStripTestUtils.createTabGroup(mActivityTestRule.getActivity(), false, 0, 1);

        // Delete the group.
        deleteTabGroup();

        // Show menu.
        showMenu();

        // Verify and click "Reopen closed group".
        onView(withText(R.string.menu_reopen_closed_group)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_reopen_closed_group)).perform(click());

        // Verify the menu is closed and the group is restored.
        onView(withText(R.string.menu_reopen_closed_group)).check(doesNotExist());
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel model =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                    Criteria.checkThat(model.getCount(), Matchers.is(3));

                    TabGroupModelFilter filter =
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getTabGroupModelFilter(false);
                    Criteria.checkThat(
                            filter.isTabInTabGroup(model.getTabAt(0)), Matchers.is(false));
                    Criteria.checkThat(
                            filter.isTabInTabGroup(model.getTabAt(1)), Matchers.is(true));
                    Criteria.checkThat(
                            filter.isTabInTabGroup(model.getTabAt(2)), Matchers.is(true));
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(model.getTabAt(2)),
                            Matchers.is("file://" + TEST_URL));
                });
    }

    @Test
    @LargeTest
    public void testReopenClosedTab_NotShownWithNoClosedTabs() {
        // Start with 1 regular tab and show menu.
        showMenu();

        // Verify "Reopen closed tab" is NOT displayed.
        onView(withText(R.string.menu_reopen_closed_tab)).check(doesNotExist());
        onView(withText(R.string.menu_reopen_closed_tabs)).check(doesNotExist());
        onView(withText(R.string.menu_reopen_closed_group)).check(doesNotExist());
    }

    @Test
    @LargeTest
    public void testBookmarkAllTabs_ShownWithMultipleTabs() {
        // Start with 2 regular tabs and show menu.
        mPage.openNewTabFast();
        showMenu();

        // Verify and click "Bookmark all tabs".
        onView(withText(R.string.menu_bookmark_all_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_bookmark_all_tabs)).perform(click());

        // Verify the menu is closed and the "Bookmarked" snackbar shows.
        onView(withText(R.string.menu_bookmark_all_tabs)).check(doesNotExist());
        onViewWaiting(withText("Bookmarked")).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testBookmarkAllTabs_NotShownWithSingleTab() {
        // Start with 1 regular tab and show menu.
        showMenu();

        // Verify "Bookmark all tabs" is NOT displayed.
        onView(withText(R.string.menu_bookmark_all_tabs)).check(doesNotExist());
    }

    private void showMenu() {
        // Calculate a position on the empty space of the tab strip (right of the last tab).
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsToRender();
        StripLayoutTab lastTab = tabs[tabs.length - 1];
        // Click 50dp to the right of the last tab to ensure we hit empty space.
        StripLayoutHelperManager manager =
                mActivityTestRule.getActivity().getLayoutManager().getStripLayoutHelperManager();
        float x = lastTab.getDrawX() + lastTab.getWidth() + 50f;
        float y = manager.getHeight() / 2;

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            manager.simulateLongPress(x, y);
                        });
        onViewWaiting(allOf(withId(R.id.tab_group_action_menu_list), isDisplayed()));
    }

    private void clearRecentlyClosedEntries() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelector selector =
                            mActivityTestRule.getActivity().getTabModelSelector();
                    Profile profile = selector.getModel(false).getProfile();
                    RecentlyClosedBridge bridge = new RecentlyClosedBridge(profile, selector);
                    bridge.clearRecentlyClosedEntries();
                    bridge.destroy();
                });
    }

    private void deleteTabGroup() {
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(
                "First view should be a group title.", views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle stripLayoutGroupTitle = ((StripLayoutGroupTitle) views[0]);
        float x = stripLayoutGroupTitle.getPaddedX();
        float y = stripLayoutGroupTitle.getPaddedY();

        // Long press tab group title to open group context menu.
        StripLayoutHelperManager manager =
                mActivityTestRule.getActivity().getLayoutManager().getStripLayoutHelperManager();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> manager.simulateLongPress(x, y));
        onViewWaiting(allOf(withId(R.id.tab_group_action_menu_list), isDisplayed()));

        // Verify and Click "Delete group".
        ViewFinder.waitForView(withText(R.string.tab_grid_dialog_toolbar_delete_group))
                .onView()
                .perform(click());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true));
                });
        ViewFinder.waitForView(withText(R.string.delete_tab_group_action))
                .onView()
                .perform(click());

        // Verify tab group was deleted.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabGroupModelFilter filter =
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelector()
                                    .getTabGroupModelFilter(false);
                    Criteria.checkThat(filter.getTabGroupCount(), Matchers.is(0));
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(false));
                });
    }
}
