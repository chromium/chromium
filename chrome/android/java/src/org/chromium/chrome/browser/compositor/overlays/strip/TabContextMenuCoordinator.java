// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;

import androidx.annotation.DimenRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.List;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on a tab. It is responsible
 * for creating a list of menu items, setting up the menu, and displaying the menu.
 */
public class TabContextMenuCoordinator extends TabOverflowMenuCoordinator<Integer> {
    private static final String MENU_USER_ACTION_PREFIX = "MobileToolbarTabMenu.";
    private final Supplier<TabModel> mTabModelSupplier;
    private final WindowAndroid mWindowAndroid;

    private TabContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        tabModelSupplier,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        shareDelegateSupplier),
                tabModelSupplier,
                tabGroupSyncService,
                collaborationService);
        mTabModelSupplier = tabModelSupplier;
        mWindowAndroid = windowAndroid;
    }

    /**
     * Creates the TabContextMenuCoordinator object.
     *
     * @param tabModelSupplier Supplies the {@link TabModel}.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param tabGroupListBottomSheetCoordinator The {@link TabGroupListBottomSheetCoordinator} that
     *     will be used to show a bottom sheet when the user selects the "Add to group" option.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param windowAndroid The {@link WindowAndroid} where this context menu will be shown.
     */
    public static TabContextMenuCoordinator createContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid) {
        Profile profile = tabModelSupplier.get().getProfile();
        @Nullable
        TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);
        @NonNull
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabContextMenuCoordinator(
                tabModelSupplier,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                shareDelegateSupplier,
                windowAndroid,
                tabGroupSyncService,
                collaborationService);
    }

    @VisibleForTesting
    static OnItemClickedCallback<Integer> getMenuItemClickedCallback(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        return (menuId, tabId, collaborationId) -> {
            if (tabId == Tab.INVALID_TAB_ID) return;
            TabModel tabModel = tabModelSupplier.get();
            Tab tab = tabModel.getTabById(tabId);
            if (tab == null) return;

            if (menuId == R.id.add_to_tab_group) {
                tabGroupListBottomSheetCoordinator.showBottomSheet(List.of(tab));
                recordUserAction("AddToTabGroup");
            } else if (menuId == R.id.remove_from_tab_group) {
                tabGroupModelFilter
                        .getTabUngrouper()
                        .ungroupTabs(List.of(tab), /* trailing= */ true, /* allowDialog= */ true);
                recordUserAction("RemoveFromTabGroup");
            } else if (menuId == R.id.share_tab) {
                shareDelegateSupplier
                        .get()
                        .share(tab, /* shareDirectly= */ false, TAB_STRIP_CONTEXT_MENU);
                recordUserAction("ShareTab");
            } else if (menuId == R.id.close_tab) {
                tabModel.getTabRemover()
                        .closeTabs(TabClosureParams.closeTab(tab).build(), /* allowDialog= */ true);
                recordUserAction("CloseTab");
            }
        };
    }

    /**
     * Show the context menu of the tab group.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabId The tab id of the interacting tab group.
     */
    protected void showMenu(RectProvider anchorViewRectProvider, int tabId) {
        createAndShowMenu(
                anchorViewRectProvider,
                tabId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ ResourcesCompat.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mWindowAndroid.getActivity().get());
        recordUserAction("Shown");
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer id) {
        var tab = mTabModelSupplier.get().getTabById(id);
        if (tab == null) return;

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.add_tab_to_group, R.id.add_to_tab_group, /* startIconId= */ 0));

        if (tab.getTabGroupId() != null) {
            // Show the option to remove the tab from its group iff the tab is already in a group.
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.remove_tab_from_group,
                            R.id.remove_from_tab_group,
                            /* startIconId= */ 0));
        }

        if (ShareUtils.shouldEnableShare(tab)) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.share, R.id.share_tab, /* startIconId= */ 0));
        }

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.close_tab, R.id.close_tab, /* startIconId= */ 0));
    }

    @Override
    protected @DimenRes int getMenuWidth() {
        return R.dimen.tab_strip_group_context_menu_max_width; // TODO(crbug.com/397247439): Fix.
    }

    @Nullable
    @Override
    protected String getCollaborationIdOrNull(Integer id) {
        var tab = mTabModelSupplier.get().getTabById(id);
        if (tab == null) return null;
        return TabShareUtils.getCollaborationIdOrNull(tab.getTabGroupId(), mTabGroupSyncService);
    }

    private static void recordUserAction(String action) {
        RecordUserAction.record(MENU_USER_ACTION_PREFIX + action);
    }
}
