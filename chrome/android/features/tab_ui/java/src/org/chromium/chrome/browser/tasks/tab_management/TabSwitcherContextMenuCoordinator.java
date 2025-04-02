// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.List;

/**
 * A coordinator for the context menu in the tab switcher accessed by long-pressing on a tab. It is
 * responsible for creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class TabSwitcherContextMenuCoordinator extends TabOverflowMenuCoordinator<@TabId Integer> {
    private static final String MENU_USER_ACTION_PREFIX = "TabSwitcher.ContextMenu";
    private final Activity mActivity;
    private final TabGroupModelFilter mTabGroupModelFilter;

    TabSwitcherContextMenuCoordinator(
            Activity activity,
            TabBookmarker tabBookmarker,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            TabListEditorManager tabListEditorManager) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        tabBookmarker,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        tabGroupCreationDialogManager,
                        shareDelegateSupplier,
                        tabListEditorManager),
                tabGroupModelFilter::getTabModel,
                tabGroupSyncService,
                collaborationService,
                activity);
        mActivity = activity;
        mTabGroupModelFilter = tabGroupModelFilter;
    }

    /**
     * @param activity The activity where this context menu will be shown.
     * @param tabBookmarker Used to bookmark tabs.
     * @param tabGroupModelFilter Supplies the {@link TabModel}.
     * @param tabGroupListBottomSheetCoordinator The {@link TabGroupListBottomSheetCoordinator} that
     *     will be used to show a bottom sheet when the user selects the "Add to group" option.
     * @param tabGroupCreationDialogManager The manager for the dialog showed on tab group creation.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param tabListEditorManager Manages the Tab List Editor.
     */
    public static TabSwitcherContextMenuCoordinator createContextMenuCoordinator(
            Activity activity,
            TabBookmarker tabBookmarker,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            TabListEditorManager tabListEditorManager) {
        Profile profile = assumeNonNull(tabGroupModelFilter.getTabModel().getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabSwitcherContextMenuCoordinator(
                activity,
                tabBookmarker,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                tabGroupCreationDialogManager,
                shareDelegateSupplier,
                tabGroupSyncService,
                collaborationService,
                tabListEditorManager);
    }

    /**
     * Show the context menu of the tab group.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabId The tab id of the interacting tab group.
     */
    public void showMenu(RectProvider anchorViewRectProvider, int tabId) {
        createAndShowMenu(
                anchorViewRectProvider,
                tabId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ R.style.TabSwitcherContextMenuAnimation,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mActivity);
        recordUserActionWithPrefix("Shown");
    }

    @VisibleForTesting
    static OnItemClickedCallback<Integer> getMenuItemClickedCallback(
            TabBookmarker tabBookmarker,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator coordinator,
            TabGroupCreationDialogManager dialogManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            TabListEditorManager tabListEditorManager) {
        return (menuId, tabId, collaborationId) -> {
            if (tabId == Tab.INVALID_TAB_ID) return;
            TabModel tabModel = tabGroupModelFilter.getTabModel();
            @Nullable Tab tab = getTabById(() -> tabModel, tabId);
            if (tab == null) return;

            if (menuId == R.id.share_tab) {
                shareDelegateSupplier
                        .get()
                        .share(tab, /* shareDirectly= */ false, TAB_STRIP_CONTEXT_MENU);
                recordUserActionWithPrefix("ShareTab");
            } else if (menuId == R.id.add_to_new_tab_group) {
                tabGroupModelFilter.createSingleTabGroup(tab);
                Token groupId = tab.getTabGroupId();
                dialogManager.showDialog(groupId, tabGroupModelFilter);
                recordUserActionWithPrefix("AddToNewGroup");
            } else if (menuId == R.id.add_to_tab_group) {
                coordinator.showBottomSheet(List.of(tab));
                recordUserActionWithPrefix("AddToGroup");
            } else if (menuId == R.id.add_to_bookmarks) {
                tabBookmarker.addOrEditBookmark(tab);
                recordUserActionWithPrefix("Bookmark");
            } else if (menuId == R.id.select_tabs) {
                tabListEditorManager.showTabListEditor();
                recordUserActionWithPrefix("SelectTabs");
            } else if (menuId == R.id.close_tab) {
                tabModel.getTabRemover()
                        .closeTabs(TabClosureParams.closeTab(tab).build(), /* allowDialog= */ true);
                recordUserActionWithPrefix("CloseTab");
            }
        };
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer id) {
        @Nullable Tab tab = getTabById(mTabGroupModelFilter::getTabModel, id);
        if (tab == null) return;

        if (mTabGroupModelFilter.getTabGroupCount() == 0) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.menu_add_to_new_group,
                            R.id.add_to_new_tab_group,
                            R.drawable.ic_widgets));
        } else {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.add_tab_to_group,
                            R.id.add_to_tab_group,
                            R.drawable.ic_widgets));
        }

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.add_to_bookmarks,
                        R.id.add_to_bookmarks,
                        R.drawable.star_outline_24dp));

        if (ShareUtils.shouldEnableShare(tab)) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.share, R.id.share_tab, R.drawable.tab_list_editor_share_icon));
        }

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.menu_select_tabs, R.id.select_tabs, R.drawable.ic_edit_24dp));

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.close_tab, R.id.close_tab, R.drawable.material_ic_close_24dp));
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return getDimensionPixelSize(R.dimen.tab_switcher_context_menu_max_width);
    }

    @Nullable
    @Override
    protected String getCollaborationIdOrNull(Integer id) {
        @Nullable Tab tab = getTabById(mTabGroupModelFilter::getTabModel, id);
        if (tab == null) return null;
        return TabShareUtils.getCollaborationIdOrNull(tab.getTabGroupId(), mTabGroupSyncService);
    }

    @Nullable
    private static Tab getTabById(Supplier<TabModel> tabModelSupplier, Integer tabId) {
        return tabModelSupplier.get().getTabById(tabId);
    }

    private static void recordUserActionWithPrefix(String action) {
        RecordUserAction.record(MENU_USER_ACTION_PREFIX + action);
    }
}
