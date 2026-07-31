// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabRailLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/**
 * Android Instrumentation Integration tests for Vertical Tabs Left Rail.
 *
 * <p>Tests comprehensive end-to-end user journeys in the Vertical Tabs Left Rail on tablets and
 * large-form factor devices, including normal/incognito switching, group creation/ungrouping, group
 * titles/colors, tab reordering, undo closure, and multi-tab selection workflows.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS + ":enable_by_default/true"})
@Batch(Batch.PER_CLASS)
public class VerticalTabsTest {
    private static final String TEST_GROUP_TITLE = "Vertical Tabs Project";
    private static final int TEST_TABS_TO_MAKE = 5;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Before
    public void setUp() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true);
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = page.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelectorSupplier().get()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel normalModel = selector.getModel(/* incognito= */ false);
                    while (normalModel.getCount() > 1) {
                        normalModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(normalModel.getTabAt(1))
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false);
                    }
                    if (normalModel.getCount() == 1 && normalModel.getTabAt(0).getIsPinned()) {
                        normalModel.unpinTab(normalModel.getTabAt(0).getId());
                    } else if (normalModel.getCount() == 0) {
                        cta.getTabCreator(/* incognito= */ false)
                                .createNewTab(
                                        new LoadUrlParams(
                                                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                                        TabLaunchType.FROM_CHROME_UI,
                                        null,
                                        0);
                    }
                });
    }

    // =========================================================================================
    // Basic Left Rail Visibility & Layout
    // =========================================================================================

    @Test
    @MediumTest
    public void testLeftRailVisibility() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();
        assertNotNull("TabModelSelector should be initialized.", selector);
        assertTrue(
                "Should have at least one tab initially.",
                getTabCountOnUiThread(selector.getCurrentModel()) >= 1);
        onView(withId(R.id.tab_list_recycler_view)).check(matches(isDisplayed()));
        onView(withId(R.id.new_tab_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testToolbarButtonsVisibility() {
        onView(withId(R.id.collapse_button)).check(matches(isDisplayed()));
        onView(withId(R.id.grid_button)).check(matches(isDisplayed()));
        onView(withId(R.id.tab_search_button)).check(matches(isDisplayed()));
        onView(withId(R.id.new_tab_button)).check(matches(isDisplayed()));
    }

    // =========================================================================================
    // Single Tab Lifecycle & UI Interaction (Create, Select, Close, Undo)
    // =========================================================================================

    @Test
    @MediumTest
    public void testCreateNewTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();
        int initialTabCount = getTabCountOnUiThread(selector.getCurrentModel());

        onView(withId(R.id.new_tab_button)).perform(click());

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getCount() == initialTabCount + 1,
                "Tab count should increase by 1 after clicking New Tab button in Left Rail.");
    }

    @Test
    @MediumTest
    public void testSelectTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        int firstTabId = ThreadUtils.runOnUiThreadBlocking(() -> selector.getCurrentTabId());
        Tab secondTab = createTabOnUiThread(cta, /* incognito= */ false);
        assertNotNull("Second tab should be created.", secondTab);

        // createTabOnUiThread inserts secondTab at index 0, moving firstTab to index 1.
        clickTabItemAtPosition(1);

        assertActiveTabId(
                selector,
                firstTabId,
                "Active tab ID should switch back to the first tab after clicking its Left Rail"
                        + " item.");
    }

    @Test
    @MediumTest
    public void testCloseTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        createTabOnUiThread(cta, /* incognito= */ false);
        int initialTabCount = getTabCountOnUiThread(selector.getCurrentModel());

        clickActionButtonAtPosition(1);

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getCount() == initialTabCount - 1,
                "Tab count should decrease by 1 after clicking close button on tab item in Left"
                        + " Rail.");
    }

    @Test
    @MediumTest
    public void testCloseTabAndUndo() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        createTabOnUiThread(cta, /* incognito= */ false);
        int initialTabCount = getTabCountOnUiThread(selector.getCurrentModel());

        clickActionButtonAtPosition(1);

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getCount() == initialTabCount - 1,
                "Tab count should decrease after closing.");

        onView(withId(R.id.snackbar_button)).perform(click());

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getCount() == initialTabCount,
                "Tab count should return to initial after clicking Undo on the UI Snackbar.");
    }

    @Test
    @MediumTest
    public void testMultipleTabsCreationAndSelectionPersistence() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        List<Tab> newTabs = new ArrayList<>();
        for (int i = 0; i < TEST_TABS_TO_MAKE; i++) {
            newTabs.add(createTabOnUiThread(cta, /* incognito= */ false));
        }

        Tab thirdTab = newTabs.get(2);
        int thirdTabId = thirdTab.getId();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = selector.getCurrentModel();
                    model.setIndex(model.indexOf(thirdTab), TabSelectionType.FROM_USER);
                });

        assertActiveTabId(selector, thirdTabId, "Active tab ID should be the third tab.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel()
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(newTabs.get(0)).build(),
                                    /* allowDialog= */ false);
                });

        assertActiveTabId(
                selector,
                thirdTabId,
                "Active tab ID should remain the third tab even after closing another tab.");
    }

    @Test
    @MediumTest
    public void testCloseAllNormalTabs() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        createTabOnUiThread(cta, /* incognito= */ false);
        createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getModel(false)
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs().build(),
                                    /* allowDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getModel(false).getCount() == 0,
                "Normal tab model should be empty after closeAllTabs.");

        // Restore a normal tab so subsequent tests in the batch start with a valid normal tab.
        createTabOnUiThread(cta, /* incognito= */ false);
    }

    // =========================================================================================
    // Tab Groups & Group Header (Create, Ungroup, Title, Color, Collapse/Expand)
    // =========================================================================================

    @Test
    @MediumTest
    public void testTabGroupCreation() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);
        assertNotNull("Tab 1 should exist.", tab1);
        assertNotNull("Tab 2 should exist.", tab2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                });

        assertTabInGroup(selector, tab1, "Tab 1 should be part of a tab group.");
        assertTabInGroup(selector, tab2, "Tab 2 should be part of a tab group.");
    }

    @Test
    @MediumTest
    public void testTabGroupUngroup() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                });

        assertTabInGroup(selector, tab1, "Tab 1 should be part of a group.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel()
                            .getTabUngrouper()
                            .ungroupTabs(
                                    List.of(tab1, tab2),
                                    /* trailing= */ false,
                                    /* allowDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> !selector.getCurrentModel().isTabInTabGroup(tab1),
                "Tab 1 should be ungrouped.");
        CriteriaHelper.pollUiThread(
                () -> !selector.getCurrentModel().isTabInTabGroup(tab2),
                "Tab 2 should be ungrouped.");
    }

    @Test
    @MediumTest
    public void testTabGroupTitleUpdate() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                });

        String testTitle = TEST_GROUP_TITLE;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().setTabGroupTitle(tab1.getTabGroupId(), testTitle);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        testTitle.equals(
                                selector.getCurrentModel().getTabGroupTitle(tab1.getTabGroupId())),
                "Tab group title should match the stored custom title.");
    }

    @Test
    @MediumTest
    public void testTabGroupColorChange() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                });

        @TabGroupColorId int targetColorId = TabGroupColorId.CYAN;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel()
                            .setTabGroupColor(tab1.getTabGroupId(), targetColorId);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        targetColorId
                                == selector.getCurrentModel()
                                        .getTabGroupColor(tab1.getTabGroupId()),
                "Tab group color ID should match the stored color ID.");
    }

    @Test
    @MediumTest
    public void testTabGroupInlineCollapseAndExpand() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                });

        assertTabInGroup(selector, tab1, "Tab 1 should be in a group.");

        clickTabItemAtPosition(0);

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getTabGroupCollapsed(tab1.getTabGroupId()),
                "Tab group should be collapsed after clicking its header in Left Rail.");

        clickTabItemAtPosition(0);

        CriteriaHelper.pollUiThread(
                () -> !selector.getCurrentModel().getTabGroupCollapsed(tab1.getTabGroupId()),
                "Tab group should be expanded after clicking its header in Left Rail again.");
    }

    // =========================================================================================
    // Pinned Tabs Strip & Visibility Lifecycle
    // =========================================================================================

    @Test
    @MediumTest
    public void testPinnedTabsVisibilityLifecycle() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);

        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        pinTabAndWait(selector, tab1, 1);
        waitForViewVisibility(cta, R.id.pinned_tabs_recycler_view, View.VISIBLE);
        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().unpinTab(tab1.getId());
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 0,
                "Model should have 0 pinned tabs after unpinning.");
        waitForViewVisibility(cta, R.id.pinned_tabs_recycler_view, View.GONE);
        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @MediumTest
    public void testSelectPinnedTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        int firstTabId = ThreadUtils.runOnUiThreadBlocking(() -> selector.getCurrentTabId());
        createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().pinTab(firstTabId, /* showUngroupDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 1,
                "Model should have 1 pinned tab.");
        waitForViewVisibility(cta, R.id.pinned_tabs_recycler_view, View.VISIBLE);
        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        // Currently secondTab is active. Click the pinned tab (firstTab) in the Pinned Strip.
        clickPinnedTabItemAtPosition(0);

        assertActiveTabId(
                selector,
                firstTabId,
                "Active tab ID should switch to the pinned tab after clicking its pill in Left"
                        + " Rail.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().unpinTab(firstTabId);
                });
    }

    @Test
    @MediumTest
    public void testMultiplePinnedTabsSelection() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        int tab1Id = ThreadUtils.runOnUiThreadBlocking(() -> selector.getCurrentTabId());
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);
        int tab2Id = tab2.getId();
        createTabOnUiThread(cta, /* incognito= */ false); // Tab3 remains unpinned as active tab.

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().pinTab(tab1Id, /* showUngroupDialog= */ false);
                    selector.getCurrentModel().pinTab(tab2Id, /* showUngroupDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 2,
                "Model should have 2 pinned tabs.");
        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        clickPinnedTabItemAtPosition(0);
        assertActiveTabId(
                selector, tab1Id, "Active tab ID should be tab1 after clicking first pinned pill.");

        clickPinnedTabItemAtPosition(1);
        assertActiveTabId(
                selector,
                tab2Id,
                "Active tab ID should be tab2 after clicking second pinned pill.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().unpinTab(tab1Id);
                    selector.getCurrentModel().unpinTab(tab2Id);
                });
    }

    @Test
    @MediumTest
    public void testClosePinnedTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().pinTab(tab1.getId(), /* showUngroupDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 1,
                "Model should have 1 pinned tab.");
        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(VISIBLE)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel()
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab1).build(),
                                    /* allowDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 0,
                "Model should have 0 pinned tabs after closing the pinned tab.");
        waitForViewVisibility(cta, R.id.pinned_tabs_recycler_view, View.GONE);
        onView(withId(R.id.pinned_tabs_recycler_view))
                .check(matches(withEffectiveVisibility(GONE)));

        // Restore a normal tab so subsequent tests in the batch start with a valid normal tab.
        createTabOnUiThread(cta, /* incognito= */ false);
    }

    @Test
    @MediumTest
    public void testUnpinTabReturnsToMainList() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        int tab1Id = tab1.getId();

        pinTabAndWait(selector, tab1, 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().unpinTab(tab1Id);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 0,
                "Model should have 0 pinned tabs after unpinning.");
        CriteriaHelper.pollUiThread(() -> !tab1.getIsPinned(), "tab1 should no longer be pinned.");
        onView(withId(R.id.tab_list_recycler_view)).check(matches(isDisplayed()));
    }

    // =========================================================================================
    // Rail Collapse / Expand Button
    // =========================================================================================

    @Test
    @MediumTest
    public void testRailCollapseAndExpand() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // 1. Verify the collapse button is displayed and initially in the EXPANDED state.
        onView(withId(R.id.collapse_button))
                .check(matches(isDisplayed()))
                .check(
                        matches(
                                withContentDescription(
                                        R.string.accessibility_collapse_vertical_tabs)));

        // 2. Click the collapse button to switch to the COLLAPSED state.
        onView(withId(R.id.collapse_button)).perform(click());

        // 3. Verify the button's content description changes to EXPAND.
        onView(withId(R.id.collapse_button))
                .check(
                        matches(
                                withContentDescription(
                                        R.string.accessibility_expand_vertical_tabs)));

        // 4. Click again to expand the rail back to the EXPANDED state.
        onView(withId(R.id.collapse_button)).perform(click());

        // 5. Verify the button returns to the COLLAPSE content description.
        onView(withId(R.id.collapse_button))
                .check(
                        matches(
                                withContentDescription(
                                        R.string.accessibility_collapse_vertical_tabs)));
    }

    // =========================================================================================
    // Left Rail Toolbar Action Buttons
    // =========================================================================================

    @Test
    @MediumTest
    public void testClickGridButtonOpensHub() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        assertFalse(
                "Hub layout should not be visible initially.",
                cta.getLayoutManager().isLayoutVisible(LayoutType.HUB));

        onView(withId(R.id.grid_button)).perform(click());

        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.HUB);

        TabUiTestHelper.leaveTabSwitcher(cta);
    }

    // =========================================================================================
    // Desktop Windowing Mode & App Header Spacer
    // =========================================================================================

    @Test
    @MediumTest
    public void testDesktopWindowSpacerVisibility() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // 1. Capture initial spacer visibility without assuming standard tablet vs. desktop
        // windowing mode.
        boolean initialSpacerVisible =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            View spacer = cta.findViewById(R.id.desktop_window_spacer);
                            return spacer != null && spacer.getVisibility() == View.VISIBLE;
                        });

        // 2. Programmatically test setting the spacer visible via the layout container.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VerticalTabRailLayout railLayout =
                            cta.findViewById(R.id.vertical_tab_rail_container);
                    railLayout.setDesktopWindowSpacerVisible(/* visible= */ true);
                });

        // 3. Verify the spacer transitions to VISIBLE.
        onView(withId(R.id.desktop_window_spacer)).check(matches(withEffectiveVisibility(VISIBLE)));

        // 4. Test setting the spacer to GONE.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VerticalTabRailLayout railLayout =
                            cta.findViewById(R.id.vertical_tab_rail_container);
                    railLayout.setDesktopWindowSpacerVisible(/* visible= */ false);
                });

        onView(withId(R.id.desktop_window_spacer)).check(matches(withEffectiveVisibility(GONE)));

        // 5. Restore the original spacer visibility for subsequent tests in the class batch.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VerticalTabRailLayout railLayout =
                            cta.findViewById(R.id.vertical_tab_rail_container);
                    railLayout.setDesktopWindowSpacerVisible(initialSpacerVisible);
                });
    }

    // =========================================================================================
    // AI Indicator Visibility
    // =========================================================================================

    @Test
    @MediumTest
    public void testAiIndicatorVisibility() {
        // Verify the AI indicator view exists in tab row items and is initially GONE by default.
        onView(withId(R.id.tab_list_recycler_view))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return isDisplayed();
                            }

                            @Override
                            public String getDescription() {
                                return "check AI indicator at position 0";
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                RecyclerView recyclerView = (RecyclerView) view;
                                RecyclerView.ViewHolder viewHolder =
                                        recyclerView.findViewHolderForAdapterPosition(0);
                                assertNotNull("ViewHolder at position 0 should exist.", viewHolder);
                                View aiIndicator =
                                        viewHolder.itemView.findViewById(R.id.ai_indicator);
                                assertNotNull("AI indicator view should exist.", aiIndicator);
                                assertTrue(
                                        "AI indicator should be GONE by default.",
                                        aiIndicator.getVisibility() == View.GONE);
                            }
                        });
    }

    // =========================================================================================
    // Drag and Drop Reordering within Rail & Tab Grouping
    // =========================================================================================

    @Test
    @MediumTest
    public void testMoveTabOrderBothDirections() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tabA = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tabB = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tabC = createTabOnUiThread(cta, /* incognito= */ false);

        // 1. Move Tab C UP from the bottom index to index 0 (upwards drag and drop reordering).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tabC.getId(), 0);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getTabAt(0).getId() == tabC.getId(),
                "Tab C should be moved UP to index 0.");

        // 2. Move Tab C DOWN from index 0 to the last index (downwards drag and drop reordering).
        int lastIndex = getTabCountOnUiThread(selector.getCurrentModel()) - 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tabC.getId(), lastIndex);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getTabAt(lastIndex).getId() == tabC.getId(),
                "Tab C should be moved DOWN to the last index.");
    }

    @Test
    @MediumTest
    public void testMoveTabIntoGroup() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab3 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                });

        assertTabInGroup(selector, tab1, "Tab 1 should be in a group.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab3.getId(), tab1.getId());
                });

        assertTabInGroup(selector, tab3, "Tab 3 should be merged into Tab 1's group.");
    }

    @Test
    @MediumTest
    public void testMoveTabWithinGroupBothDirections() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab3 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                    selector.getCurrentModel().mergeTabsToGroup(tab3.getId(), tab1.getId());
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().isTabInTabGroup(tab3),
                "All 3 tabs should be in a group.");

        // 1. Move child Tab 3 UP within the group to index 0.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tab3.getId(), 0);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        selector.getCurrentModel().getTabAt(0).getId() == tab3.getId()
                                && selector.getCurrentModel().isTabInTabGroup(tab3),
                "Tab 3 should be moved UP to index 0 while remaining in the group.");

        // 2. Move child Tab 3 DOWN within the group to index 2.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tab3.getId(), 2);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        selector.getCurrentModel().getTabAt(2).getId() == tab3.getId()
                                && selector.getCurrentModel().isTabInTabGroup(tab3),
                "Tab 3 should be moved DOWN to index 2 while remaining in the group.");
    }

    @Test
    @MediumTest
    public void testMoveTabGroupOrderBothDirections() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tabA = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tabB = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tabC = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tabA.getId(), tabB.getId());
                });

        assertTabInGroup(selector, tabA, "Tab A and B should be in a group.");

        // 1. Move Tab C UP above the Tab Group to index 0.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tabC.getId(), 0);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        selector.getCurrentModel().getTabAt(0).getId() == tabC.getId()
                                && !selector.getCurrentModel().isTabInTabGroup(tabC),
                "Tab C should be moved UP above the Tab Group to index 0.");

        // 2. Move Tab C DOWN below the Tab Group to the last index.
        int lastIndex = getTabCountOnUiThread(selector.getCurrentModel()) - 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tabC.getId(), lastIndex);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        selector.getCurrentModel().getTabAt(lastIndex).getId() == tabC.getId()
                                && !selector.getCurrentModel().isTabInTabGroup(tabC),
                "Tab C should be moved DOWN below the Tab Group.");
    }

    @Test
    @MediumTest
    public void testDragTabOutOfGroupBothDirections() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tab1 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab2 = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tab3 = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().mergeTabsToGroup(tab1.getId(), tab2.getId());
                    selector.getCurrentModel().mergeTabsToGroup(tab3.getId(), tab1.getId());
                });

        assertTabInGroup(selector, tab3, "All 3 tabs should be in a group initially.");

        // 1. Simulate dragging Tab 1 UPWARDS out of the group (trailing = false).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel()
                            .getTabUngrouper()
                            .ungroupTabs(
                                    List.of(tab1), /* trailing= */ false, /* allowDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> !selector.getCurrentModel().isTabInTabGroup(tab1),
                "Tab 1 should be ungrouped after dragging UPWARDS out of group.");

        // 2. Simulate dragging Tab 3 DOWNWARDS out of the group (trailing = true).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel()
                            .getTabUngrouper()
                            .ungroupTabs(
                                    List.of(tab3), /* trailing= */ true, /* allowDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> !selector.getCurrentModel().isTabInTabGroup(tab3),
                "Tab 3 should be ungrouped after dragging DOWNWARDS out of group.");
    }

    @Test
    @MediumTest
    public void testMovePinnedTabOrderBothDirections() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelectorSupplier().get();

        Tab tabA = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tabB = createTabOnUiThread(cta, /* incognito= */ false);
        Tab tabC = createTabOnUiThread(cta, /* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().pinTab(tabA.getId(), /* showUngroupDialog= */ false);
                    selector.getCurrentModel().pinTab(tabB.getId(), /* showUngroupDialog= */ false);
                    selector.getCurrentModel().pinTab(tabC.getId(), /* showUngroupDialog= */ false);
                });

        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 3,
                "Model should have 3 pinned tabs.");

        // 1. Move Pinned Tab C UP/LEFT from index 2 to index 0.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tabC.getId(), 0);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        selector.getCurrentModel().getTabAt(0).getId() == tabC.getId()
                                && tabC.getIsPinned(),
                "Pinned Tab C should be moved UP/LEFT to index 0.");

        // 2. Move Pinned Tab C DOWN/RIGHT from index 0 to index 2.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().moveTab(tabC.getId(), 2);
                });

        CriteriaHelper.pollUiThread(
                () ->
                        selector.getCurrentModel().getTabAt(2).getId() == tabC.getId()
                                && tabC.getIsPinned(),
                "Pinned Tab C should be moved DOWN/RIGHT to index 2.");

        // Cleanup: unpin tabs so subsequent batched tests start clean.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().unpinTab(tabA.getId());
                    selector.getCurrentModel().unpinTab(tabB.getId());
                    selector.getCurrentModel().unpinTab(tabC.getId());
                });
        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == 0,
                "Pinned tabs should be unpinned.");
    }

    private static Tab createTabOnUiThread(ChromeTabbedActivity cta, boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return cta.getTabCreator(incognito)
                            .createNewTab(
                                    new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                                    TabLaunchType.FROM_CHROME_UI,
                                    /* parent= */ null,
                                    /* position= */ 0);
                });
    }

    private static void waitForViewVisibility(
            ChromeTabbedActivity cta, int viewId, int visibility) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View view = cta.findViewById(viewId);
                    return view != null && view.getVisibility() == visibility;
                },
                "View " + viewId + " should transition to visibility " + visibility);
    }

    private static void assertTabInGroup(TabModelSelector selector, Tab tab, String message) {
        CriteriaHelper.pollUiThread(() -> selector.getCurrentModel().isTabInTabGroup(tab), message);
    }

    private static void assertActiveTabId(
            TabModelSelector selector, int expectedTabId, String message) {
        CriteriaHelper.pollUiThread(() -> selector.getCurrentTabId() == expectedTabId, message);
    }

    private static void pinTabAndWait(TabModelSelector selector, Tab tab, int expectedPinnedCount) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().pinTab(tab.getId(), /* showUngroupDialog= */ false);
                });
        CriteriaHelper.pollUiThread(
                () -> selector.getCurrentModel().getPinnedTabsCount() == expectedPinnedCount,
                "Pinned tabs count should be " + expectedPinnedCount + ".");
    }

    private static void performActionOnRecyclerViewItem(
            int recyclerViewId, int index, String description, Callback<View> action) {
        onView(withId(recyclerViewId))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return isDisplayed();
                            }

                            @Override
                            public String getDescription() {
                                return description + " at position " + index;
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                RecyclerView recyclerView = (RecyclerView) view;
                                RecyclerView.ViewHolder viewHolder =
                                        recyclerView.findViewHolderForAdapterPosition(index);
                                assertNotNull(
                                        "ViewHolder at position " + index + " should exist.",
                                        viewHolder);
                                action.onResult(viewHolder.itemView);
                            }
                        });
    }

    private static void clickTabItemAtPosition(int index) {
        performActionOnRecyclerViewItem(
                R.id.tab_list_recycler_view, index, "click tab item", view -> view.performClick());
    }

    private static void clickActionButtonAtPosition(int index) {
        performActionOnRecyclerViewItem(
                R.id.tab_list_recycler_view,
                index,
                "click action button",
                view -> {
                    View actionButton = view.findViewById(R.id.action_button);
                    assertNotNull("Action button should exist at position " + index, actionButton);
                    actionButton.performClick();
                });
    }

    private static void clickPinnedTabItemAtPosition(int index) {
        performActionOnRecyclerViewItem(
                R.id.pinned_tabs_recycler_view,
                index,
                "click pinned tab item",
                view -> view.performClick());
    }
}
