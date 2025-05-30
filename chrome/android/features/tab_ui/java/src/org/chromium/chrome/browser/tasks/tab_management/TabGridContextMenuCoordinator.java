// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
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
 * A coordinator for the context menu accessed by long-pressing on a tab. It is responsible for
 * creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class TabGridContextMenuCoordinator extends TabOverflowMenuCoordinator<@TabId Integer> {
    /** An interface to show the TabListEditor. */
    @FunctionalInterface
    public interface ShowTabListEditor {
        /**
         * Show the TabListEditor.
         *
         * @param tabId The tab ID of the tab that the context menu was shown for.
         */
        void show(@TabId int tabId);
    }

    private static final String MENU_USER_ACTION_PREFIX = "TabSwitcher.ContextMenu.";
    private final Activity mActivity;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final BookmarkModel mBookmarkModel;

    TabGridContextMenuCoordinator(
            Activity activity,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            Profile profile,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            ShowTabListEditor showTabListEditor) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        tabBookmarkerSupplier,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        tabGroupCreationDialogManager,
                        shareDelegateSupplier,
                        showTabListEditor),
                tabGroupModelFilter::getTabModel,
                tabGroupSyncService,
                collaborationService,
                activity);
        mActivity = activity;
        mTabGroupModelFilter = tabGroupModelFilter;
        mBookmarkModel = BookmarkModel.getForProfile(profile);
    }

    /**
     * @param activity The activity where this context menu will be shown.
     * @param tabBookmarkerSupplier Supplies the {@link TabBookmarker} used to bookmark tabs.
     * @param tabGroupModelFilter Supplies the {@link TabModel}.
     * @param tabGroupListBottomSheetCoordinator The {@link TabGroupListBottomSheetCoordinator} that
     *     will be used to show a bottom sheet when the user selects the "Add to group" option.
     * @param tabGroupCreationDialogManager The manager for the dialog showed on tab group creation.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param showTabListEditor Shows the Tab List Editor.
     */
    public static TabGridContextMenuCoordinator createContextMenuCoordinator(
            Activity activity,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            ShowTabListEditor showTabListEditor) {
        Profile profile = assumeNonNull(tabGroupModelFilter.getTabModel().getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabGridContextMenuCoordinator(
                activity,
                tabBookmarkerSupplier,
                profile,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                tabGroupCreationDialogManager,
                shareDelegateSupplier,
                tabGroupSyncService,
                collaborationService,
                showTabListEditor);
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
                /* animStyle= */ ResourcesCompat.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mActivity,
                /* isIncognito= */ false);
        recordUserActionWithPrefix("Shown");
    }

    @VisibleForTesting
    static OnItemClickedCallback<Integer> getMenuItemClickedCallback(
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator coordinator,
            TabGroupCreationDialogManager dialogManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            ShowTabListEditor showTabListEditor) {
        return (menuId, tabId, collaborationId, listViewTouchTracker) -> {
            if (tabId == Tab.INVALID_TAB_ID) return;
            TabModel tabModel = tabGroupModelFilter.getTabModel();
            TabBookmarker tabBookmarker = tabBookmarkerSupplier.get();
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
                recordUserActionWithPrefix(
                        tab.getTabGroupId() == null ? "AddToGroup" : "MoveToGroup");
            } else if (menuId == R.id.edit_bookmark) {
                tabBookmarker.addOrEditBookmark(tab);
                recordUserActionWithPrefix("EditBookmark");
            } else if (menuId == R.id.add_to_bookmarks) {
                tabBookmarker.addOrEditBookmark(tab);
                recordUserActionWithPrefix("AddBookmark");
            } else if (menuId == R.id.select_tabs) {
                showTabListEditor.show(tab.getId());
                recordUserActionWithPrefix("SelectTabs");
            } else if (menuId == R.id.close_tab) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                tabModel.getTabRemover()
                        .closeTabs(
                                TabClosureParams.closeTab(tab).allowUndo(allowUndo).build(),
                                /* allowDialog= */ true);
                recordUserActionWithPrefix("CloseTab");
            }
        };
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer id) {
        @Nullable Tab tab = getTabById(mTabGroupModelFilter::getTabModel, id);
        if (tab == null) return;

        if (ShareUtils.shouldEnableShare(tab)) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.share, R.id.share_tab, R.drawable.tab_list_editor_share_icon));
        }

        if (mTabGroupModelFilter.getTabGroupCount() == 0) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.menu_add_tab_to_new_group,
                            R.id.add_to_new_tab_group,
                            R.drawable.ic_widgets));
        } else {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            tab.getTabGroupId() == null
                                    ? R.string.menu_add_tab_to_group
                                    : R.string.menu_move_tab_to_group,
                            R.id.add_to_tab_group,
                            R.drawable.ic_widgets));
        }

        if (mBookmarkModel.hasBookmarkIdForTab(tab)) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.edit_bookmark,
                            R.id.edit_bookmark,
                            R.drawable.btn_star_filled));
        } else {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.add_to_bookmarks,
                            R.id.add_to_bookmarks,
                            R.drawable.star_outline_24dp));
        }

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.select_tab, R.id.select_tabs, R.drawable.ic_edit_24dp));

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.close_tab, R.id.close_tab, R.drawable.material_ic_close_24dp));
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return getDimensionPixelSize(
                mTabGroupModelFilter.getTabGroupCount() == 0
                        ? R.dimen.tab_grid_context_menu_extended_width
                        : R.dimen.tab_grid_context_menu_max_width);
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
