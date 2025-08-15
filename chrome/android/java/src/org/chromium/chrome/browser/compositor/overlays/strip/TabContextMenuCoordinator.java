// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;
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

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
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
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.tasks.tab_management.GroupWindowChecker;
import org.chromium.chrome.browser.tasks.tab_management.GroupWindowState;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on a tab. It is responsible
 * for creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class TabContextMenuCoordinator extends TabOverflowMenuCoordinator<Integer> {
    @SuppressWarnings("HidingField")
    private final Supplier<TabModel> mTabModelSupplier;

    private final TabGroupModelFilter mTabGroupModelFilter;
    private final WindowAndroid mWindowAndroid;
    private final Context mContext;

    private TabContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Context context,
            @Nullable TabGroupSyncService tabGroupSyncService,
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
                multiInstanceManager,
                tabGroupSyncService,
                collaborationService,
                context);
        mTabModelSupplier = tabModelSupplier;
        mTabGroupModelFilter = tabGroupModelFilter;
        mWindowAndroid = windowAndroid;
        mContext = context;
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
            WindowAndroid windowAndroid,
            Context context) {
        Profile profile = assumeNonNull(tabModelSupplier.get().getProfile());

        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabContextMenuCoordinator(
                tabModelSupplier,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                multiInstanceManager,
                shareDelegateSupplier,
                windowAndroid,
                context,
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
                multiInstanceManager.moveTabsToOtherWindow(Collections.singletonList(tab));
            } else if (menuId == R.id.share_tab) {
                shareDelegateSupplier
                        .get()
                        .share(tab, /* shareDirectly= */ false, TAB_STRIP_CONTEXT_MENU);
                RecordUserAction.record("MobileToolbarTabMenu.ShareTab");
            } else if (menuId == R.id.pin_tab_menu_id) {
                tabModel.pinTab(tab.getId());
            } else if (menuId == R.id.unpin_tab_menu_id) {
                tabModel.unpinTab(tab.getId());
            } else if (menuId == R.id.close_tab) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                tabModel.getTabRemover()
                        .closeTabs(
                                TabClosureParams.closeTab(tab)
                                        .allowUndo(allowUndo)
                                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                        .build(),
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
                /* animStyle= */ Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                assumeNonNull(mWindowAndroid.getActivity().get()));
        RecordUserAction.record("MobileToolbarTabMenu.Shown");
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer id) {
        var tab = mTabModelSupplier.get().getTabById(id);
        if (tab == null) return;
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();

        itemList.add(createMoveToTabGroupItem(tab, isIncognito));

        if (tab.getTabGroupId() != null) {
            // Show the option to remove the tab from its group iff the tab is already in a group.
            itemList.add(
                    buildListItem(
                            R.string.remove_tab_from_group,
                            R.id.remove_from_tab_group,
                            isIncognito));
        }

        if (tab.getTabGroupId() == null
                && MultiWindowUtils.isMultiInstanceApi31Enabled()
                && mMultiInstanceManager != null) {
            // Show the option to move the tab to another window iff the tab is not in a group.
            itemList.add(
                    createMoveToWindowItem(
                            id,
                            isIncognito,
                            R.plurals.move_tab_to_another_window,
                            R.id.move_to_other_window_menu_id));
        }

        itemList.add(buildMenuDivider(isIncognito));

        if (ShareUtils.shouldEnableShare(tab)) {
            itemList.add(buildListItem(R.string.share, R.id.share_tab, isIncognito));
        }

        if (ChromeFeatureList.sAndroidPinnedTabs.isEnabled()) {
            int menuId = tab.getIsPinned() ? R.id.unpin_tab_menu_id : R.id.pin_tab_menu_id;
            int titleId = tab.getIsPinned() ? R.string.menu_unpin_tab : R.string.menu_pin_tab;
            itemList.add(buildListItem(titleId, menuId, isIncognito));
        }

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
    protected @Nullable String getCollaborationIdOrNull(Integer id) {
        var tab = mTabModelSupplier.get().getTabById(id);
        if (tab == null) return null;
        return TabShareUtils.getCollaborationIdOrNull(tab.getTabGroupId(), mTabGroupSyncService);
    }

    @Override
    protected void moveToNewWindow(Integer tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return;
        TabModel tabModel = mTabModelSupplier.get();
        Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return;
        RecordUserAction.record("MobileToolbarTabMenu.MoveTabToNewWindow");
        assumeNonNull(mMultiInstanceManager).moveTabsToNewWindow(Collections.singletonList(tab));
    }

    @Override
    protected void moveToWindow(InstanceInfo instanceInfo, Integer tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return;
        TabModel tabModel = mTabModelSupplier.get();
        Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return;
        RecordUserAction.record("MobileToolbarTabMenu.MoveTabToOtherWindow");
        assumeNonNull(mMultiInstanceManager)
                .moveTabsToWindow(
                        instanceInfo, Collections.singletonList(tab), TabList.INVALID_TAB_INDEX);
    }

    private ListItem createMoveToTabGroupItem(Tab tab, boolean isIncognito) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)) {
            return buildListItem(
                    R.string.menu_add_tab_to_group, R.id.add_to_tab_group, isIncognito);
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
                                            RecordUserAction.record(
                                                    "MobileToolbarTabMenu.NewGroup");
                                            createNewGroupForTabs(
                                                    List.of(tab),
                                                    mTabGroupModelFilter,
                                                    /* tabMovedCallback= */ null,
                                                    /* tabGroupCreationCallback= */ null);
                                        })
                                .build()));
        // Available tab groups.
        @Nullable Token groupToNotBeIncluded = tab.getTabGroupId();
        List<ListItem> potentialGroups =
                isIncognito
                        ? getIncognitoTabGroups(tab, groupToNotBeIncluded)
                        : getRegularTabGroups(tab, groupToNotBeIncluded);
        if (!potentialGroups.isEmpty()) {
            submenuItems.add(buildMenuDivider(isIncognito));
            submenuItems.addAll(potentialGroups);
        }

        return new ListItem(
                MENU_ITEM_WITH_SUBMENU,
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(TITLE, mContext.getString(R.string.menu_add_tab_to_group))
                        .with(ENABLED, true)
                        .with(SUBMENU_ITEMS, submenuItems)
                        .build());
    }

    private List<ListItem> getRegularTabGroups(Tab tab, @Nullable Token groupToNotBeIncluded) {
        GroupWindowChecker windowChecker =
                new GroupWindowChecker(mTabGroupSyncService, mTabGroupModelFilter);
        List<SavedTabGroup> sortedTabGroups =
                windowChecker.getSortedGroupList(
                        groupWindowState ->
                                groupWindowState != IN_CURRENT_CLOSING
                                        // TODO(crbug.com/437327793): Support group in other window
                                        && groupWindowState != GroupWindowState.IN_ANOTHER
                                        && groupWindowState != GroupWindowState.HIDDEN,
                        (a, b) -> Long.compare(b.updateTimeMs, a.updateTimeMs));

        List<ListItem> result = new ArrayList<>();

        // TODO(crbug.com/437327793): Stop filtering out Inactive windows if we can support moving a
        // tab to a group in an Inactive window.
        Set<Integer> activeInstanceIds = new HashSet<>();
        List<InstanceInfo> activeInstances =
                assumeNonNull(mMultiInstanceManager).getInstanceInfo(ACTIVE);
        for (InstanceInfo activeInstance : activeInstances) {
            activeInstanceIds.add(activeInstance.instanceId);
        }

        for (SavedTabGroup tabGroup : sortedTabGroups) {
            if (tabGroup.localId == null) continue;
            if (Objects.equals(groupToNotBeIncluded, tabGroup.localId.tabGroupId)) {
                continue;
            }
            Token groupId = tabGroup.localId.tabGroupId;

            TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
            @WindowId int windowId = tabWindowManager.findWindowIdForTabGroup(groupId);
            if (!activeInstanceIds.contains(windowId)) continue; // Skip groups w/o active window.

            @Nullable Integer firstTabInGroupTabId = tabGroup.savedTabs.get(0).localId;
            assert firstTabInGroupTabId != null : "Tab groups shouldn't be empty";
            String fallbackLabel =
                    TabGroupTitleUtils.getDefaultTitle(
                            mContext, mTabGroupModelFilter.getTabCountForGroup(groupId));
            String label = TextUtils.isEmpty(tabGroup.title) ? fallbackLabel : tabGroup.title;
            result.add(
                    new ListItem(
                            MENU_ITEM,
                            new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                    .with(TITLE, label)
                                    .with(ENABLED, true)
                                    .with(
                                            CLICK_LISTENER,
                                            (v) -> {
                                                RecordUserAction.record(
                                                        "MobileToolbarTabMenu.MoveTabToGroup");
                                                mergeTabsToDest(
                                                        List.of(tab),
                                                        firstTabInGroupTabId,
                                                        mTabGroupModelFilter,
                                                        /* tabMovedCallback= */ null);
                                            })
                                    .with(START_ICON_DRAWABLE, getCircleDrawable(groupId))
                                    .build()));
        }
        return result;
    }

    private List<ListItem> getIncognitoTabGroups(Tab tab, @Nullable Token groupToNotBeIncluded) {
        List<ListItem> result = new ArrayList<>();
        for (Token groupId : mTabGroupModelFilter.getAllTabGroupIds()) {
            if (Objects.equals(groupToNotBeIncluded, groupId)) {
                continue;
            }

            int tabIdInGroup = mTabGroupModelFilter.getGroupLastShownTabId(groupId);
            result.add(
                    new ListItem(
                            MENU_ITEM,
                            new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                    .with(TITLE, mTabGroupModelFilter.getTabGroupTitle(groupId))
                                    .with(ENABLED, true)
                                    .with(
                                            CLICK_LISTENER,
                                            (v) -> {
                                                RecordUserAction.record(
                                                        "MobileToolbarTabMenu.MoveTabToIncognitoGroup");
                                                mergeTabsToDest(
                                                        List.of(tab),
                                                        tabIdInGroup,
                                                        mTabGroupModelFilter,
                                                        /* tabMovedCallback= */ null);
                                            })
                                    .with(START_ICON_DRAWABLE, getCircleDrawable(groupId))
                                    .build()));
        }
        return result;
    }

    private @Nullable GradientDrawable getCircleDrawable(Token groupId) {
        @Nullable Drawable sourceDrawable =
                mContext.getDrawable(R.drawable.tab_group_dialog_color_icon);
        @Nullable GradientDrawable circleDrawable = null;
        if (sourceDrawable != null) {
            circleDrawable = (GradientDrawable) sourceDrawable.mutate();
            @ColorInt
            int color =
                    getTabGroupColorPickerItemColor(
                            mContext,
                            mTabGroupModelFilter.getTabGroupColor(groupId),
                            /* isIncognito= */ false);
            circleDrawable.setColor(color);
        }
        return circleDrawable;
    }
}
