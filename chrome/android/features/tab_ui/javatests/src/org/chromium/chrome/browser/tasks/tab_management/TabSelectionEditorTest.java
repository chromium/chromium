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

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.InstrumentationRegistry;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.core.view.MenuItemCompat;
import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
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
@Batch(Batch.PER_CLASS)
public class TabSelectionEditorTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER)
                    .setRevision(2)
                    .build();

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
        mTabModelSelector = sActivityTestRule.getActivity().getTabModelSelector();
        mParentView = (ViewGroup) sActivityTestRule.getActivity().findViewById(R.id.coordinator);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                    sActivityTestRule.getActivity(), mParentView, mTabModelSelector,
                    sActivityTestRule.getActivity().getTabContentManager(), getMode(),
                    sActivityTestRule.getActivity().getCompositorViewHolderForTesting());

            mTabSelectionEditorController = mTabSelectionEditorCoordinator.getController();
            mTabSelectionEditorLayout =
                    mTabSelectionEditorCoordinator.getTabSelectionEditorLayoutForTesting();
            mRef = new WeakReference<>(mTabSelectionEditorLayout);
        });
    }

    @After
    public void tearDown() {
        if (mTabSelectionEditorCoordinator != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                if (mTabSelectionEditorController.isVisible()) {
                    mTabSelectionEditorController.hide();
                }
                mTabSelectionEditorCoordinator.destroy();
            });

            if (sActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                        LayoutType.TAB_SWITCHER)) {
                TabUiTestHelper.leaveTabSwitcher(sActivityTestRule.getActivity());
            }
        }
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                       sActivityTestRule.getActivity())
                        && SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private void prepareBlankTab(int num, boolean isIncognito) {
        for (int i = 0; i < num - 1; i++) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    sActivityTestRule.getActivity(), isIncognito, true);
            sActivityTestRule.loadUrl("about:blank");
        }
    }

    private void prepareBlankTabWithThumbnail(int num, boolean isIncognito) {
        if (isIncognito) {
            TabUiTestHelper.prepareTabsWithThumbnail(sActivityTestRule, 0, num, "about:blank");
        } else {
            TabUiTestHelper.prepareTabsWithThumbnail(sActivityTestRule, num, 0, "about:blank");
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
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
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
    @FlakyTest(message = "https://crbug.com/1237368")
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
        // verifyUndoSnackbarWithTextIsShown(sActivityTestRule.getActivity().getString(
        //     R.string.undo_bar_group_tabs_message, 2));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUndoToolbarGroup() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @DisabledTest(message = "https://crbug.com/1352950")
    public void testConfigureToolbarMenuItems() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorGroupAction.createAction(
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(tabs);
        });

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        final int groupId = R.id.tab_selection_editor_group_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId).verifyToolbarActionViewWithText(
                closeId, "Close");
        verifyToolbarMenuItemState(groupId, /*enabled=*/false);

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot.verifyToolbarActionViewEnabled(closeId).verifyToolbarActionViewWithText(
                closeId, "Close");
        verifyToolbarMenuItemState(groupId, /*enabled=*/true);

        // TODO(crbug.com/1352950): This step failed to deselect the first tab. It is likely
        // verifyToolbarMenuItemState() left the menu open (race condition?) so the deselect click
        // got consumed prior to clicking on the tab.
        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId).verifyToolbarActionViewWithText(
                closeId, "Close");
        verifyToolbarMenuItemState(groupId, /*enabled=*/false);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testToolbarMenuItem_CloseActionView() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(tabs);
        });

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickToolbarActionView(closeId);

        assertEquals(1, getTabsInCurrentTabModel().size());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testToolbarMenuItem_CloseMenuItem() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(tabs);
        });

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        showToolbarMenu();
        mRobot.actionRobot.clickToolbarMenuItem("Close");
        hideToolbarMenu();

        assertEquals(1, getTabsInCurrentTabModel().size());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testSelectionAction_IndividualTabSelection() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorSelectionAction.createAction(
                    sActivityTestRule.getActivity(), ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT,
                    IconPosition.END, /*isIncognito=*/false));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(tabs);
        });

        final int selectionId = R.id.tab_selection_editor_selection_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickItemAtAdapterPosition(1);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Deselect all");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");
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
    @LargeTest
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabSelectionEditorController.show(tabs); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance_oneSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabSelectionEditorController.show(tabs); });

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_one_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance_onePreSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 1;

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorController.show(tabs, preSelectedTabCount); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_one_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance_twoPreSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 2;

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorController.show(tabs, preSelectedTabCount); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_two_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance_allPreSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = tabs.size();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorController.show(tabs, preSelectedTabCount); });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_all_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testSelectionAction_Toggle() throws IOException {
        prepareBlankTab(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorSelectionAction.createAction(
                    sActivityTestRule.getActivity(), ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT,
                    IconPosition.END, /*isIncognito=*/false));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(tabs);
        });

        final int selectionId = R.id.tab_selection_editor_selection_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickToolbarActionView(selectionId);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Deselect all")
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyItemSelectedAtAdapterPosition(2)
                .verifyToolbarSelectionText("3 selected");

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "selection_action_all_tabs_selected");

        mRobot.actionRobot.clickToolbarActionView(selectionId);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all")
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyItemNotSelectedAtAdapterPosition(2);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "selection_action_all_tabs_deselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
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

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testToolbarActionViewAndMenuItemContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorGroupAction.createAction(
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(tabs);
        });
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        final int groupId = R.id.tab_selection_editor_group_menu_item;
        View close = mTabSelectionEditorLayout.getToolbar().findViewById(closeId);
        MenuItem group = mTabSelectionEditorLayout.getToolbar().getMenu().findItem(groupId);
        assertNull(close.getContentDescription());
        assertNull(MenuItemCompat.getContentDescription(group));

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        assertEquals("Close 2 selected tabs", close.getContentDescription());
        assertEquals("Group 2 selected tabs", MenuItemCompat.getContentDescription(group));
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
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
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
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
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

    private void showToolbarMenu() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorLayout.getToolbar().showOverflowMenu(); });
    }

    private void hideToolbarMenu() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorLayout.getToolbar().hideOverflowMenu(); });
    }

    private void verifyToolbarMenuItemState(int id, boolean enabled) {
        // Espresso works poorly for MenuItem as the ID of the view != the MenuItem's ID and the
        // enabled state is handled outside of the view hierarchy.
        showToolbarMenu();
        boolean[] isEnabled = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            isEnabled[0] =
                    mTabSelectionEditorLayout.getToolbar().getMenu().findItem(id).isEnabled();
        });
        assertEquals(enabled, isEnabled[0]);
        hideToolbarMenu();
    }
}
