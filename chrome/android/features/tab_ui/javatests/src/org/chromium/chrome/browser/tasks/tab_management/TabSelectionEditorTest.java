// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_YES;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasType;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.isNotNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_TO_GTS_ANIMATION;

import android.content.Intent;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * End-to-end test for TabSelectionEditor.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
@DisableFeatures(TAB_TO_GTS_ANIMATION)
@Batch(Batch.PER_CLASS)
public class TabSelectionEditorTest {
    private static final String TAB_GROUP_LAUNCH_POLISH_PARAMS =
            "force-fieldtrial-params=Study.Group:enable_launch_polish/true";
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";
    private static final String PAGE_WITH_NO_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_no_canonical.html";
    // clang-format on

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
                    .setRevision(8)
                    .setDescription("TabGridThumbnailView update.")
                    .build();

    @Mock
    private Callback<RecyclerViewPosition> mSetRecyclerViewPosition;

    private TabSelectionEditorTestingRobot mRobot = new TabSelectionEditorTestingRobot();

    private TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private TabSelectionEditorLayout mTabSelectionEditorLayout;
    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private WeakReference<TabSelectionEditorLayout> mRef;

    private ViewGroup mParentView;
    private SnackbarManager mSnackbarManager;
    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mTabModelSelector = sActivityTestRule.getActivity().getTabModelSelector();
        mParentView = (ViewGroup) sActivityTestRule.getActivity().findViewById(R.id.coordinator);
        mSnackbarManager = sActivityTestRule.getActivity().getSnackbarManager();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                    sActivityTestRule.getActivity(), mParentView,
                    sActivityTestRule.getActivity().getBrowserControlsManager(), mTabModelSelector,
                    sActivityTestRule.getActivity().getTabContentManager(),
                    mSetRecyclerViewPosition, getMode(),
                    sActivityTestRule.getActivity().getCompositorViewHolderForTesting(),
                    /*displayGroups=*/true, mSnackbarManager);

            mTabSelectionEditorController = mTabSelectionEditorCoordinator.getController();
            mTabSelectionEditorLayout =
                    mTabSelectionEditorCoordinator.getTabSelectionEditorLayoutForTesting();
            mRef = new WeakReference<>(mTabSelectionEditorLayout);
            mBookmarkModel = sActivityTestRule.getActivity().getBookmarkModelForTesting();
        });
    }

    @After
    public void tearDown() {
        if (mTabSelectionEditorCoordinator != null) {
            if (sActivityTestRule.getActivity().findViewById(R.id.app_menu_list) != null) {
                Espresso.pressBack();
            }

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
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSnackbarManager.dismissAllSnackbars(); });
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return SysUtils.isLowEndDevice() ? TabListCoordinator.TabListMode.LIST
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

    private void prepareBlankTabGroup(int num, boolean isIncognito) {
        ArrayList<String> urls = new ArrayList<String>(Collections.nCopies(num, "about:blank"));

        prepareTabGroupWithUrls(urls, isIncognito);
    }

    private void prepareTabGroupWithUrls(ArrayList<String> urls, boolean isIncognito) {
        for (String url : urls) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    sActivityTestRule.getActivity(), isIncognito, true);
            sActivityTestRule.loadUrl(url);
        }
        if (urls.size() == 1) return;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ArrayList<Tab> tabs = new ArrayList<>();
            TabModel model = mTabModelSelector.getCurrentModel();
            TabGroupModelFilter filter =
                    (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                            .getCurrentTabModelFilter();
            for (int i = model.getCount() - urls.size(); i < model.getCount(); i++) {
                tabs.add(model.getTabAt(i));
            }
            // Don't notify to avoid snackbar appearing.
            filter.mergeListOfTabsToGroup(tabs.subList(1, tabs.size()), tabs.get(0),
                    /*isSameGroup=*/false, /*notify=*/false);
        });
    }

    @Test
    @MediumTest
    public void testShowTabs() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionViewDisabled(R.id.tab_selection_editor_group_menu_item)
                .verifyToolbarActionViewWithText(
                        R.id.tab_selection_editor_group_menu_item, "Group tabs")
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(1);
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Close tabs", /*enabled=*/false);
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    public void testToggleItem() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0).verifyToolbarSelectionText(
                "1 tab");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs);
    }

    @Test
    @MediumTest
    public void testToolbarNavigationButtonHideTabSelectionEditor() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();

        verify(mSetRecyclerViewPosition, times(1)).onResult(isNotNull());
    }

    @Test
    @MediumTest
    public void testToolbarGroupButtonEnabledState() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyToolbarActionViewDisabled(
                R.id.tab_selection_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarActionViewDisabled(
                R.id.tab_selection_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(
                R.id.tab_selection_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionViewDisabled(
                R.id.tab_selection_editor_group_menu_item);
    }

    @Test
    @MediumTest
    public void testUndoToolbarGroup() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TabUiTestHelper.enterTabSwitcher(cta);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyToolbarActionViewDisabled(
                R.id.tab_selection_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionView(R.id.tab_selection_editor_group_menu_item);

        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 1);

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testConfigureToolbarMenuItems() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        final int groupId = R.id.tab_selection_editor_group_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId).verifyToolbarActionViewWithText(
                closeId, "Close tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /*enabled=*/false);
        Espresso.pressBack();

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot.verifyToolbarActionViewEnabled(closeId).verifyToolbarActionViewWithText(
                closeId, "Close tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /*enabled=*/true);
        Espresso.pressBack();

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId).verifyToolbarActionViewWithText(
                closeId, "Close tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /*enabled=*/false);
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_CloseActionView() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickToolbarActionView(closeId);

        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        verify(mSetRecyclerViewPosition, times(2)).onResult(isNotNull());

        assertEquals(1, getTabsInCurrentTabModel().size());
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_CloseActionView_WithGroups() {
        prepareBlankTab(2, false);
        prepareBlankTabGroup(3, false);
        prepareBlankTabGroup(1, false);
        prepareBlankTabGroup(2, false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(closeId).verifyToolbarSelectionText(
                "5 tabs");

        View close = mTabSelectionEditorLayout.getToolbar().findViewById(closeId);
        assertEquals("Close 5 selected tabs", close.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(closeId);

        assertEquals(3, getTabsInCurrentTabModel().size());
    }

    // Regression test for https://crbug.com/1374935
    @Test
    @MediumTest
    public void testToolbarMenuItem_GroupActionView_WithGroups() {
        prepareBlankTab(2, false); // Index: 0, 1
        prepareBlankTabGroup(3, false); // Index: 2
        prepareBlankTabGroup(1, false); // Index: 3
        prepareBlankTabGroup(2, false); // Index: 4
        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int groupId = R.id.tab_selection_editor_group_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(groupId);

        mRobot.actionRobot.clickItemAtAdapterPosition(4)
                .clickItemAtAdapterPosition(3)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(groupId).verifyToolbarSelectionText(
                "5 tabs");

        View close = mTabSelectionEditorLayout.getToolbar().findViewById(groupId);
        assertEquals("Group 5 selected tabs", close.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(groupId);

        assertEquals(2, getTabsInCurrentTabModelFilter().size());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testToolbarMenuItem_GroupActionAndUndo() throws Exception {
        prepareBlankTabWithThumbnail(2, false);
        prepareBlankTabGroup(3, false);
        prepareBlankTabGroup(1, false);
        prepareBlankTabGroup(2, false);
        TabUiTestHelper.createTabsWithThumbnail(sActivityTestRule, 1, "about:blank", false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();
        List<Tab> beforeTabOrder = getTabsInCurrentTabModel();

        Tab selectedTab = beforeTabOrder.get(4);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabModelSelector.getCurrentModel().setIndex(4, TabSelectionType.FROM_USER, false);
        });
        assertEquals(selectedTab, mTabModelSelector.getCurrentTab());

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int groupId = R.id.tab_selection_editor_group_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(groupId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3)
                .clickItemAtAdapterPosition(4)
                .clickItemAtAdapterPosition(5);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(groupId).verifyToolbarSelectionText(
                "9 tabs");

        View group = mTabSelectionEditorLayout.getToolbar().findViewById(groupId);
        assertEquals("Group 9 selected tabs", group.getContentDescription());

        // Force the position to something fixed to 100% avoid flakes here.
        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    TabListRecyclerView recyclerView =
                            ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                                    R.id.tab_list_view));
                    recyclerView.scrollToPosition(4);
                    return recyclerView;
                });

        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);
        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "groups_before_undo_scrolled");

        mRobot.actionRobot.clickToolbarActionView(groupId);

        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        TabUiTestHelper.verifyTabSwitcherCardCount(sActivityTestRule.getActivity(), 1);

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        TabUiTestHelper.verifyTabSwitcherCardCount(sActivityTestRule.getActivity(), 6);

        assertEquals(selectedTab, mTabModelSelector.getCurrentTab());
        List<Tab> finalTabs = getTabsInCurrentTabModel();
        assertEquals(beforeTabOrder.size(), finalTabs.size());
        assertEquals(beforeTabOrder, finalTabs);
        List<Tab> finalRootTabs = getTabsInCurrentTabModelFilter();
        assertEquals(tabs.size(), finalRootTabs.size());
        assertEquals(tabs, finalRootTabs);
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_CloseMenuItem() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tabs");
        Espresso.pressBack();

        assertEquals(2, getTabsInCurrentTabModel().size());

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tab");

        assertEquals(1, getTabsInCurrentTabModel().size());
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionView() throws IOException {
        Intents.init();
        prepareBlankTab(1, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorShareAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int shareId = R.id.tab_selection_editor_share_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewWithText(shareId, "Share tabs");
        mRobot.resultRobot.verifyToolbarActionViewDisabled(shareId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(shareId).verifyToolbarSelectionText(
                "1 tab");

        View share = mTabSelectionEditorLayout.getToolbar().findViewById(shareId);
        assertEquals("Share 1 selected tab", share.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(shareId);

        CriteriaHelper.pollUiThread(()
                                            -> Criteria.checkThat("Share sheet was not shown.",
                                                    sActivityTestRule.getActivity()
                                                            .getRootUiCoordinatorForTesting()
                                                            .getBottomSheetController(),
                                                    notNullValue()));

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/plain"))))));
        Intents.release();
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionTabsOnly() throws IOException {
        prepareBlankTab(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorShareAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(2);

        TabSelectionEditorShareAction.setIntentCallbackForTesting((result -> {
            assertEquals(Intent.ACTION_SEND, result.getAction());
            assertEquals(httpsCanonicalUrl, result.getStringExtra(Intent.EXTRA_TEXT));
            assertEquals("text/plain", result.getType());
            assertEquals("1 link from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
        }));

        final int shareId = R.id.tab_selection_editor_share_menu_item;
        mRobot.actionRobot.clickToolbarActionView(shareId);
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionGroupsOnly() throws IOException {
        ArrayList<String> urls = new ArrayList<String>();
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL));
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTP_CANONICAL_URL));
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_NO_CANONICAL_URL));
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_NO_CANONICAL_URL));

        prepareTabGroupWithUrls(urls, false);
        prepareBlankTabGroup(2, false);

        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        // Url string formatting
        for (int i = 0; i < urls.size(); i++) {
            urls.set(i, (i + 1) + ". " + urls.get(i));
        }
        urls.add("");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorShareAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickItemAtAdapterPosition(1).clickItemAtAdapterPosition(2);

        TabSelectionEditorShareAction.setIntentCallbackForTesting((result -> {
            assertEquals(Intent.ACTION_SEND, result.getAction());
            assertEquals(String.join("\n", urls), result.getStringExtra(Intent.EXTRA_TEXT));
            assertEquals("text/plain", result.getType());
            assertEquals("4 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
        }));

        final int shareId = R.id.tab_selection_editor_share_menu_item;
        mRobot.actionRobot.clickToolbarActionView(shareId);
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionTabsWithGroups() throws IOException {
        prepareBlankTab(2, false);

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        prepareBlankTabGroup(2, false);

        ArrayList<String> urls = new ArrayList<String>();
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTP_CANONICAL_URL));
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_NO_CANONICAL_URL));
        prepareTabGroupWithUrls(urls, false);

        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        // Url string formatting
        urls.add(0, httpsCanonicalUrl);
        for (int i = 0; i < urls.size(); i++) {
            urls.set(i, (i + 1) + ". " + urls.get(i));
        }
        urls.add("");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorShareAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3);

        TabSelectionEditorShareAction.setIntentCallbackForTesting((result -> {
            assertEquals(Intent.ACTION_SEND, result.getAction());
            assertEquals(String.join("\n", urls), result.getStringExtra(Intent.EXTRA_TEXT));
            assertEquals("text/plain", result.getType());
            assertEquals("3 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
        }));

        final int shareId = R.id.tab_selection_editor_share_menu_item;
        mRobot.actionRobot.clickToolbarActionView(shareId);
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_ShareActionAllFilterableTabs() throws Exception {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorShareAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int shareId = R.id.tab_selection_editor_share_menu_item;
        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionViewDisabled(shareId).verifyToolbarSelectionText(
                "2 tabs");
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_BookmarkActionSingleTab() throws Exception {
        prepareBlankTab(1, false);

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(
                    TabSelectionEditorBookmarkAction.createAction(sActivityTestRule.getActivity(),
                            ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int bookmarkId = R.id.tab_selection_editor_bookmark_menu_item;
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickToolbarActionView(bookmarkId);

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mBookmarkModel.doesBookmarkExist(
                    mBookmarkModel.getUserBookmarkIdForTab(tabs.get(0))));

            Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
            Assert.assertEquals(
                    Snackbar.UMA_BOOKMARK_ADDED, currentSnackbar.getIdentifierForTesting());
            Assert.assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
            currentSnackbar.getController().onAction(null);
        });
        BookmarkEditActivity activity = BookmarkTestUtil.waitForEditActivity();
        activity.finish();

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_BookmarkActionGroupsOnly() throws Exception {
        prepareBlankTabGroup(2, false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(
                    TabSelectionEditorBookmarkAction.createAction(sActivityTestRule.getActivity(),
                            ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int bookmarkId = R.id.tab_selection_editor_bookmark_menu_item;
        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickToolbarActionView(bookmarkId);

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Tab tab : tabs) {
                Assert.assertTrue(mBookmarkModel.doesBookmarkExist(
                        mBookmarkModel.getUserBookmarkIdForTab(tab)));
            }
            Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
            Assert.assertEquals(
                    Snackbar.UMA_BOOKMARK_ADDED, currentSnackbar.getIdentifierForTesting());
            Assert.assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
        });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mTabSelectionEditorController.handleBackPressed());
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        Assert.assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mSnackbarManager.dismissSnackbars(
                                mSnackbarManager.getCurrentSnackbarForTesting().getController()));
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_BookmarkActionTabsWithGroups() throws Exception {
        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        ArrayList<String> urls = new ArrayList<String>();
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTP_CANONICAL_URL));
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_NO_CANONICAL_URL));

        prepareTabGroupWithUrls(urls, false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(
                    TabSelectionEditorBookmarkAction.createAction(sActivityTestRule.getActivity(),
                            ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int bookmarkId = R.id.tab_selection_editor_bookmark_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewWithText(bookmarkId, "Bookmark tabs");
        mRobot.resultRobot.verifyToolbarActionViewDisabled(bookmarkId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(1);

        mRobot.resultRobot.verifyToolbarActionViewEnabled(bookmarkId)
                .verifyToolbarSelectionText("3 tabs");

        View bookmark = mTabSelectionEditorLayout.getToolbar().findViewById(bookmarkId);
        assertEquals("Bookmark 3 selected tabs", bookmark.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(bookmarkId);

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
            Assert.assertEquals(
                    Snackbar.UMA_BOOKMARK_ADDED, currentSnackbar.getIdentifierForTesting());
            Assert.assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
            currentSnackbar.getController().onAction(null);
        });

        BookmarkAddEditFolderActivity activity = BookmarkTestUtil.waitForAddEditFolderActivity();
        activity.finish();

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testSelectionAction_IndividualTabSelection() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(
                    TabSelectionEditorSelectionAction.createAction(sActivityTestRule.getActivity(),
                            ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(
                    tabs, preSelectedTabCount, /*recyclerViewPosition=*/null);
        });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionViewDisabled(R.id.tab_selection_editor_group_menu_item)
                .verifyToolbarActionViewWithText(
                        R.id.tab_selection_editor_group_menu_item, "Group tab")
                .verifyToolbarSelectionText("1 tab")
                .verifyHasAtLeastNItemVisible(tabs.size() + 1)
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyHasItemViewTypeAtAdapterPosition(1, TabProperties.UiType.DIVIDER)
                .verifyDividerAlwaysStartsAtTheEdgeOfScreen();
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Close tab", /*enabled=*/true);
        Espresso.pressBack();
    }

    @Test
    @LargeTest
    public void testShowTabsWithPreSelectedTabs_6Tabs() {
        // clang-format on
        prepareBlankTab(7, false);
        int preSelectedTabCount = 6;
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(
                    tabs, preSelectedTabCount, /*recyclerViewPosition=*/null);
        });

        mRobot.resultRobot.verifyToolbarSelectionText("6 tabs")
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
                ()
                        -> mTabSelectionEditorController.show(
                                tabs, preSelectedTabCount, /*recyclerViewPosition=*/null));

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                            R.id.tab_list_view));
                });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                            R.id.tab_list_view));
                });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_v2_one_selected_tab_0.85");
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(
                    tabs, preSelectedTabCount, /*recyclerViewPosition=*/null);
        });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                            R.id.tab_list_view));
                });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_v2_one_pre_selected_tab_0.85");
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(
                    tabs, preSelectedTabCount, /*recyclerViewPosition=*/null);
        });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                            R.id.tab_list_view));
                });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_v2_pre_selected_tab_0.85");
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
            mTabSelectionEditorController.show(
                    tabs, preSelectedTabCount, /*recyclerViewPosition=*/null);
        });

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                            R.id.tab_list_view));
                });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "grid_view_v2_all_pre_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testSelectionAction_Toggle() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(
                    TabSelectionEditorSelectionAction.createAction(sActivityTestRule.getActivity(),
                            ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        final int selectionId = R.id.tab_selection_editor_selection_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickToolbarActionView(selectionId);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Deselect all")
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyItemSelectedAtAdapterPosition(2)
                .verifyToolbarSelectionText("3 tabs");
        TabListRecyclerView tabListRecyclerView =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return ((TabListRecyclerView) mTabSelectionEditorLayout.findViewById(
                            R.id.tab_list_view));
                });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "selection_action_all_tabs_selected");

        mRobot.actionRobot.clickToolbarActionView(selectionId);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all")
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyItemNotSelectedAtAdapterPosition(2)
                .verifyToolbarSelectionText("Select tabs");
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "selection_action_all_tabs_deselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListViewAppearance() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "list_view");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListViewV2Shows() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListViewAppearance_oneSelectedTab() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabSelectionEditorLayout);
        mRenderTestRule.render(mTabSelectionEditorLayout, "list_view_one_selected_tab");
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListView_select() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot
                .verifyToolbarActionViewDisabled(R.id.tab_selection_editor_group_menu_item)
                .verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
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

        showSelectionEditor(tabs);
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testToolbarMenuItem_SelectAllMenu() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(
                    TabSelectionEditorSelectionAction.createAction(sActivityTestRule.getActivity(),
                            ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Select all", /*enabled=*/true)
                .verifyToolbarMenuItemState("Close tabs", /*enabled=*/false);
        mRobot.actionRobot.clickToolbarMenuItem("Select all");
        mRobot.resultRobot.verifyToolbarMenuItemState("Deselect all", /*enabled=*/true)
                .verifyToolbarMenuItemState("Close tabs", /*enabled=*/true);
        mRobot.actionRobot.clickToolbarMenuItem("Deselect all");
        mRobot.resultRobot.verifyToolbarMenuItemState("Select all", /*enabled=*/true);
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testToolbarActionViewAndMenuItemContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<TabSelectionEditorAction> actions = new ArrayList<>();
            actions.add(TabSelectionEditorCloseAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.IF_ROOM, ButtonType.TEXT, IconPosition.START));
            actions.add(TabSelectionEditorGroupAction.createAction(sActivityTestRule.getActivity(),
                    ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START));

            mTabSelectionEditorController.configureToolbarWithMenuItems(actions, null);
        });
        showSelectionEditor(tabs);
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        final int closeId = R.id.tab_selection_editor_close_menu_item;
        View close = mTabSelectionEditorLayout.getToolbar().findViewById(closeId);
        assertNull(close.getContentDescription());
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /*enabled=*/false)
                .verifyToolbarMenuItemWithContentDescription("Group tabs", null);
        Espresso.pressBack();

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        assertEquals("Close 2 selected tabs", close.getContentDescription());
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /*enabled=*/true)
                .verifyToolbarMenuItemWithContentDescription("Group tabs", "Group 2 selected tabs");
        Espresso.pressBack();

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickToolbarMenuButton();
        assertEquals("Close 1 selected tab", close.getContentDescription());
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tab", /*enabled=*/false)
                .verifyToolbarMenuItemWithContentDescription("Group tab", null);
        Espresso.pressBack();
    }

    // This is a regression test for crbug.com/1132478.
    @Test
    @MediumTest
    public void testTabSelectionEditorContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs);
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        assertEquals("Multi-select mode", mTabSelectionEditorLayout.getContentDescription());
    }

    @Test
    @MediumTest
    public void testToolbarNavigationButtonContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs);
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        assertEquals("Hide multi-select mode",
                mTabSelectionEditorLayout.getToolbar().getNavigationContentDescription());
    }

    @Test
    @MediumTest
    public void testEditorHideCorrectly() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs);
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mTabSelectionEditorController.handleBackPressed());
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        showSelectionEditor(tabs);
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testBackgroundViewAccessibilityImportance() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        Map<View, Integer> initialValues = getParentViewAccessibilityImportanceMap();

        showSelectionEditor(tabs);
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

    /**
     * Retrieves all tabs from the current tab model
     */
    private List<Tab> getTabsInCurrentTabModel() {
        List<Tab> tabs = new ArrayList<>();

        TabModel currentTabModel = mTabModelSelector.getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }

        return tabs;
    }

    /**
     * Retrieves all non-grouped tabs and the last focused tab in each tab group from the current
     * tab model
     */
    private List<Tab> getTabsInCurrentTabModelFilter() {
        List<Tab> tabs = new ArrayList<>();

        TabGroupModelFilter filter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
        for (int i = 0; i < filter.getCount(); i++) {
            tabs.add(filter.getTabAt(i));
        }

        return tabs;
    }

    private void showSelectionEditor(List<Tab> tabs) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorController.show(
                    tabs, /*preSelectedTabCount=*/0, /*recyclerViewPosition=*/null);
        });
    }
}
