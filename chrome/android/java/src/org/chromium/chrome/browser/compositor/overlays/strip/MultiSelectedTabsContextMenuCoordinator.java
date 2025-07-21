// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;

import android.content.res.Resources;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on a multi selected tab. It
 * is responsible for creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class MultiSelectedTabsContextMenuCoordinator
        extends TabOverflowMenuCoordinator<List<Integer>> {
    private final TabModel mTabModel;
    private final WindowAndroid mWindowAndroid;
    private final TabGroupModelFilter mTabGroupModelFilter;

    private MultiSelectedTabsContextMenuCoordinator(
            TabModel tabModel,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        () -> tabModel,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        multiInstanceManager),
                () -> tabModel,
                tabGroupSyncService,
                collaborationService,
                windowAndroid.getActivity().get());
        mTabModel = tabModel;
        mWindowAndroid = windowAndroid;
        mTabGroupModelFilter = tabGroupModelFilter;
    }

    /**
     * Creates MultiSelectedTabsContextMenuCoordinator object.
     *
     * @param tabModel {@link TabModel}.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param tabGroupListBottomSheetCoordinator The {@link TabGroupListBottomSheetCoordinator} that
     *     will be used to show a bottom sheet when the user selects the "Add to group" option.
     * @param multiInstanceManager The {@link MultiInstanceManager} that will be used to move tabs
     *     from one window to another.
     * @param windowAndroid The {@link WindowAndroid} where this context menu will be shown.
     */
    public static MultiSelectedTabsContextMenuCoordinator createContextMenuCoordinator(
            TabModel tabModel,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid) {
        Profile profile = assumeNonNull(tabModel.getProfile());

        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new MultiSelectedTabsContextMenuCoordinator(
                tabModel,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                multiInstanceManager,
                windowAndroid,
                tabGroupSyncService,
                collaborationService);
    }

    @VisibleForTesting
    static OnItemClickedCallback<List<Integer>> getMenuItemClickedCallback(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager) {
        return (menuId, tabIds, collaborationId, listViewTouchTracker) -> {
            assert !tabIds.isEmpty() : "Empty tab id list provided";
            TabModel tabModel = tabModelSupplier.get();
            List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, false);
            List<Tab> groupedTabs = getGroupedTabs(tabGroupModelFilter, tabs);

            if (menuId == R.id.add_to_tab_group) {
                // The bottom sheet will handle ungrouping any grouped tabs.
                tabGroupListBottomSheetCoordinator.showBottomSheet(tabs);
            } else if (menuId == R.id.remove_from_tab_group) {
                assert !groupedTabs.isEmpty() : "No grouped tabs in the list.";
                tabGroupModelFilter
                        .getTabUngrouper()
                        .ungroupTabs(groupedTabs, /* trailing= */ true, /* allowDialog= */ true);
            } else if (menuId == R.id.move_to_other_window_menu_id) {
                if (!groupedTabs.isEmpty()) {
                    // Ungroup all tabs before performing the move operation.
                    tabGroupModelFilter
                            .getTabUngrouper()
                            .ungroupTabs(
                                    groupedTabs, /* trailing= */ true, /* allowDialog= */ false);
                }
                multiInstanceManager.moveTabsToOtherWindow(tabs);
            } else if (menuId == R.id.close_tab) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                tabModel.getTabRemover()
                        .closeTabs(
                                TabClosureParams.closeTabs(tabs)
                                        .allowUndo(allowUndo)
                                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                        .build(),
                                /* allowDialog= */ true);
            }
        };
    }

    /**
     * Show the context menu for multi selected tabs.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabIds The tab ids of the interacting multi selected tabs.
     */
    protected void showMenu(RectProvider anchorViewRectProvider, List<Integer> tabIds) {
        createAndShowMenu(
                anchorViewRectProvider,
                tabIds,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                assumeNonNull(mWindowAndroid.getActivity().get()));
        RecordUserAction.record("MobileToolbarTabMenu.Shown");
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, List<Integer> ids) {
        assert !ids.isEmpty() : "Empty ids list provided";
        boolean isIncognito = mTabModel.isIncognitoBranded();
        Resources res = assumeNonNull(mWindowAndroid.getActivity().get()).getResources();
        String title;

        // Add tabs to group.
        title = res.getQuantityString(R.plurals.add_tab_to_group_menu_item, ids.size());
        itemList.add(
                new ListItemBuilder()
                        .withTitle(title)
                        .withMenuId(R.id.add_to_tab_group)
                        .withIsIncognito(isIncognito)
                        .build());
        // Remove tabs from group.
        if (isAnyTabGrouped(TabModelUtils.getTabsById(ids, mTabModel, false))) {
            // Show the option if any selected tab is part of a group.
            itemList.add(
                    buildListItem(
                            R.string.remove_tabs_from_group,
                            R.id.remove_from_tab_group,
                            isIncognito));
        }
        // Move tabs to another window.
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            title =
                    res.getQuantityString(
                            R.plurals.move_tabs_to_another_window,
                            MultiWindowUtils.getInstanceCount());
            itemList.add(
                    new ListItemBuilder()
                            .withTitle(title)
                            .withMenuId(R.id.move_to_other_window_menu_id)
                            .withIsIncognito(isIncognito)
                            .build());
        }
        // Divider
        itemList.add(buildMenuDivider(isIncognito));
        // Close tabs
        itemList.add(buildListItem(R.string.close, R.id.close_tab, isIncognito));
    }

    private static ListItem buildListItem(
            @StringRes int titleRes, @IdRes int menuId, boolean isIncognito) {
        return new ListItemBuilder()
                .withTitleRes(titleRes)
                .withMenuId(menuId)
                .withIsIncognito(isIncognito)
                .build();
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return MathUtils.clamp(
                anchorViewWidthPx,
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));
    }

    @Override
    protected @Nullable String getCollaborationIdOrNull(List<Integer> ids) {
        // Multi-select does not support collaboration yet.
        return null;
    }

    /**
     * Checks if any tab in the provided list is part of a tab group.
     *
     * @param tabs The {@link List} of {@link Tab}s to check.
     * @return {@code true} if at least one tab in the list belongs to a group, {@code false}
     *     otherwise.
     */
    private boolean isAnyTabGrouped(List<Tab> tabs) {
        for (Tab tab : tabs) {
            if (mTabGroupModelFilter.isTabInTabGroup(tab)) return true;
        }
        return false;
    }

    /**
     * Filters the given list of tabs, returning a new list containing only the tabs that are part
     * of a tab group.
     *
     * @param filter The {@link TabGroupModelFilter} used to find grouped tabs.
     * @param tabs The list of {@link Tab}s to filter.
     * @return A new list of {@link Tab}s that are in a tab group.
     */
    private static List<Tab> getGroupedTabs(TabGroupModelFilter filter, List<Tab> tabs) {
        List<Tab> groupedTabs = new ArrayList<>();
        for (Tab tab : tabs) {
            if (filter.isTabInTabGroup(tab)) groupedTabs.add(tab);
        }
        return groupedTabs;
    }
}
