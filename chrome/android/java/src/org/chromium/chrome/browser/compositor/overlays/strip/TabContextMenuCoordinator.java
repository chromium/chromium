// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.isTabPinningFromStripEnabled;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.ACTIVE;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;
import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.createNewGroupForTabs;
import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.mergeTabsToDest;
import static org.chromium.chrome.browser.tasks.tab_management.GroupWindowState.IN_CURRENT_CLOSING;
import static org.chromium.components.tab_groups.TabGroupColorPickerUtils.getTabGroupColorPickerItemColor;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_DRAWABLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View.OnClickListener;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManagerUtils;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.tasks.tab_management.GroupWindowChecker;
import org.chromium.chrome.browser.tasks.tab_management.GroupWindowState;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabStripReorderingHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on a single tab or a tab
 * part of a multiple tab selection. It is responsible for creating a list of menu items, setting up
 * the menu, and displaying the menu.
 */
@NullMarked
public class TabContextMenuCoordinator extends TabStripReorderingHelper<AnchorInfo> {

    /** Stores the primary anchor tab of the context menu & ids of all multiselected tabs. */
    public static class AnchorInfo {
        private final int mAnchorTabId;
        private final List<Integer> mAllTabIds;

        public AnchorInfo(int anchorTabId, List<Integer> allTabIds) {
            mAnchorTabId = anchorTabId;
            mAllTabIds = allTabIds;
        }

        public int getAnchorTabId() {
            return mAnchorTabId;
        }

        public List<Integer> getAllTabIds() {
            return mAllTabIds;
        }

        @Override
        public boolean equals(Object otherObject) {
            if (!(otherObject instanceof AnchorInfo)) return false;
            AnchorInfo other = (AnchorInfo) otherObject;
            return other.mAnchorTabId == mAnchorTabId && other.mAllTabIds.equals(mAllTabIds);
        }

        @Override
        public int hashCode() {
            return Objects.hashCode(List.of(mAnchorTabId, mAllTabIds));
        }

        @Override
        public String toString() {
            return List.of(mAnchorTabId, mAllTabIds).toString();
        }
    }

    @SuppressWarnings("HidingField")
    private final Supplier<TabModel> mTabModelSupplier;

    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupCreationCallback mTabGroupCreationCallback;
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;

    private TabContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            BiConsumer<AnchorInfo, Boolean> reorderFunction) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        tabModelSupplier,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        multiInstanceManager,
                        shareDelegateSupplier),
                tabModelSupplier,
                multiInstanceManager,
                tabGroupSyncService,
                collaborationService,
                activity,
                reorderFunction);
        mTabModelSupplier = tabModelSupplier;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupCreationCallback = tabGroupCreationCallback;
        mWindowAndroid = windowAndroid;
        mActivity = activity;
    }

    /**
     * Creates the TabContextMenuCoordinator object.
     *
     * @param tabModelSupplier Supplies the {@link TabModel}.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param tabGroupListBottomSheetCoordinator The {@link TabGroupListBottomSheetCoordinator} that
     *     will be used to show a bottom sheet when the user selects the "Add to group" option.
     * @param tabGroupCreationCallback The {@link TabGroupCreationCallback} to run after creating a
     *     new tab group for the interacting tab(s) through the submenu.
     * @param multiInstanceManager The {@link MultiInstanceManager} that will be used to move tabs
     *     from one window to another.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param windowAndroid The {@link WindowAndroid} where this context menu will be shown.
     * @param activity The {@link Activity}.
     */
    public static TabContextMenuCoordinator createContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            BiConsumer<AnchorInfo, Boolean> reorderFunction) {
        Profile profile = assumeNonNull(tabModelSupplier.get().getProfile());

        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabContextMenuCoordinator(
                tabModelSupplier,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                tabGroupCreationCallback,
                multiInstanceManager,
                shareDelegateSupplier,
                windowAndroid,
                activity,
                tabGroupSyncService,
                collaborationService,
                reorderFunction);
    }

    @VisibleForTesting
    static OnItemClickedCallback<AnchorInfo> getMenuItemClickedCallback(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        return (menuId, anchorInfo, collaborationId, listViewTouchTracker) -> {
            List<Integer> tabIds = anchorInfo.getAllTabIds();
            assert !tabIds.isEmpty() : "Empty tab id list provided";
            TabModel tabModel = tabModelSupplier.get();
            List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
            assert !tabs.isEmpty() : "Empty tab list provided";
            recordMenuAction(menuId, tabs.size() > 1);

            if (menuId == R.id.add_to_tab_group) {
                tabGroupListBottomSheetCoordinator.showBottomSheet(tabs);
            } else if (menuId == R.id.remove_from_tab_group) {
                // Ungrouping in reverse to maintain the order of the tabs.
                Collections.reverse(tabs);
                tabGroupModelFilter
                        .getTabUngrouper()
                        .ungroupTabs(tabs, /* trailing= */ true, /* allowDialog= */ true);
            } else if (menuId == R.id.move_to_other_window_menu_id) {
                multiInstanceManager.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);
            } else if (menuId == R.id.share_tab) {
                assert tabs.size() == 1 : "Share is only available for single tab selection.";
                shareDelegateSupplier
                        .get()
                        .share(tabs.get(0), /* shareDirectly= */ false, TAB_STRIP_CONTEXT_MENU);
            } else if (menuId == R.id.duplicate_tab_menu_id) {
                for (Tab tab : tabs) {
                    tabModel.duplicateTab(tab);
                }
                tabModel.clearMultiSelection(/* notifyObservers= */ true);
            } else if (menuId == R.id.pin_tab_menu_id) {
                for (Tab tab : tabs) {
                    tabModel.pinTab(tab.getId(), /* showUngroupDialog= */ tabs.size() == 1);
                }
            } else if (menuId == R.id.unpin_tab_menu_id) {
                // Unpinning in reverse to maintain the order of the tabs.
                for (int i = tabs.size() - 1; i >= 0; i--) {
                    tabModel.unpinTab(tabs.get(i).getId());
                }
            } else if (menuId == R.id.mute_site_menu_id) {
                tabModel.setMuteSetting(tabs, /* mute= */ true);
            } else if (menuId == R.id.unmute_site_menu_id) {
                tabModel.setMuteSetting(tabs, /* mute= */ false);
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

    @VisibleForTesting
    boolean areAllTabsMuted(List<Tab> tabs) {
        TabModel tabModel = mTabModelSupplier.get();
        for (Tab tab : tabs) {
            GURL url = tab.getUrl();
            if (url.isEmpty()) continue;

            String scheme = url.getScheme();
            boolean isChromeScheme =
                    UrlConstants.CHROME_SCHEME.equals(scheme)
                            || UrlConstants.CHROME_NATIVE_SCHEME.equals(scheme);

            if (isChromeScheme && tab.getWebContents() == null) continue;

            if (!tabModel.isMuted(tab)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Show the context menu for the given tabs.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param anchorInfo The {@link AnchorInfo} for the context menu to be shown.
     */
    protected void showMenu(RectProvider anchorViewRectProvider, AnchorInfo anchorInfo) {
        createAndShowMenu(
                anchorViewRectProvider,
                anchorInfo,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                assumeNonNull(mWindowAndroid.getActivity().get()));
        recordUserAction("Shown", anchorInfo.getAllTabIds().size() > 1);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, AnchorInfo anchorInfo) {
        List<Integer> ids = anchorInfo.getAllTabIds();
        assert !ids.isEmpty() : "Empty tab id list provided";
        TabModel tabModel = mTabModelSupplier.get();
        List<Tab> tabs = TabModelUtils.getTabsById(ids, tabModel, /* allowClosing= */ false);
        assert !tabs.isEmpty() : "Empty tab list provided";
        boolean isIncognito = tabModel.isIncognitoBranded();
        if (tabs.size() == 1) {
            buildMenuActionItemsForSingleTab(itemList, anchorInfo, tabs, isIncognito);
        } else {
            buildMenuActionItemsForMultipleTabs(itemList, anchorInfo, tabs, isIncognito);
        }
    }

    @Override
    protected boolean canItemMoveTowardStart(AnchorInfo anchorInfo) {
        TabModel tabModel = mTabModelSupplier.get();
        @Nullable Tab tab = tabModel.getTabById(anchorInfo.getAllTabIds().get(0));
        if (tab == null) return false;
        int idx = tabModel.indexOf(tab);
        return tab.getIsPinned() ? idx > 0 : idx > tabModel.findFirstNonPinnedTabIndex();
    }

    @Override
    protected boolean canItemMoveTowardEnd(AnchorInfo anchorInfo) {
        List<Integer> tabs = anchorInfo.getAllTabIds();
        TabModel tabModel = mTabModelSupplier.get();
        @Nullable Tab tab = tabModel.getTabById(tabs.get(tabs.size() - 1));
        if (tab == null) return false;
        int idx = tabModel.indexOf(tab);
        return tab.getIsPinned()
                ? idx < tabModel.findFirstNonPinnedTabIndex() - 1
                : idx < tabModel.getCount() - 1;
    }

    private void buildMenuActionItemsForSingleTab(
            ModelList itemList, AnchorInfo anchorInfo, List<Tab> tabs, boolean isIncognito) {
        itemList.add(createMoveToTabGroupItem(tabs, isIncognito));
        if (TabGroupUtils.isAnyTabInGroup(tabs)) {
            itemList.add(createRemoveFromTabGroupItem(tabs, isIncognito));
        }
        if (shouldShowMoveToWindowItem(tabs)) {
            itemList.add(createMoveToWindowItem(anchorInfo, isIncognito));
        }
        List<ListItem> reorderItems = createReorderItems(anchorInfo);
        // Need to check list is non-empty before calling addAll; otherwise we get assertion error.
        if (!reorderItems.isEmpty()) itemList.addAll(reorderItems);
        itemList.add(buildMenuDivider(isIncognito));
        if (ShareUtils.shouldEnableShare(tabs.get(0))) {
            // Share is only available for single tab selection.
            itemList.add(createShareItem(isIncognito));
        }
        if (ChromeFeatureList.sAndroidContextMenuDuplicateTabs.isEnabled()) {
            itemList.add(createDuplicateTabsItem(isIncognito));
        }
        if (isTabPinningFromStripEnabled()) {
            itemList.add(createPinUnpinTabItem(tabs, isIncognito));
        }
        if (ChromeFeatureList.sMediaIndicatorsAndroid.isEnabled()) {
            itemList.add(createMuteUnmuteSiteItem(tabs, isIncognito));
        }
        itemList.add(createCloseItem(isIncognito));
    }

    private void buildMenuActionItemsForMultipleTabs(
            ModelList itemList, AnchorInfo anchorInfo, List<Tab> tabs, boolean isIncognito) {
        itemList.add(createMoveToTabGroupItem(tabs, isIncognito));
        if (TabGroupUtils.isAnyTabInGroup(tabs)) {
            itemList.add(createRemoveFromTabGroupItem(tabs, isIncognito));
        }
        if (shouldShowMoveToWindowItem(tabs)) {
            itemList.add(createMoveToWindowItem(anchorInfo, isIncognito));
        }
        List<ListItem> reorderItems = createReorderItems(anchorInfo);
        if (!reorderItems.isEmpty()) itemList.addAll(reorderItems);
        itemList.add(buildMenuDivider(isIncognito));
        if (ChromeFeatureList.sAndroidContextMenuDuplicateTabs.isEnabled()) {
            itemList.add(createDuplicateTabsItem(isIncognito));
        }
        if (isTabPinningFromStripEnabled()) {
            itemList.add(createPinUnpinTabItem(tabs, isIncognito));
        }
        if (ChromeFeatureList.sMediaIndicatorsAndroid.isEnabled()) {
            itemList.add(createMuteUnmuteSiteItem(tabs, isIncognito));
        }
        itemList.add(createCloseItem(isIncognito));
    }

    private static ListItem buildListItem(
            @StringRes int titleRes, @IdRes int menuId, boolean isIncognito) {
        return new ListItemBuilder()
                .withTitleRes(titleRes)
                .withMenuId(menuId)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createMoveToTabGroupItem(List<Tab> tabs, boolean isIncognito) {
        String title =
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_group_menu_item, tabs.size());
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)) {
            return new ListItemBuilder()
                    .withTitle(title)
                    .withMenuId(R.id.add_to_tab_group)
                    .withIsIncognito(isIncognito)
                    .build();
        }
        List<ListItem> submenuItems = new ArrayList<>();
        // "Add to new group" item
        submenuItems.add(
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE_ID, R.string.create_new_group_row_title)
                                .with(ENABLED, true)
                                .with(
                                        CLICK_LISTENER,
                                        (v) -> {
                                            recordMenuAction(
                                                    R.id.add_to_new_group_sub_menu_id,
                                                    tabs.size() > 1);
                                            createNewGroupForTabs(
                                                    tabs,
                                                    mTabGroupModelFilter,
                                                    /* tabMovedCallback= */ null,
                                                    mTabGroupCreationCallback);
                                        })
                                .build()));
        // Available tab groups.
        @Nullable Token groupToNotBeIncluded = tabs.get(0).getTabGroupId();
        List<ListItem> potentialGroups =
                isIncognito
                        ? getIncognitoTabGroups(tabs, groupToNotBeIncluded)
                        : getRegularTabGroups(tabs, groupToNotBeIncluded);
        if (!potentialGroups.isEmpty()) {
            submenuItems.add(buildMenuDivider(isIncognito));
            submenuItems.addAll(potentialGroups);
        }

        return new ListItem(
                MENU_ITEM_WITH_SUBMENU,
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(TITLE, title)
                        .with(ENABLED, true)
                        .with(SUBMENU_ITEMS, submenuItems)
                        .build());
    }

    private boolean shouldShowMoveToWindowItem(List<Tab> tabs) {
        if (TabGroupUtils.isAnyTabInGroup(tabs)) return false;
        return MultiWindowUtils.isMultiInstanceApi31Enabled() && mMultiInstanceManager != null;
    }

    private ListItem createRemoveFromTabGroupItem(List<Tab> tabs, boolean isIncognito) {
        String title =
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.remove_tabs_from_group_menu_item, tabs.size());
        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.remove_from_tab_group)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createMoveToWindowItem(AnchorInfo anchorInfo, boolean isIncognito) {
        assumeNonNull(mMultiInstanceManager);
        return createMoveToWindowItem(
                anchorInfo,
                isIncognito,
                anchorInfo.getAllTabIds().size() > 1
                        ? R.plurals.move_tabs_to_another_window
                        : R.plurals.move_tab_to_another_window,
                R.id.move_to_other_window_menu_id);
    }

    private ListItem createShareItem(boolean isIncognito) {
        return buildListItem(R.string.share, R.id.share_tab, isIncognito);
    }

    private ListItem createDuplicateTabsItem(boolean isIncognito) {
        String title = mActivity.getResources().getString(R.string.duplicate_tab_menu_item);

        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.duplicate_tab_menu_id)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createPinUnpinTabItem(List<Tab> tabs, boolean isIncognito) {
        boolean showUnpin = true;
        for (Tab tab : tabs) {
            if (!tab.getIsPinned()) {
                showUnpin = false;
                break;
            }
        }
        String title =
                showUnpin
                        ? mActivity
                                .getResources()
                                .getQuantityString(R.plurals.unpin_tabs_menu_item, tabs.size())
                        : mActivity
                                .getResources()
                                .getQuantityString(R.plurals.pin_tabs_menu_item, tabs.size());
        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(showUnpin ? R.id.unpin_tab_menu_id : R.id.pin_tab_menu_id)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createMuteUnmuteSiteItem(List<Tab> tabs, boolean isIncognito) {
        boolean showUnmute = areAllTabsMuted(tabs);
        String title =
                showUnmute
                        ? mActivity
                                .getResources()
                                .getQuantityString(R.plurals.unmute_sites_menu_item, tabs.size())
                        : mActivity
                                .getResources()
                                .getQuantityString(R.plurals.mute_sites_menu_item, tabs.size());
        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(showUnmute ? R.id.unmute_site_menu_id : R.id.mute_site_menu_id)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createCloseItem(boolean isIncognito) {
        return buildListItem(R.string.close, R.id.close_tab, isIncognito);
    }

    private static void recordMenuAction(int menuId, boolean isMultipleTabs) {
        if (menuId == R.id.add_to_tab_group) {
            recordUserAction("AddToTabGroup", isMultipleTabs);
        } else if (menuId == R.id.remove_from_tab_group) {
            recordUserAction("RemoveTabFromTabGroup", isMultipleTabs);
        } else if (menuId == R.id.move_to_other_window_menu_id) {
            if (MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE) == 1) {
                recordUserAction("MoveTabToNewWindow", isMultipleTabs);
            } else {
                recordUserAction("MoveTabsToOtherWindow", isMultipleTabs);
            }
        } else if (menuId == R.id.share_tab) {
            recordUserAction("ShareTab", isMultipleTabs);
        } else if (menuId == R.id.pin_tab_menu_id) {
            recordUserAction("PinTab", isMultipleTabs);
        } else if (menuId == R.id.unpin_tab_menu_id) {
            recordUserAction("UnpinTab", isMultipleTabs);
        } else if (menuId == R.id.close_tab) {
            recordUserAction("CloseTab", isMultipleTabs);
        } else if (menuId == R.id.add_to_new_group_sub_menu_id) {
            recordUserAction("NewGroup", isMultipleTabs);
        } else if (menuId == R.id.add_to_group_sub_menu_id) {
            recordUserAction("MoveTabToGroup", isMultipleTabs);
        } else if (menuId == R.id.add_to_group_incognito_sub_menu_id) {
            recordUserAction("MoveTabToIncognitoGroup", isMultipleTabs);
        } else if (menuId == R.id.move_to_new_window_sub_menu_id) {
            recordUserAction("MoveTabToNewWindow", isMultipleTabs);
        } else if (menuId == R.id.move_to_other_window_sub_menu_id) {
            recordUserAction("MoveTabToOtherWindow", isMultipleTabs);
        } else if (menuId == R.id.mute_site_menu_id) {
            recordUserAction("MuteSite", isMultipleTabs);
        } else if (menuId == R.id.unmute_site_menu_id) {
            recordUserAction("UnmuteSite", isMultipleTabs);
        } else if (menuId == R.id.duplicate_tab_menu_id) {
            recordUserAction("DuplicateTab", isMultipleTabs);
        } else {
            assert false : "Unknown menu id: " + menuId;
        }
    }

    private List<ListItem> getRegularTabGroups(
            List<Tab> tabs, @Nullable Token groupToNotBeIncluded) {
        GroupWindowChecker windowChecker =
                new GroupWindowChecker(mTabGroupSyncService, mTabGroupModelFilter);
        List<SavedTabGroup> sortedTabGroups =
                windowChecker.getSortedGroupList(
                        groupWindowState ->
                                groupWindowState != IN_CURRENT_CLOSING
                                        && groupWindowState != GroupWindowState.HIDDEN,
                        (a, b) -> Long.compare(b.updateTimeMs, a.updateTimeMs));

        List<ListItem> result = new ArrayList<>();

        // TODO(crbug.com/437327793): Stop filtering out Inactive windows if we can support moving a
        // tab to a group in an Inactive window.
        Map<Integer, InstanceInfo> activeInstancesById = new HashMap<>();
        List<InstanceInfo> activeInstances = assumeNonNull(mMultiInstanceManager).getInstanceInfo(ACTIVE);
        for (InstanceInfo activeInstance : activeInstances) {
            activeInstancesById.put(activeInstance.instanceId, activeInstance);
        }

        for (SavedTabGroup tabGroup : sortedTabGroups) {
            if (tabGroup.localId == null) continue;
            if (Objects.equals(groupToNotBeIncluded, tabGroup.localId.tabGroupId)) {
                continue;
            }
            Token groupId = tabGroup.localId.tabGroupId;

            TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
            @WindowId int windowId = tabWindowManager.findWindowIdForTabGroup(groupId);
            boolean isGroupInCurrentWindow =
                    windowId == mMultiInstanceManager.getCurrentInstanceId();
            if (!activeInstancesById.containsKey(windowId)) {
                continue; // Skip groups w/o active window.
            }

            @Nullable Integer firstTabInGroupTabId = tabGroup.savedTabs.get(0).localId;
            assert firstTabInGroupTabId != null : "Tab groups shouldn't be empty";
            String label =
                    TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                            mActivity, tabWindowManager, groupId, /* isIncognito= */ false);
            // If no title could be found nor could a default be generated, skip the group
            if (label == null) continue;
            @TabGroupColorId
            int colorId =
                    TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                            tabWindowManager, groupId, /* isIncognito= */ false);
            OnClickListener clickListener =
                    (v) -> {
                        recordMenuAction(R.id.add_to_group_sub_menu_id, tabs.size() > 1);
                        if (isGroupInCurrentWindow) {
                            // If the tab is already in the current window,
                            // then just merge it to the group.
                            mergeTabsToDest(
                                    tabs,
                                    firstTabInGroupTabId,
                                    mTabGroupModelFilter,
                                    /* tabMovedCallback= */ null);
                        } else {
                            mMultiInstanceManager.moveTabsToWindowAndMergeToDest(
                                    activeInstancesById.get(windowId), tabs, firstTabInGroupTabId);
                        }
                    };
            result.add(
                    new ListItem(
                            MENU_ITEM,
                            new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                    .with(TITLE, label)
                                    .with(ENABLED, true)
                                    .with(CLICK_LISTENER, clickListener)
                                    .with(START_ICON_DRAWABLE, getCircleDrawable(colorId))
                                    .build()));
        }
        return result;
    }

    private List<ListItem> getIncognitoTabGroups(
            List<Tab> tabs, @Nullable Token groupToNotBeIncluded) {
        List<ListItem> result = new ArrayList<>();
        for (Token groupId : mTabGroupModelFilter.getAllTabGroupIds()) {
            if (Objects.equals(groupToNotBeIncluded, groupId)) {
                continue;
            }

            int tabIdInGroup = mTabGroupModelFilter.getGroupLastShownTabId(groupId);
            OnClickListener clickListener =
                    (v) -> {
                        recordMenuAction(R.id.add_to_group_incognito_sub_menu_id, tabs.size() > 1);
                        mergeTabsToDest(
                                tabs,
                                tabIdInGroup,
                                mTabGroupModelFilter,
                                /* tabMovedCallback= */ null);
                    };
            result.add(
                    new ListItem(
                            MENU_ITEM,
                            new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                    .with(
                                            TITLE,
                                            TabGroupTitleUtils.getDisplayableTitle(
                                                    mActivity, mTabGroupModelFilter, groupId))
                                    .with(ENABLED, true)
                                    .with(CLICK_LISTENER, clickListener)
                                    .with(
                                            START_ICON_DRAWABLE,
                                            getCircleDrawable(
                                                    mTabGroupModelFilter.getTabGroupColor(groupId)))
                                    .build()));
        }
        return result;
    }

    private @Nullable GradientDrawable getCircleDrawable(@TabGroupColorId int colorId) {
        Drawable sourceDrawable = mActivity.getDrawable(R.drawable.tab_group_dialog_color_icon);
        GradientDrawable circleDrawable = null;
        if (sourceDrawable != null) {
            circleDrawable = (GradientDrawable) sourceDrawable.mutate();
            @ColorInt
            int color =
                    getTabGroupColorPickerItemColor(mActivity, colorId, /* isIncognito= */ false);
            circleDrawable.setColor(color);
        }
        return circleDrawable;
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return MathUtils.clamp(
                anchorViewWidthPx,
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));
    }

    @Override
    protected @Nullable String getCollaborationIdOrNull(AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        if (tabIds.isEmpty() || tabIds.size() > 1) return null;
        var tab = mTabModelSupplier.get().getTabById(tabIds.get(0));
        if (tab == null) return null;
        return TabShareUtils.getCollaborationIdOrNull(tab.getTabGroupId(), mTabGroupSyncService);
    }

    @Override
    protected void moveToNewWindow(AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        if (tabIds.isEmpty()) return;
        TabModel tabModel = mTabModelSupplier.get();
        List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
        if (tabs.isEmpty()) return;
        ungroupTabs(tabs);
        recordMenuAction(R.id.move_to_new_window_sub_menu_id, tabs.size() > 1);
        assumeNonNull(mMultiInstanceManager).moveTabsToNewWindow(tabs, NewWindowAppSource.MENU);
    }

    @Override
    protected void moveToWindow(InstanceInfo instanceInfo, AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        if (tabIds.isEmpty()) return;
        TabModel tabModel = mTabModelSupplier.get();
        List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
        if (tabs.isEmpty()) return;
        ungroupTabs(tabs);
        recordMenuAction(R.id.move_to_other_window_sub_menu_id, tabs.size() > 1);
        assumeNonNull(mMultiInstanceManager)
                .moveTabsToWindow(
                        instanceInfo, tabs, TabList.INVALID_TAB_INDEX, NewWindowAppSource.MENU);
    }

    private List<ListItem> createReorderItems(AnchorInfo anchorInfo) {
        return createReorderItems(
                anchorInfo,
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.move_tabs_left, anchorInfo.getAllTabIds().size()),
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.move_tabs_right, anchorInfo.getAllTabIds().size()));
    }

    /** Ungroups any tabs in {@param tabs} which are currently in a group. */
    private void ungroupTabs(List<Tab> tabs) {
        List<Tab> groupedTabs = TabGroupUtils.getGroupedTabs(mTabGroupModelFilter, tabs);
        if (!groupedTabs.isEmpty()) {
            // Ungroup all tabs before performing the move operation.
            mTabGroupModelFilter
                    .getTabUngrouper()
                    .ungroupTabs(groupedTabs, /* trailing= */ true, /* allowDialog= */ false);
        }
    }

    private static void recordUserAction(String label, boolean isMultipleTabs) {
        String action = isMultipleTabs ? label + ".MultiTab" : label;
        RecordUserAction.record("MobileToolbarTabMenu." + action);
    }
}
