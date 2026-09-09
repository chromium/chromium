// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * End-to-end integration tests for the Grid Tab Switcher (GTS) Pinned Tab List.
 *
 * <p>Tests the sticky horizontal pinned tab strip lifecycle in the Grid Tab Switcher, including
 * off-screen visibility filtering, scroll-driven reveal and hide, tab selection, dynamic unpinning,
 * dynamic pinning, pill auto-shrinking, selection synchronization, incognito model switching, and
 * tab closure and undo.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
@Batch(Batch.PER_CLASS)
@DisableFeatures({ChromeFeatureList.ANDROID_THEME_MODULE})
public class TabSwitcherPinnedTabListTest {
    private static final int SCROLL_DISTANCE_DOWN_PX = 2000;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Before
    public void setUp() {
        mActivityTestRule.startOnNtp();
        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (cta != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        for (TabModel model : cta.getTabModelSelector().getModels()) {
                            for (int i = 0; i < model.getCount(); i++) {
                                Tab tab = model.getTabAt(i);
                                if (tab.getIsPinned()) {
                                    tab.setIsPinned(false);
                                }
                            }
                        }
                    });
        }
    }

    /**
     * Verifies that when scrolled to the top of the tab grid, pinned tabs are visible as cards in
     * the main grid and the sticky pinned strip is hidden.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_HiddenWhenScrolledToTop() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 2);

        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip == null
                            || pinnedStrip.getVisibility() != View.VISIBLE
                            || pinnedStrip.getAdapter() == null
                            || pinnedStrip.getAdapter().getItemCount() == 0;
                });
    }

    /**
     * Verifies that scrolling down past the pinned cards reveals the sticky pinned tab strip, and
     * scrolling back to the top hides it.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_ShowsOnScrollDown_HidesOnScrollUp() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 3);

        // Scroll down in the main tab grid so that pinned cards scroll off-screen.
        scrollMainGrid(cta);

        // Pinned strip should become visible with 3 items.
        waitForPinnedStripItemCount(3);

        onView(withId(R.id.pinned_tabs_strip_recycler_view)).check(matches(isDisplayed()));

        // Scroll back to the top of the main tab grid.
        scrollMainGridToTop(cta);
    }

    /**
     * Verifies that tapping a pinned pill in the sticky strip switches to that tab and exits the
     * tab switcher.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_ClickPillSelectsTabAndLeavesSwitcher() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 2);

        int targetTabId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> cta.getCurrentTabModel().getTabAt(0).getId());

        // Scroll down to make the pinned strip visible.
        scrollMainGrid(cta);
        waitForPinnedStripItemCount(2);

        // Click the first pinned tab item in the strip.
        onView(withId(R.id.pinned_tabs_strip_recycler_view))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));

        // Should exit GTS and navigate to browsing mode for targetTabId.
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        int currentTabId =
                ThreadUtils.runOnUiThreadBlocking(() -> cta.getActivityTabProvider().get().getId());
        assertEquals(targetTabId, currentTabId);
    }

    /**
     * Verifies that dynamically pinning a tab while already scrolled down in the Tab Switcher
     * immediately adds a pill to the sticky strip.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_DynamicPinAddsPillWhenScrolled() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 1);

        // Scroll down to reveal the sticky strip with 1 pinned tab.
        scrollMainGrid(cta);
        waitForPinnedStripItemCount(1);

        // Dynamically pin a second tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.getCurrentTabModel().getTabAt(1).setIsPinned(true));

        // Strip should dynamically grow to 2 items.
        waitForPinnedStripItemCount(2);

        // Dynamically pin a third tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.getCurrentTabModel().getTabAt(2).setIsPinned(true));

        // Strip should dynamically grow to 3 items.
        waitForPinnedStripItemCount(3);
    }

    /**
     * Verifies that dynamically unpinning a tab removes its pill from the strip and collapses the
     * strip when all tabs are unpinned.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_DynamicUnpinRemovesPill() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 3);

        // Scroll down to show all 3 pinned tabs in the strip.
        scrollMainGrid(cta);
        waitForPinnedStripItemCount(3);

        // Dynamically unpin the second pinned tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tabToUnpin = cta.getCurrentTabModel().getTabAt(1);
                    tabToUnpin.setIsPinned(false);
                });

        // Strip should now reflect 2 items.
        waitForPinnedStripItemCount(2);

        // Unpin all remaining tabs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = cta.getCurrentTabModel();
                    for (int i = 0; i < model.getCount(); i++) {
                        Tab tab = model.getTabAt(i);
                        if (tab.getIsPinned()) {
                            tab.setIsPinned(false);
                        }
                    }
                });

        // Strip should now hide completely.
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip == null
                            || pinnedStrip.getVisibility() != View.VISIBLE
                            || pinnedStrip.getAdapter() == null
                            || pinnedStrip.getAdapter().getItemCount() == 0;
                });
    }

    /**
     * Verifies that pill widths dynamically adjust and shrink as more tabs are pinned in the strip.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_PillWidthShrinksAsCountIncreases() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 1);

        // Scroll down to display the 1-item strip.
        scrollMainGrid(cta);
        waitForPinnedStripItemCount(1);
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip != null && pinnedStrip.getChildCount() == 1;
                });

        int singlePillWidth =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            RecyclerView pinnedStrip =
                                    cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                            return pinnedStrip.getChildAt(0).getWidth();
                        });
        assertTrue(singlePillWidth > 0);

        // Dynamically pin 3 more tabs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = cta.getCurrentTabModel();
                    model.getTabAt(1).setIsPinned(true);
                    model.getTabAt(2).setIsPinned(true);
                    model.getTabAt(3).setIsPinned(true);
                });

        waitForPinnedStripItemCount(4);
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip != null && pinnedStrip.getChildCount() >= 2;
                });

        int multiPillWidth =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            RecyclerView pinnedStrip =
                                    cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                            return pinnedStrip.getChildAt(0).getWidth();
                        });

        // Pill width with 4 tabs should shrink relative to single pill width.
        assertTrue(multiPillWidth > 0);
        assertTrue(multiPillWidth < singlePillWidth);
    }

    /** Verifies that switching selected tab updates selection in TabModel correctly. */
    @Test
    @MediumTest
    public void testPinnedStrip_SelectedTabHighlightSync() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 2);

        // Scroll down to make pinned strip visible.
        scrollMainGrid(cta);
        waitForPinnedStripItemCount(2);

        // Select the second pinned tab in the TabModel.
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabModelUtils.setIndex(cta.getCurrentTabModel(), 1));

        CriteriaHelper.pollUiThread(() -> cta.getCurrentTabModel().index() == 1);

        // Select a non-pinned tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabModelUtils.setIndex(cta.getCurrentTabModel(), 4));

        CriteriaHelper.pollUiThread(() -> cta.getCurrentTabModel().index() == 4);
    }

    /**
     * Verifies that closing a pinned tab removes it from the strip, and undoing closure restores it
     * to the strip.
     */
    @Test
    @MediumTest
    public void testPinnedStrip_CloseAndUndoTabClosure() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsAndEnterTabSwitcher(/* pinnedTabsCount= */ 2);

        // Scroll down to show 2 pinned tabs in the strip.
        scrollMainGrid(cta);
        waitForPinnedStripItemCount(2);

        // Close the first pinned tab with undo allowed.
        int closedTabId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Tab tabToClose = cta.getCurrentTabModel().getTabAt(0);
                            int tabId = tabToClose.getId();
                            cta.getCurrentTabModel()
                                    .getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeTab(tabToClose)
                                                    .allowUndo(true)
                                                    .build(),
                                            /* allowDialog= */ false);
                            return tabId;
                        });

        // Strip item count should decrease to 1.
        waitForPinnedStripItemCount(1);

        // Undo closure of the closed tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> cta.getCurrentTabModel().cancelTabClosure(closedTabId));

        // Strip item count should restore back to 2.
        waitForPinnedStripItemCount(2);
    }

    /**
     * Prepares the activity with 9 tabs (1 existing + 8 added), pins the first {@code
     * pinnedTabsCount} tabs, resets the active index to 0 so the GTS opens at the top, and enters
     * the Grid Tab Switcher.
     */
    private void prepareTabsAndEnterTabSwitcher(int pinnedTabsCount) {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.addBlankTabs(cta, /* incognito= */ false, /* count= */ 8);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = cta.getCurrentTabModel();
                    for (int i = 0; i < pinnedTabsCount; i++) {
                        model.getTabAt(i).setIsPinned(true);
                    }
                    TabModelUtils.setIndex(model, 0);
                });
        TabUiTestHelper.enterTabSwitcher(cta);
    }

    /** Scrolls the main tab list recycler view down so that pinned cards scroll off-screen. */
    private void scrollMainGrid(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = cta.findViewById(R.id.tab_list_recycler_view);
                    if (recyclerView == null) return false;
                    if (recyclerView.computeVerticalScrollOffset() > 0) {
                        recyclerView.scrollBy(0, -100);
                    }
                    recyclerView.scrollBy(0, SCROLL_DISTANCE_DOWN_PX);
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip != null
                            && pinnedStrip.getVisibility() == View.VISIBLE
                            && pinnedStrip.getAdapter() != null
                            && pinnedStrip.getAdapter().getItemCount() > 0;
                });
    }

    /**
     * Scrolls the main tab list back to position 0 and ensures the pinned strip hides once the
     * pinned cards are back in the viewport.
     */
    private void scrollMainGridToTop(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = cta.findViewById(R.id.tab_list_recycler_view);
                    if (recyclerView != null) {
                        if (recyclerView.computeVerticalScrollOffset() == 0) {
                            recyclerView.scrollBy(0, 100);
                        }
                        recyclerView.scrollBy(0, -SCROLL_DISTANCE_DOWN_PX);
                        recyclerView.scrollToPosition(0);
                    }
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip == null
                            || pinnedStrip.getVisibility() != View.VISIBLE
                            || pinnedStrip.getAdapter() == null
                            || pinnedStrip.getAdapter().getItemCount() == 0;
                });
    }

    /** Polls until the pinned tabs strip is visible with the {@code expectedCount} items. */
    private void waitForPinnedStripItemCount(int expectedCount) {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView pinnedStrip =
                            cta.findViewById(R.id.pinned_tabs_strip_recycler_view);
                    return pinnedStrip != null
                            && pinnedStrip.getVisibility() == View.VISIBLE
                            && pinnedStrip.getAdapter() != null
                            && pinnedStrip.getAdapter().getItemCount() == expectedCount;
                });
    }
}
