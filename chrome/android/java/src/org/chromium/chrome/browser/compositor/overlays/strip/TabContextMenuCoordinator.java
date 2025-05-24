// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
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
    private final Supplier<TabModel> mTabModelSupplier;
    private final WindowAndroid mWindowAndroid;

    private TabContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
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
                        multiInstanceManager,
                        shareDelegateSupplier),
                tabModelSupplier,
                tabGroupSyncService,
                collaborationService,
                windowAndroid.getActivity().get());
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
     * @param multiInstanceManager The {@link MultiInstanceManager} that will be used to move tabs
     *     from one window to another.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param windowAndroid The {@link WindowAndroid} where this context menu will be shown.
     */
    public static TabContextMenuCoordinator createContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
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
                multiInstanceManager,
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
            MultiInstanceManager multiInstanceManager,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        return (menuId, tabId, collaborationId, listViewTouchTracker) -> {
            if (tabId == Tab.INVALID_TAB_ID) return;
            TabModel tabModel = tabModelSupplier.get();
            Tab tab = tabModel.getTabById(tabId);
            if (tab == null) return;

            if (menuId == R.id.add_to_tab_group) {
                tabGroupListBottomSheetCoordinator.showBottomSheet(List.of(tab));
                RecordUserAction.record("MobileToolbarTabMenu.AddToTabGroup");
            } else if (menuId == R.id.remove_from_tab_group) {
                tabGroupModelFilter
                        .getTabUngrouper()
                        .ungroupTabs(List.of(tab), /* trailing= */ true, /* allowDialog= */ true);
                RecordUserAction.record("MobileToolbarTabMenu.RemoveFromTabGroup");
            } else if (menuId == R.id.move_to_other_window_menu_id) {
                if (MultiWindowUtils.getInstanceCount() == 1) {
                    RecordUserAction.record("MobileToolbarTabMenu.MoveTabToNewWindow");
                } else {
                    RecordUserAction.record("MobileToolbarTabMenu.MoveTabToOtherWindow");
                }
                multiInstanceManager.moveTabToOtherWindow(tab);
            } else if (menuId == R.id.share_tab) {
                shareDelegateSupplier
                        .get()
                        .share(tab, /* shareDirectly= */ false, TAB_STRIP_CONTEXT_MENU);
                RecordUserAction.record("MobileToolbarTabMenu.ShareTab");
            } else if (menuId == R.id.close_tab) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                tabModel.getTabRemover()
                        .closeTabs(
                                TabClosureParams.closeTab(tab).allowUndo(allowUndo).build(),
                                /* allowDialog= */ true);
                RecordUserAction.record("MobileToolbarTabMenu.CloseTab");
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
        RecordUserAction.record("MobileToolbarTabMenu.Shown");
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer id) {
        var tab = mTabModelSupplier.get().getTabById(id);
        if (tab == null) return;
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.menu_add_tab_to_group,
                        R.id.add_to_tab_group,
                        isIncognito,
                        /* enabled= */ true));

        if (tab.getTabGroupId() != null) {
            // Show the option to remove the tab from its group iff the tab is already in a group.
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.remove_tab_from_group,
                            R.id.remove_from_tab_group,
                            isIncognito,
                            /* enabled= */ true));
        }

        if (tab.getTabGroupId() == null && MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            // Show the option to move the tab to another window iff the tab is not in a group.
            Activity activity = mWindowAndroid.getActivity().get();
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            activity.getResources()
                                    .getQuantityString(
                                            R.plurals.move_tab_to_another_window,
                                            MultiWindowUtils.getInstanceCount()),
                            R.id.move_to_other_window_menu_id,
                            isIncognito,
                            /* enabled= */ true));
        }

        itemList.add(buildMenuDivider(isIncognito));

        if (ShareUtils.shouldEnableShare(tab)) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.share, R.id.share_tab, isIncognito, /* enabled= */ true));
        }

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.close, R.id.close_tab, isIncognito, /* enabled= */ true));
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return MathUtils.clamp(
                anchorViewWidthPx,
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));
    }

    @Nullable
    @Override
    protected String getCollaborationIdOrNull(Integer id) {
        var tab = mTabModelSupplier.get().getTabById(id);
        if (tab == null) return null;
        return TabShareUtils.getCollaborationIdOrNull(tab.getTabGroupId(), mTabGroupSyncService);
    }
}
