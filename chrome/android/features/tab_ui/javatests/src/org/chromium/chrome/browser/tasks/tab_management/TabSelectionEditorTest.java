// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_YES;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * End-to-end test for TabSelectionEditor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
@DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
public class TabSelectionEditorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private TabSelectionEditorTestingRobot mRobot = new TabSelectionEditorTestingRobot();

    private TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private TabSelectionEditorLayout mTabSelectionEditorLayout;
    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private WeakReference<TabSelectionEditorLayout> mRef;

    private ViewGroup mParentView;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mParentView = (ViewGroup) mActivityTestRule.getActivity().findViewById(R.id.coordinator);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                    mActivityTestRule.getActivity(), mParentView, mTabModelSelector,
                    mActivityTestRule.getActivity().getTabContentManager(), getMode());

            mTabSelectionEditorController = mTabSelectionEditorCoordinator.getController();
            mTabSelectionEditorLayout =
                    mTabSelectionEditorCoordinator.getTabSelectionEditorLayoutForTesting();
            mRef = new WeakReference<>(mTabSelectionEditorLayout);
        });
    }

    @After
    public void tearDown() {
        if (mTabSelectionEditorCoordinator != null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { mTabSelectionEditorCoordinator.destroy(); });
        }
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()
                        && SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private void prepareBlankTab(int num, boolean isIncognito) {
        for (int i = 0; i < num - 1; i++) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), isIncognito, true);
            mActivityTestRule.loadUrl("about:blank");
        }
    }

    private void prepareBlankTabWithThumbnail(int num, boolean isIncognito) {
        if (isIncognito) {
            TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 0, num, "about:blank");
        } else {
            TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, num, 0, "about:blank");
        }
    }

    @Test
    @MediumTest
    public void testShowTabs() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(1);
    }

    @Test
    @MediumTest
    public void testToggleItem() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0).verifyToolbarSelectionText(
                "1 selected");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testToolbarNavigationButtonHideTabSelectionEditor() {
        // clang-format on
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    @Test
    @MediumTest
    public void testToolbarGroupButtonEnabledState() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled();

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionButtonEnabled();

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled();
    }

    @Test
    @MediumTest
    public void testToolbarGroupButton() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyToolbarActionButtonWithResourceId(
                R.string.tab_selection_editor_group);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionButton();

        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();

        // TODO(1021803): verify the undo snack after the bug is resolved.
        // verifyUndoSnackbarWithTextIsShown(mActivityTestRule.getActivity().getString(
        //     R.string.undo_bar_group_tabs_message, 2));
    }

    @Test
    @MediumTest
    public void testUndoToolbarGroup() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TabUiTestHelper.enterTabSwitcher(cta);

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyToolbarActionButtonWithResourceId(
                R.string.tab_selection_editor_group);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionButton();

        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 1);

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    public void testConfigureToolbar_ActionButtonEnableThreshold() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        int enableThreshold = 1;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorController.configureToolbar("Test",
                    R.plurals.accessibility_tab_selection_editor_group_button, null,
                    enableThreshold, null);
            mTabSelectionEditorController.show(tabs);
        });

        mRobot.resultRobot.verifyToolbarActionButtonDisabled().verifyToolbarActionButtonWithText(
                "Test");

        for (int i = 0; i < enableThreshold; i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot.verifyToolbarActionButtonEnabled();

        mRobot.actionRobot.clickItemAtAdapterPosition(enableThreshold - 1);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled();
    }

    @Test
    @MediumTest
    public void testShowTabsWithPreSelectedTabs() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabSelectionEditorController.show(tabs, preSelectedTabCount));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group)
                .verifyToolbarSelectionText("1 selected")
                .verifyHasAtLeastNItemVisible(tabs.size() + 1)
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyHasItemViewTypeAtAdapterPosition(1, TabProperties.UiType.DIVIDER)
                .verifyDividerAlwaysStartsAtTheEdgeOfScreen();
    }

    @Test
    @MediumTest
    // clang-format off
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1, supported_abis_includes = "x86",
        message = "https://crbug.com/1075548")
    public void testShowTabsWithPreSelectedTabs_6Tabs() {
        // clang-format on
        prepareBlankTab(7, false);
        int preSelectedTabCount = 6;
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabSelectionEditorController.show(tabs, preSelectedTabCount));

        mRobot.resultRobot.verifyToolbarSelectionText("6 selected")
                .verifyHasItemViewTypeAtAdapterPosition(
                        preSelectedTabCount, TabProperties.UiType.DIVIDER)
                .verifyDividerAlwaysStartsAtTheEdgeOfScreenAtPosition(preSelectedTabCount);
    }

    @Test
    @MediumTest
    public void testDividerIsNotClickable() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabSelectionEditorController.show(tabs, preSelectedTabCount));

        mRobot.resultRobot.verifyDividerNotClickableNotFocusable();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testGridViewAppearance() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabSelectionEditorController.show(tabs); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testGridViewAppearance_oneSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabSelectionEditorController.show(tabs); });

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_one_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testGridViewAppearance_onePreSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 1;

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorController.show(tabs, preSelectedTabCount); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_one_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testGridViewAppearance_twoPreSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 2;

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorController.show(tabs, preSelectedTabCount); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_two_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testGridViewAppearance_allPreSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = tabs.size();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorController.show(tabs, preSelectedTabCount); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_all_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testListViewAppearance() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "list_view");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testListViewAppearance_oneSelectedTab() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "list_view_one_selected_tab");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testListView_select() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled().verifyTabSelectionEditorIsVisible();
        mRobot.actionRobot.clickEndButtonAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionButtonEnabled().verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testTabSelectionEditorLayoutCanBeGarbageCollected() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorCoordinator.destroy();
            mTabSelectionEditorCoordinator = null;
            mTabSelectionEditorLayout = null;
            mTabSelectionEditorController = null;
        });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // A longer timeout is needed. Achieve that by using the CriteriaHelper.pollUiThread.
        CriteriaHelper.pollUiThread(() -> GarbageCollectionTestUtils.canBeGarbageCollected(mRef));
    }

    @Test
    @MediumTest
    public void testSelectionTabAccessibilityString() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        String expectedAccessibilityString = "Select about:blank tab";

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        // Test deselected tab
        View tabView = mTabSelectionEditorCoordinator.getTabListRecyclerViewForTesting()
                               .findViewHolderForAdapterPosition(0)
                               .itemView;
        assertFalse(tabView.createAccessibilityNodeInfo().isChecked());

        // Test selected tab
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        assertTrue(tabView.createAccessibilityNodeInfo().isChecked());
    }

    @Test
    @MediumTest
    public void testToolbarActionButtonContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorController.configureToolbar("Group",
                    R.plurals.accessibility_tab_selection_editor_group_button, null, 2, null);
            mTabSelectionEditorController.show(tabs);
        });
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        Button actionButton =
                mTabSelectionEditorLayout.getToolbar().findViewById(R.id.action_button);
        assertNull(actionButton.getContentDescription());

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        assertEquals("Group 2 selected tabs", actionButton.getContentDescription());
    }

    // This is a regression test for crbug.com/1132478.
    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testTabSelectionEditorContentDescription() {
        // clang-format on
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        assertEquals("Multi-select mode", mTabSelectionEditorLayout.getContentDescription());
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testToolbarNavigationButtonContentDescription() {
        // clang-format on
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        assertEquals("Hide multi-select mode",
                mTabSelectionEditorLayout.getToolbar().getNavigationContentDescription());
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testEditorHideCorrectly() {
        // clang-format on
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        Espresso.pressBack();
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testBackgroundViewAccessibilityImportance() {
        // clang-format on
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        Map<View, Integer> initialValues = getParentViewAccessibilityImportanceMap();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabSelectionEditorController.show(tabs); });
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        ViewGroup parentView = (ViewGroup) mTabSelectionEditorLayout.getParent();
        verifyBackgroundViewAccessibilityImportance(parentView, true, initialValues);

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        verifyBackgroundViewAccessibilityImportance(parentView, false, initialValues);
    }

    private Map<View, Integer> getParentViewAccessibilityImportanceMap() {
        Map<View, Integer> map = new HashMap<>();

        for (int i = 0; i < mParentView.getChildCount(); i++) {
            View view = mParentView.getChildAt(i);
            map.put(view, view.getImportantForAccessibility());
        }

        map.put(mParentView, mParentView.getImportantForAccessibility());
        return map;
    }

    private void verifyBackgroundViewAccessibilityImportance(ViewGroup parentView,
            boolean isTabSelectionEditorShowing, Map<View, Integer> initialValues) {
        assertEquals(isTabSelectionEditorShowing ? IMPORTANT_FOR_ACCESSIBILITY_NO
                                                 : initialValues.get(parentView).intValue(),
                parentView.getImportantForAccessibility());

        for (int i = 0; i < parentView.getChildCount(); i++) {
            View view = parentView.getChildAt(i);
            int expected = isTabSelectionEditorShowing
                    ? IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                    : initialValues.get(view).intValue();
            if (view == mTabSelectionEditorLayout) {
                expected = IMPORTANT_FOR_ACCESSIBILITY_YES;
            }

            assertEquals(expected, view.getImportantForAccessibility());
        }
    }

    private List<Tab> getTabsInCurrentTabModel() {
        List<Tab> tabs = new ArrayList<>();

        TabModel currentTabModel = mTabModelSelector.getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }

        return tabs;
    }
}
