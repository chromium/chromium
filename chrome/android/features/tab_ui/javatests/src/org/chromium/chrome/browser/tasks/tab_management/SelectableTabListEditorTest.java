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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.isNotNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUP_PARITY_ANDROID;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
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
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** End-to-end test for the selectable TabListEditor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group",
    "force-fieldtrial-params=Study.Group:enable_launch_polish/true"
})
@DisableFeatures({TAB_GROUP_PARITY_ANDROID})
@Batch(Batch.PER_CLASS)
public class SelectableTabListEditorTest {
    private static final String TAB_GROUP_LAUNCH_POLISH_PARAMS =
            "force-fieldtrial-params=Study.Group:enable_launch_polish/true";
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";
    private static final String PAGE_WITH_NO_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_no_canonical.html";

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
                    .setRevision(9)
                    .setDescription("Favicon update.")
                    .build();

    @Mock private Callback<RecyclerViewPosition> mSetRecyclerViewPosition;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;

    private TabListEditorTestingRobot mRobot = new TabListEditorTestingRobot();

    private TabModelSelector mTabModelSelector;
    private TabListEditorCoordinator.TabListEditorController mTabListEditorController;
    private TabListEditorLayout mTabListEditorLayout;
    private TabListEditorCoordinator mTabListEditorCoordinator;
    private WeakReference<TabListEditorLayout> mRef;
    private ViewGroup mParentView;
    private SnackbarManager mSnackbarManager;
    private BookmarkModel mBookmarkModel;
    private TabGroupCreationDialogManager mCreationDialogManager;
    private AppHeaderCoordinator mAppHeaderStateProvider;
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        // Eagerly inflate the tab switcher.

        boolean isTabSwitcherReady =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return cta.getTabSwitcherSupplierForTesting().get() != null;
                        });
        if (!isTabSwitcherReady) {
            TabUiTestHelper.enterTabSwitcher(cta);
            TabUiTestHelper.leaveTabSwitcher(cta);
        }

        mTabModelSelector = cta.getTabModelSelector();
        mParentView = cta.findViewById(R.id.coordinator);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCreationDialogManager =
                            new TabGroupCreationDialogManager(
                                    cta,
                                    cta.getModalDialogManager(),
                                    /* onTabGroupCreation= */ null);
                    ViewGroup compositorViewHolder = cta.getCompositorViewHolderForTesting();
                    ViewGroup rootView =
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(cta)
                                    ? (ViewGroup) cta.findViewById(R.id.tab_switcher_view_holder)
                                    : compositorViewHolder;
                    mSnackbarManager = new SnackbarManager(cta, rootView, null);
                    var currentTabModelFilterSupplier =
                            mTabModelSelector
                                    .getTabModelFilterProvider()
                                    .getCurrentTabModelFilterSupplier();
                    mAppHeaderStateProvider =
                            (AppHeaderCoordinator)
                                    sActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting()
                                            .getDesktopWindowStateProvider();
                    mEdgeToEdgeSupplier = new ObservableSupplierImpl<>();
                    mTabListEditorCoordinator =
                            new TabListEditorCoordinator(
                                    cta,
                                    mParentView,
                                    mParentView,
                                    cta.getBrowserControlsManager(),
                                    currentTabModelFilterSupplier,
                                    cta.getTabContentManager(),
                                    mSetRecyclerViewPosition,
                                    getMode(),
                                    /* displayGroups= */ true,
                                    mSnackbarManager,
                                    /* bottomSheetController= */ null,
                                    TabProperties.TabActionState.SELECTABLE,
                                    /* gridCardOnClickListenerProvider= */ null,
                                    mModalDialogManager,
                                    mAppHeaderStateProvider,
                                    mEdgeToEdgeSupplier);

                    mTabListEditorController = mTabListEditorCoordinator.getController();
                    mTabListEditorLayout =
                            mTabListEditorCoordinator.getTabListEditorLayoutForTesting();
                    mRef = new WeakReference<>(mTabListEditorLayout);
                    mBookmarkModel = cta.getBookmarkModelForTesting();
                });

        // The inner most callback may check pref service in C++, needs to be on the UI thread.
        Callback<Callback<Integer>> immediateContinue =
                (Callback<Integer> callback) ->
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> callback.onResult(ConfirmationResult.IMMEDIATE_CONTINUE));
        doCallback(1, immediateContinue)
                .when(mActionConfirmationManager)
                .processCloseTabAttempt(any(), any());
    }

    @After
    public void tearDown() {
        if (mTabListEditorCoordinator != null) {
            if (sActivityTestRule.getActivity().findViewById(R.id.app_menu_list) != null) {
                Espresso.pressBack();
            }

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        if (mTabListEditorController.isVisible()) {
                            mTabListEditorController.hide();
                        }
                        mTabListEditorCoordinator.destroy();
                    });

            if (sActivityTestRule
                    .getActivity()
                    .getLayoutManager()
                    .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                TabUiTestHelper.leaveTabSwitcher(sActivityTestRule.getActivity());
            }
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mSnackbarManager == null) return;
                    mSnackbarManager.dismissAllSnackbars();
                });
        BookmarkUtils.clearLastUsedPrefs();
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private void prepareBlankTab(int num, boolean isIncognito) {
        for (int i = 0; i < num - 1; i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(),
                    sActivityTestRule.getActivity(),
                    isIncognito,
                    true);
            sActivityTestRule.loadUrl("about:blank");
        }
    }

    private void createNewTab(@TabLaunchType int launchType, boolean isIncognito) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule
                            .getActivity()
                            .getTabCreator(isIncognito)
                            .createNewTab(new LoadUrlParams("about:blank"), launchType, null);
                });
    }

    private void prepareBlankTabWithThumbnail(int num, boolean isIncognito) {
        if (isIncognito) {
            TabUiTestHelper.prepareTabsWithThumbnail(sActivityTestRule, 0, num, "about:blank");
        } else {
            TabUiTestHelper.prepareTabsWithThumbnail(sActivityTestRule, num, 0, "about:blank");
        }
    }

    private void prepareBlankTabGroup(int num, boolean isIncognito) {
        ArrayList<String> urls = new ArrayList<>(Collections.nCopies(num, "about:blank"));

        prepareTabGroupWithUrls(urls, isIncognito);
    }

    private void prepareTabGroupWithUrls(ArrayList<String> urls, boolean isIncognito) {
        for (String url : urls) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(),
                    sActivityTestRule.getActivity(),
                    isIncognito,
                    true);
            sActivityTestRule.loadUrl(url);
        }
        if (urls.size() == 1) return;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArrayList<Tab> tabs = new ArrayList<>();
                    TabModel model = mTabModelSelector.getCurrentModel();
                    TabGroupModelFilter filter =
                            (TabGroupModelFilter)
                                    mTabModelSelector
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter();
                    for (int i = model.getCount() - urls.size(); i < model.getCount(); i++) {
                        tabs.add(model.getTabAt(i));
                    }
                    // Don't notify to avoid snackbar appearing.
                    filter.mergeListOfTabsToGroup(
                            tabs.subList(1, tabs.size()), tabs.get(0), /* notify= */ false);
                });
    }

    @Test
    @MediumTest
    public void testShowTabs() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));
                    showSelectionEditor(tabs, actions);
                });

        mRobot.resultRobot
                .verifyTabListEditorIsVisible()
                .verifyToolbarActionViewDisabled(R.id.tab_list_editor_group_menu_item)
                .verifyToolbarActionViewWithText(R.id.tab_list_editor_group_menu_item, "Group tabs")
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(1);
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Close tabs", /* enabled= */ false);
        Espresso.pressBack();
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    @Restriction(DeviceFormFactor.TABLET)
    @Feature("DesktopWindow")
    @SmallTest
    public void testMarginWithAppHeaders() {
        // Height to apply as top margin.
        int appHeaderHeight =
                sActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_height);
        Rect windowRect = new Rect();
        sActivityTestRule.getActivity().getWindow().getDecorView().getGlobalVisibleRect(windowRect);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Trigger desktop window - set app headers
                    Rect widestUnoccludedRect =
                            new Rect(windowRect.left, 0, windowRect.right, appHeaderHeight);
                    var state = new AppHeaderState(windowRect, widestUnoccludedRect, true);
                    mAppHeaderStateProvider.setStateForTesting(true, state);
                });

        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);

        mRobot.resultRobot.verifyTabListEditorHasTopMargin(appHeaderHeight);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Exit desktop window.
                    var state = new AppHeaderState(windowRect, new Rect(), false);
                    mAppHeaderStateProvider.setStateForTesting(false, state);
                });

        // Verify margin is reset.
        mRobot.resultRobot.verifyTabListEditorHasTopMargin(0);
    }

    @Test
    @MediumTest
    public void testToggleItem() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs, null);

        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionText("1 tab");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs);
    }

    @Test
    @MediumTest
    public void testSelectItemsThroughActionButton() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs, null);

        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0);

        mRobot.actionRobot.clickActionButtonAdapterPosition(0, getActionButtonId());
        mRobot.resultRobot
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionText("1 tab");

        mRobot.actionRobot.clickActionButtonAdapterPosition(1, getActionButtonId());
        mRobot.resultRobot
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyToolbarSelectionText("2 tabs");

        mRobot.actionRobot.clickActionButtonAdapterPosition(1, getActionButtonId());
        mRobot.resultRobot
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyToolbarSelectionText("1 tab");

        mRobot.actionRobot.clickActionButtonAdapterPosition(0, getActionButtonId());
        mRobot.resultRobot
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs);
    }

    @Test
    @MediumTest
    public void testToolbarNavigationButtonHideTabListEditor() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs, null);

        mRobot.resultRobot.verifyTabListEditorIsVisible();

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        verify(mSetRecyclerViewPosition, times(1)).onResult(isNotNull());
    }

    @Test
    @MediumTest
    public void testHideOnNewTab() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        createNewTab(TabLaunchType.FROM_STARTUP, false);
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        createNewTab(TabLaunchType.FROM_RESTORE, false);
        mRobot.resultRobot.verifyTabListEditorIsHidden();
    }

    @Test
    @MediumTest
    public void testToolbarGroupButtonEnabledState() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));
                    showSelectionEditor(tabs, actions);
                });

        mRobot.resultRobot.verifyToolbarActionViewDisabled(R.id.tab_list_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(R.id.tab_list_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(R.id.tab_list_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarActionViewEnabled(R.id.tab_list_editor_group_menu_item);

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionViewDisabled(R.id.tab_list_editor_group_menu_item);
    }

    @Test
    @MediumTest
    public void testUndoToolbarGroup() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TabUiTestHelper.enterTabSwitcher(cta);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));
                    showSelectionEditor(tabs, actions);
                });

        mRobot.resultRobot.verifyToolbarActionViewDisabled(R.id.tab_list_editor_group_menu_item);

        mRobot.actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionView(R.id.tab_list_editor_group_menu_item);

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 1);

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testConfigureToolbarMenuItems() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        final int closeId = R.id.tab_list_editor_close_menu_item;
        mRobot.resultRobot
                .verifyToolbarActionViewDisabled(closeId)
                .verifyToolbarActionViewWithText(closeId, "Close tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /* enabled= */ false);
        Espresso.pressBack();

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(closeId)
                .verifyToolbarActionViewWithText(closeId, "Close tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /* enabled= */ true);
        Espresso.pressBack();

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot
                .verifyToolbarActionViewDisabled(closeId)
                .verifyToolbarActionViewWithText(closeId, "Close tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot.verifyToolbarMenuItemState("Group tabs", /* enabled= */ false);
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_CloseActionView() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));
                    showSelectionEditor(tabs, actions);
                });

        final int closeId = R.id.tab_list_editor_close_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickToolbarActionView(closeId);

        mRobot.resultRobot.verifyTabListEditorIsHidden();
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));

                    showSelectionEditor(tabs, actions);
                });

        final int closeId = R.id.tab_list_editor_close_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(closeId);

        mRobot.actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(closeId)
                .verifyToolbarSelectionText("5 tabs");

        View close = mTabListEditorLayout.getToolbar().findViewById(closeId);
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        final int groupId = R.id.tab_list_editor_group_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(groupId);

        mRobot.actionRobot
                .clickItemAtAdapterPosition(4)
                .clickItemAtAdapterPosition(3)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(0);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(groupId)
                .verifyToolbarSelectionText("5 tabs");

        View close = mTabListEditorLayout.getToolbar().findViewById(groupId);
        assertEquals("Group 5 selected tabs", close.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(groupId);

        assertEquals(2, getTabsInCurrentTabModelFilter().size());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/1511804")
    public void testToolbarMenuItem_GroupActionAndUndo() throws Exception {
        prepareBlankTabWithThumbnail(2, false);
        prepareBlankTabGroup(3, false);
        prepareBlankTabGroup(1, false);
        prepareBlankTabGroup(2, false);
        TabUiTestHelper.createTabsWithThumbnail(sActivityTestRule, 1, "about:blank", false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();
        List<Tab> beforeTabOrder = getTabsInCurrentTabModel();

        Tab selectedTab = beforeTabOrder.get(4);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModelSelector.getCurrentModel().setIndex(4, TabSelectionType.FROM_USER);
                });
        assertEquals(selectedTab, mTabModelSelector.getCurrentTab());

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        final int groupId = R.id.tab_list_editor_group_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewDisabled(groupId);

        mRobot.actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3)
                .clickItemAtAdapterPosition(4)
                .clickItemAtAdapterPosition(5);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(groupId)
                .verifyToolbarSelectionText("9 tabs");

        View group = mTabListEditorLayout.getToolbar().findViewById(groupId);
        assertEquals("Group 9 selected tabs", group.getContentDescription());

        // Force the position to something fixed to 100% avoid flakes here.
        TabListRecyclerView tabListRecyclerView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            TabListRecyclerView recyclerView =
                                    mTabListEditorLayout.findViewById(R.id.tab_list_recycler_view);
                            recyclerView.scrollToPosition(4);
                            return recyclerView;
                        });

        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);
        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "groups_before_undo_scrolled");

        mRobot.actionRobot.clickToolbarActionView(groupId);

        mRobot.resultRobot.verifyTabListEditorIsHidden();
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tabs");
        Espresso.pressBack();

        assertEquals(2, getTabsInCurrentTabModel().size());

        mRobot.actionRobot
                .clickItemAtAdapterPosition(0)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tab");

        assertEquals(1, getTabsInCurrentTabModel().size());
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionView() {
        Intents.init();
        prepareBlankTab(1, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorShareAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int shareId = R.id.tab_list_editor_share_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewWithText(shareId, "Share tabs");
        mRobot.resultRobot.verifyToolbarActionViewDisabled(shareId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(shareId)
                .verifyToolbarSelectionText("1 tab");

        View share = mTabListEditorLayout.getToolbar().findViewById(shareId);
        assertEquals("Share 1 selected tab", share.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(shareId);

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Share sheet was not shown.",
                                sActivityTestRule
                                        .getActivity()
                                        .getRootUiCoordinatorForTesting()
                                        .getBottomSheetController(),
                                notNullValue()));

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CHOOSER)),
                        hasExtras(
                                hasEntry(
                                        equalTo(Intent.EXTRA_INTENT),
                                        allOf(
                                                hasAction(equalTo(Intent.ACTION_SEND)),
                                                hasType("text/plain"))))));
        Intents.release();
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionTabsOnly() {
        prepareBlankTab(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorShareAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(2);

        TabListEditorShareAction.setIntentCallbackForTesting(
                result -> {
                    assertEquals(Intent.ACTION_SEND, result.getAction());
                    assertEquals(httpsCanonicalUrl, result.getStringExtra(Intent.EXTRA_TEXT));
                    assertEquals("text/plain", result.getType());
                    assertEquals("1 link from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
                });

        final int shareId = R.id.tab_list_editor_share_menu_item;
        mRobot.actionRobot.clickToolbarActionView(shareId);
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionGroupsOnly() {
        ArrayList<String> urls = new ArrayList<>();
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorShareAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot.clickItemAtAdapterPosition(1).clickItemAtAdapterPosition(2);

        TabListEditorShareAction.setIntentCallbackForTesting(
                result -> {
                    assertEquals(Intent.ACTION_SEND, result.getAction());
                    assertEquals(String.join("\n", urls), result.getStringExtra(Intent.EXTRA_TEXT));
                    assertEquals("text/plain", result.getType());
                    assertEquals("4 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
                });

        final int shareId = R.id.tab_list_editor_share_menu_item;
        mRobot.actionRobot.clickToolbarActionView(shareId);
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testToolbarMenuItem_ShareActionTabsWithGroups() {
        prepareBlankTab(2, false);

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        prepareBlankTabGroup(2, false);

        ArrayList<String> urls = new ArrayList<>();
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorShareAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3);

        TabListEditorShareAction.setIntentCallbackForTesting(
                result -> {
                    assertEquals(Intent.ACTION_SEND, result.getAction());
                    assertEquals(String.join("\n", urls), result.getStringExtra(Intent.EXTRA_TEXT));
                    assertEquals("text/plain", result.getType());
                    assertEquals("3 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
                });

        final int shareId = R.id.tab_list_editor_share_menu_item;
        mRobot.actionRobot.clickToolbarActionView(shareId);
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_ShareActionAllFilterableTabs() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorShareAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int shareId = R.id.tab_list_editor_share_menu_item;
        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(1);
        mRobot.resultRobot
                .verifyToolbarActionViewDisabled(shareId)
                .verifyToolbarSelectionText("2 tabs");
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.R, message = "crbug.com/1511804")
    public void testToolbarMenuItem_BookmarkActionSingleTab() {
        prepareBlankTab(1, false);

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorBookmarkAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int bookmarkId = R.id.tab_list_editor_bookmark_menu_item;
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickToolbarActionView(bookmarkId);

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            mBookmarkModel.doesBookmarkExist(
                                    mBookmarkModel.getUserBookmarkIdForTab(tabs.get(0))));

                    Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
                    assertEquals(
                            Snackbar.UMA_BOOKMARK_ADDED, currentSnackbar.getIdentifierForTesting());
                    assertEquals(
                            "Bookmarked to Mobile bookmarks", currentSnackbar.getTextForTesting());
                    currentSnackbar.getController().onAction(null);
                });
        BookmarkEditActivity activity = BookmarkTestUtil.waitForEditActivity();
        activity.finish();

        mRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_BookmarkActionGroupsOnly() {
        prepareBlankTabGroup(2, false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorBookmarkAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int bookmarkId = R.id.tab_list_editor_bookmark_menu_item;
        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickToolbarActionView(bookmarkId);

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Tab tab : tabs) {
                        assertTrue(
                                mBookmarkModel.doesBookmarkExist(
                                        mBookmarkModel.getUserBookmarkIdForTab(tab)));
                    }
                    Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
                    assertEquals(
                            Snackbar.UMA_BOOKMARK_ADDED, currentSnackbar.getIdentifierForTesting());
                    assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
                });

        mRobot.resultRobot.verifyTabListEditorIsVisible();
        ThreadUtils.runOnUiThreadBlocking(() -> mTabListEditorController.handleBackPressed());
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSnackbarManager.dismissSnackbars(
                                mSnackbarManager.getCurrentSnackbarForTesting().getController()));
    }

    @Test
    @MediumTest
    public void testToolbarMenuItem_BookmarkActionTabsWithGroups() {
        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        ArrayList<String> urls = new ArrayList<>();
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTP_CANONICAL_URL));
        urls.add(sActivityTestRule.getTestServer().getURL(PAGE_WITH_NO_CANONICAL_URL));

        prepareTabGroupWithUrls(urls, false);
        List<Tab> tabs = getTabsInCurrentTabModelFilter();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorBookmarkAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int bookmarkId = R.id.tab_list_editor_bookmark_menu_item;
        mRobot.resultRobot.verifyToolbarActionViewWithText(bookmarkId, "Bookmark tabs");
        mRobot.resultRobot.verifyToolbarActionViewDisabled(bookmarkId);

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(1);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(bookmarkId)
                .verifyToolbarSelectionText("3 tabs");

        View bookmark = mTabListEditorLayout.getToolbar().findViewById(bookmarkId);
        assertEquals("Bookmark 3 selected tabs", bookmark.getContentDescription());

        mRobot.actionRobot.clickToolbarActionView(bookmarkId);

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
                    assertEquals(
                            Snackbar.UMA_BOOKMARK_ADDED, currentSnackbar.getIdentifierForTesting());
                    assertEquals("Bookmarked", currentSnackbar.getTextForTesting());
                    currentSnackbar.getController().onAction(null);
                });

        BookmarkEditActivity activity = BookmarkTestUtil.waitForEditActivity();
        activity.finish();

        mRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testSelectionAction_IndividualTabSelection() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorSelectionAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int selectionId = R.id.tab_list_editor_selection_menu_item;
        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickItemAtAdapterPosition(1);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Deselect all");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.resultRobot.verifyTabListEditorIsVisible();

        TabListRecyclerView tabListRecyclerView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mTabListEditorLayout.findViewById(R.id.tab_list_recycler_view);
                        });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "grid_view_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testGridViewAppearance_oneSelectedTab() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabListEditorIsVisible();

        TabListRecyclerView tabListRecyclerView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mTabListEditorLayout.findViewById(R.id.tab_list_recycler_view);
                        });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "grid_view_v2_one_selected_tab_0.85");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testSelectionAction_Toggle() throws IOException {
        prepareBlankTabWithThumbnail(3, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        // Enter tab switcher to get all thumbnails.
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                sActivityTestRule.getActivity().getCurrentTabModel());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorSelectionAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END));

                    showSelectionEditor(tabs, actions);
                });

        final int selectionId = R.id.tab_list_editor_selection_menu_item;
        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all");

        mRobot.actionRobot.clickToolbarActionView(selectionId);
        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Deselect all")
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyItemSelectedAtAdapterPosition(2)
                .verifyToolbarSelectionText("3 tabs");
        TabListRecyclerView tabListRecyclerView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mTabListEditorLayout.findViewById(R.id.tab_list_recycler_view);
                        });
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "selection_action_all_tabs_selected");

        mRobot.actionRobot.clickToolbarActionView(selectionId);
        mRobot.resultRobot
                .verifyToolbarActionViewEnabled(selectionId)
                .verifyToolbarActionViewWithText(selectionId, "Select all")
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyItemNotSelectedAtAdapterPosition(2)
                .verifyToolbarSelectionText("Select tabs");
        TabUiTestHelper.waitForThumbnailsToFetch(tabListRecyclerView);

        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "selection_action_all_tabs_deselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListViewAppearance() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.resultRobot.verifyTabListEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "list_view");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListViewV2Shows() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs, null);

        mRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListViewAppearance_oneSelectedTab() throws IOException {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRobot.resultRobot.verifyTabListEditorIsVisible();

        ChromeRenderTestRule.sanitize(mTabListEditorLayout);
        mRenderTestRule.render(mTabListEditorLayout, "list_view_one_selected_tab");
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testListView_select() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot
                .verifyToolbarActionViewDisabled(R.id.tab_list_editor_group_menu_item)
                .verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testTabListEditorLayoutCanBeGarbageCollected() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorCoordinator.destroy();
                    mTabListEditorCoordinator = null;
                    mTabListEditorLayout = null;
                    mTabListEditorController = null;
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // A longer timeout is needed. Achieve that by using the CriteriaHelper.pollUiThread.
        CriteriaHelper.pollUiThread(() -> GarbageCollectionTestUtils.canBeGarbageCollected(mRef));
    }

    @Test
    @MediumTest
    public void testSelectionTabAccessibilityChecked() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        // Test deselected tab
        View tabView =
                mTabListEditorCoordinator
                        .getTabListRecyclerViewForTesting()
                        .findViewHolderForAdapterPosition(0)
                        .itemView;
        assertFalse(tabView.createAccessibilityNodeInfo().isChecked());

        // Test selected tab
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        assertTrue(tabView.createAccessibilityNodeInfo().isChecked());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
    public void testEdgeToEdgePadAdjuster() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);

        TabListRecyclerView tabListRecyclerView =
                mTabListEditorCoordinator.getTabListRecyclerViewForTesting();
        int originalPaddingBottom = tabListRecyclerView.getPaddingBottom();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
                });

        EdgeToEdgePadAdjuster padAdjuster =
                mTabListEditorCoordinator.getEdgeToEdgePadAdjusterForTesting();
        assertNotNull("Pad adjuster should be created when feature enabled.", padAdjuster);
        verify(mEdgeToEdgeController).registerAdjuster(eq(padAdjuster));

        int bottomEdgeToEdgePadding = 60;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    padAdjuster.overrideBottomInset(bottomEdgeToEdgePadding);
                });
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "The tab list recycler view was not padded to account for"
                                        + " edge-to-edge.",
                                tabListRecyclerView.getPaddingBottom(),
                                Matchers.equalTo(originalPaddingBottom + bottomEdgeToEdgePadding)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    padAdjuster.overrideBottomInset(0);
                });
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "The additional edge-to-edge padding to the tab list recycler view"
                                        + " was not properly cleared.",
                                tabListRecyclerView.getPaddingBottom(),
                                Matchers.equalTo(originalPaddingBottom)));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testToolbarMenuItem_SelectAllMenu() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorSelectionAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START));
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));

                    showSelectionEditor(tabs, actions);
                });
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Select all", /* enabled= */ true)
                .verifyToolbarMenuItemState("Close tabs", /* enabled= */ false);
        mRobot.actionRobot.clickToolbarMenuItem("Select all");
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Deselect all", /* enabled= */ true)
                .verifyToolbarMenuItemState("Close tabs", /* enabled= */ true);
        mRobot.actionRobot.clickToolbarMenuItem("Deselect all");
        mRobot.resultRobot.verifyToolbarMenuItemState("Select all", /* enabled= */ true);
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testToolbarActionViewAndMenuItemContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<TabListEditorAction> actions = new ArrayList<>();
                    actions.add(
                            TabListEditorCloseAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    mActionConfirmationManager));
                    actions.add(
                            TabListEditorGroupAction.createAction(
                                    sActivityTestRule.getActivity(),
                                    mCreationDialogManager,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START));

                    showSelectionEditor(tabs, actions);
                });
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        final int closeId = R.id.tab_list_editor_close_menu_item;
        View close = mTabListEditorLayout.getToolbar().findViewById(closeId);
        assertNull(close.getContentDescription());
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Group tabs", /* enabled= */ false)
                .verifyToolbarMenuItemWithContentDescription("Group tabs", null);
        Espresso.pressBack();

        for (int i = 0; i < tabs.size(); i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        assertEquals("Close 2 selected tabs", close.getContentDescription());
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Group tabs", /* enabled= */ true)
                .verifyToolbarMenuItemWithContentDescription("Group tabs", "Group 2 selected tabs");
        Espresso.pressBack();

        mRobot.actionRobot.clickItemAtAdapterPosition(0).clickToolbarMenuButton();
        assertEquals("Close 1 selected tab", close.getContentDescription());
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Group tab", /* enabled= */ true)
                .verifyToolbarMenuItemWithContentDescription("Group tab", "Group 1 selected tab");
        Espresso.pressBack();
    }

    // This is a regression test for crbug.com/1132478.
    @Test
    @MediumTest
    public void testTabListEditorContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        assertEquals("Multi-select mode", mTabListEditorLayout.getContentDescription());
    }

    @Test
    @MediumTest
    public void testToolbarNavigationButtonContentDescription() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        assertEquals(
                "Hide multi-select mode",
                mTabListEditorLayout.getToolbar().getNavigationContentDescription());
    }

    @Test
    @MediumTest
    public void testEditorHideCorrectly() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();

        ThreadUtils.runOnUiThreadBlocking(() -> mTabListEditorController.handleBackPressed());
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testBackgroundViewAccessibilityImportance() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        Map<View, Integer> initialValues = getParentViewAccessibilityImportanceMap();

        showSelectionEditor(tabs, null);
        mRobot.resultRobot.verifyTabListEditorIsVisible();
        ViewGroup parentView = (ViewGroup) mTabListEditorLayout.getParent();
        verifyBackgroundViewAccessibilityImportance(parentView, true, initialValues);

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        verifyBackgroundViewAccessibilityImportance(parentView, false, initialValues);
    }

    @Test
    @MediumTest
    public void testMoveToClosableState() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        showSelectionEditor(tabs, null);

        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0);

        mRobot.actionRobot.clickActionButtonAdapterPosition(0, getActionButtonId());
        mRobot.resultRobot
                .verifyAdapterHasItemCount(2)
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionText("1 tab");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.setTabActionState(TabActionState.CLOSABLE);
                });

        mRobot.resultRobot
                .verifyAdapterHasItemCount(2)
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionText("Select tabs");

        mRobot.actionRobot.clickActionButtonAdapterPosition(0, getActionButtonId());
        mRobot.resultRobot.verifyAdapterHasItemCount(1).verifyItemNotSelectedAtAdapterPosition(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.setTabActionState(TabActionState.SELECTABLE);
                });
        mRobot.actionRobot.clickActionButtonAdapterPosition(0, getActionButtonId());
        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionText("1 tab");
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

    private void verifyBackgroundViewAccessibilityImportance(
            ViewGroup parentView,
            boolean isTabListEditorShowing,
            Map<View, Integer> initialValues) {
        assertEquals(
                isTabListEditorShowing
                        ? IMPORTANT_FOR_ACCESSIBILITY_NO
                        : initialValues.get(parentView).intValue(),
                parentView.getImportantForAccessibility());

        for (int i = 0; i < parentView.getChildCount(); i++) {
            View view = parentView.getChildAt(i);
            int expected =
                    isTabListEditorShowing
                            ? IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            : initialValues.get(view).intValue();
            if (view == mTabListEditorLayout) {
                expected = IMPORTANT_FOR_ACCESSIBILITY_YES;
            }

            assertEquals(expected, view.getImportantForAccessibility());
        }
    }

    /** Retrieves all tabs from the current tab model */
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
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        for (int i = 0; i < filter.getCount(); i++) {
            tabs.add(filter.getTabAt(i));
        }

        return tabs;
    }

    private void showSelectionEditor(List<Tab> tabs, @Nullable List<TabListEditorAction> actions) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.show(tabs, /* recyclerViewPosition= */ null);
                    if (actions != null) {
                        mTabListEditorController.configureToolbarWithMenuItems(actions);
                    }
                });
    }

    private @IdRes int getActionButtonId() {
        if (getMode() == TabListCoordinator.TabListMode.GRID) {
            return R.id.action_button;
        } else {
            return R.id.end_button;
        }
    }
}
