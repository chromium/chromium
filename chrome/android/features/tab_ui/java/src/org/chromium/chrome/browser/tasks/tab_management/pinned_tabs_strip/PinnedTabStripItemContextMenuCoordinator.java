// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.Resources;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.List;
import java.util.function.Supplier;

/**
 * A coordinator for the overflow menu accessed by long-pressing on a pinned tab. It is responsible
 * for creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class PinnedTabStripItemContextMenuCoordinator
        extends TabOverflowMenuCoordinator<@TabId Integer> {
    private static final String MENU_USER_ACTION_PREFIX = "TabSwitcher.PinnedTabs.ContextMenu.";
    private final Activity mActivity;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final BookmarkModel mBookmarkModel;

    /**
     * Constructs a new {@link PinnedTabStripItemContextMenuCoordinator}.
     *
     * @param activity The {@link Activity} context.
     * @param profile The {@link Profile} for the current tab model.
     * @param tabBookmarkerSupplier The supplier for a {@link TabBookmarker} instance.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to get the tab model.
     * @param tabGroupListBottomSheetCoordinator The coordinator for the bottom sheet to move tabs.
     * @param tabGroupCreationDialogManager The manager for the dialog to create a new tab group.
     * @param tabGroupSyncService The {@link TabGroupSyncService} to handle tab group sync, may be
     *     null.
     * @param collaborationService The {@link CollaborationService} for handling collaboration.
     */
    PinnedTabStripItemContextMenuCoordinator(
            Activity activity,
            Profile profile,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        tabBookmarkerSupplier,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        tabGroupCreationDialogManager),
                tabGroupModelFilter::getTabModel,
                /* multiInstanceManager= */ null,
                tabGroupSyncService,
                collaborationService,
                activity);
        mActivity = activity;
        mTabGroupModelFilter = tabGroupModelFilter;
        mBookmarkModel = BookmarkModel.getForProfile(profile);
    }

    /**
     * Show the overflow menu of the tab.
     *
     * @param anchorViewRectProvider The menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabId The tab id of the interacting tab.
     */
    public void showMenu(RectProvider anchorViewRectProvider, int tabId) {
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();
        createAndShowMenu(
                anchorViewRectProvider,
                tabId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mActivity,
                isIncognito);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer tabId) {
        // TODO(crbug.com/445195867): Refactor to extract common code with
        // TabGridContextMenuCoordinator.
        @Nullable Tab tab = getTabById(mTabGroupModelFilter::getTabModel, tabId);
        if (tab == null) return;
        boolean isIncognito = tab.isIncognitoBranded();

        itemList.add(buildGroupItem(tab, isIncognito));
        itemList.add(buildBookmarkItem(tab, isIncognito));
        itemList.add(buildTogglePinStateItem(tab));
        itemList.add(buildCloseTabItem(isIncognito));
    }

    @Nullable
    @Override
    protected String getCollaborationIdOrNull(Integer id) {
        return null;
    }

    private ListItem buildGroupItem(Tab tab, boolean isIncognito) {
        if (mTabGroupModelFilter.getTabGroupCount() == 0) {
            return new ListItemBuilder()
                    .withTitleRes(R.string.menu_add_tab_to_new_group)
                    .withMenuId(R.id.add_to_new_tab_group)
                    .withStartIconRes(R.drawable.ic_widgets)
                    .withIsIncognito(isIncognito)
                    .build();
        } else {
            @StringRes
            int title =
                    tab.getTabGroupId() == null
                            ? R.string.menu_add_tab_to_group
                            : R.string.menu_move_tab_to_group;
            return new ListItemBuilder()
                    .withTitleRes(title)
                    .withMenuId(R.id.add_to_tab_group)
                    .withStartIconRes(R.drawable.ic_widgets)
                    .withIsIncognito(isIncognito)
                    .build();
        }
    }

    private ListItem buildBookmarkItem(Tab tab, boolean isIncognito) {
        if (mBookmarkModel.hasBookmarkIdForTab(tab)) {
            return new ListItemBuilder()
                    .withTitleRes(R.string.edit_bookmark)
                    .withMenuId(R.id.edit_bookmark)
                    .withStartIconRes(R.drawable.btn_star_filled)
                    .withIsIncognito(isIncognito)
                    .build();
        } else {
            return new ListItemBuilder()
                    .withTitleRes(R.string.add_to_bookmarks)
                    .withMenuId(R.id.add_to_bookmarks)
                    .withStartIconRes(R.drawable.star_outline_24dp)
                    .withIsIncognito(isIncognito)
                    .build();
        }
    }

    private ListItem buildCloseTabItem(boolean isIncognito) {
        return new ListItemBuilder()
                .withTitleRes(R.string.close_tab)
                .withMenuId(R.id.close_tab)
                .withStartIconRes(R.drawable.material_ic_close_24dp)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem buildTogglePinStateItem(Tab tab) {
        @StringRes int titleRes = R.string.unpin_tab;
        @IdRes int menuId = R.id.unpin_tab;
        return new ListItemBuilder()
                .withTitleRes(titleRes)
                .withMenuId(menuId)
                .withStartIconRes(R.drawable.ic_keep_off_24dp)
                .withIsIncognito(tab.isIncognitoBranded())
                .build();
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.tab_grid_context_menu_max_width);
    }

    @VisibleForTesting
    static OnItemClickedCallback<Integer> getMenuItemClickedCallback(
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator coordinator,
            TabGroupCreationDialogManager dialogManager) {
        return (menuId, tabId, collaborationId, listViewTouchTracker) -> {
            if (tabId == Tab.INVALID_TAB_ID) return;
            TabModel tabModel = tabGroupModelFilter.getTabModel();
            TabBookmarker tabBookmarker = tabBookmarkerSupplier.get();
            @Nullable Tab tab = getTabById(() -> tabModel, tabId);
            if (tab == null) return;

            if (menuId == R.id.add_to_new_tab_group) {
                tabGroupModelFilter.createSingleTabGroup(tab);
                Token groupId = assumeNonNull(tab.getTabGroupId());
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
            } else if (menuId == R.id.unpin_tab) {
                tabModel.unpinTab(tab.getId());
                recordUserActionWithPrefix("UnpinTab");
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

    public static PinnedTabStripItemContextMenuCoordinator createContextMenuCoordinator(
            Activity activity,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationDialogManager tabGroupCreationDialogManager) {
        Profile profile = assumeNonNull(tabGroupModelFilter.getTabModel().getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);
        return new PinnedTabStripItemContextMenuCoordinator(
                activity,
                profile,
                tabBookmarkerSupplier,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                tabGroupCreationDialogManager,
                tabGroupSyncService,
                collaborationService);
    }

    private static void recordUserActionWithPrefix(String action) {
        RecordUserAction.record(MENU_USER_ACTION_PREFIX + action);
    }

    private static @Nullable Tab getTabById(Supplier<TabModel> tabModelSupplier, @TabId int tabId) {
        return tabModelSupplier.get().getTabById(tabId);
    }
}
