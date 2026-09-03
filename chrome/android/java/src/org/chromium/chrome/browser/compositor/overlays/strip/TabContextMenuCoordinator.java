// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.ACTIVE;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;
import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.createNewGroupForTabs;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View.OnClickListener;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripMenuMetricsUtils.TabMenuAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.share.send_tab_to_self.ShareEntryPoint;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManagerUtils;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.browser.tasks.tab_management.GroupWindowChecker;
import org.chromium.chrome.browser.tasks.tab_management.GroupWindowInfo;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUiUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabStripReorderingHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.LayoutSwitchEntryPoint;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncherSupplier;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.BooleanSupplier;
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
    }

    /** Layout types for the tab strip. */
    @IntDef({TabStripLayoutType.HORIZONTAL, TabStripLayoutType.VERTICAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabStripLayoutType {
        int HORIZONTAL = 0;
        int VERTICAL = 1;
    }

    @VisibleForTesting
    interface SendTabToSelfCoordinatorCreator {
        SendTabToSelfCoordinator create(
                Context context,
                @Nullable WindowAndroid windowAndroid,
                String url,
                String title,
                BottomSheetController bottomSheetController,
                Profile profile,
                DeviceLockActivityLauncher deviceLockActivityLauncher,
                Supplier<@Nullable Tab> tabProvider,
                Activity activity,
                SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
                ActivityResultTracker activityResultTracker,
                MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
                SnackbarManager snackbarManager,
                @ShareEntryPoint int entryPoint);
    }

    private static SendTabToSelfCoordinatorCreator sSendTabToSelfCreator =
            SendTabToSelfCoordinator::new;

    static void setSendTabToSelfCreatorForTesting(SendTabToSelfCoordinatorCreator creator) {
        sSendTabToSelfCreator = creator;
    }

    private final TabGroupCreationCallback mTabGroupCreationCallback;
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final int mCircleSize;
    private final @TabStripLayoutType int mTabStripLayout;
    private final @Nullable BooleanSupplier mCanActivateTabLayoutToggleMenuSupplier;

    private TabContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            @Nullable Supplier<TabBookmarker> tabBookmarkerSupplier,
            BiConsumer<AnchorInfo, Boolean> reorderFunction,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager,
            @TabClosingSource int tabClosingSource,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier,
            @TabStripLayoutType int tabStripLayout) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        tabModelSupplier,
                        tabGroupListBottomSheetCoordinator,
                        tabGroupCreationCallback,
                        multiInstanceManager,
                        shareDelegateSupplier,
                        tabBookmarkerSupplier,
                        windowAndroid,
                        activity,
                        snackbarManager,
                        activityResultTracker,
                        modalDialogManager,
                        tabClosingSource,
                        tabStripLayout),
                tabModelSupplier,
                multiInstanceManager,
                tabGroupSyncService,
                collaborationService,
                activity,
                reorderFunction);
        mTabGroupCreationCallback = tabGroupCreationCallback;
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mCanActivateTabLayoutToggleMenuSupplier = canActivateTabLayoutToggleMenuSupplier;
        mTabStripLayout = tabStripLayout;

        mCircleSize = getDimensionPixelSize(R.dimen.tab_group_nested_menu_color_icon_size);
    }

    /**
     * Creates the TabContextMenuCoordinator object.
     *
     * @param tabModelSupplier Supplies the {@link TabModel}.
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
     * @param tabBookmarkerSupplier Supplies the {@link TabBookmarker} to add/edit bookmarks.
     * @param reorderFunction Callback to run when reordering tabs.
     * @param snackbarManager The {@link SnackbarManager} used to show snackbar UI.
     * @param activityResultTracker The {@link ActivityResultTracker} to track activity results.
     * @param modalDialogManager The {@link ModalDialogManager} to show modal dialogs.
     * @param tabClosingSource The {@link TabClosingSource} indicating where the tab is closed from.
     * @param canActivateTabLayoutToggleMenuSupplier Supplies whether tab layout toggle menu can be
     *     activated.
     * @param tabStripLayout The active {@link TabStripLayoutType}.
     */
    public static TabContextMenuCoordinator createContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            @Nullable Supplier<TabBookmarker> tabBookmarkerSupplier,
            BiConsumer<AnchorInfo, Boolean> reorderFunction,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager,
            @TabClosingSource int tabClosingSource,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier,
            @TabStripLayoutType int tabStripLayout) {
        Profile profile = assumeNonNull(tabModelSupplier.get().getProfile());

        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabContextMenuCoordinator(
                tabModelSupplier,
                tabGroupListBottomSheetCoordinator,
                tabGroupCreationCallback,
                multiInstanceManager,
                shareDelegateSupplier,
                windowAndroid,
                activity,
                tabGroupSyncService,
                collaborationService,
                tabBookmarkerSupplier,
                reorderFunction,
                snackbarManager,
                activityResultTracker,
                modalDialogManager,
                tabClosingSource,
                canActivateTabLayoutToggleMenuSupplier,
                tabStripLayout);
    }

    @VisibleForTesting
    static OnItemClickedCallback<AnchorInfo> getMenuItemClickedCallback(
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @Nullable Supplier<TabBookmarker> tabBookmarkerSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager,
            @TabClosingSource int tabClosingSource,
            @TabStripLayoutType int tabStripLayout) {
        return (menuId, anchorInfo, collaborationId, listViewTouchTracker) -> {
            List<Integer> tabIds = anchorInfo.getAllTabIds();
            assert !tabIds.isEmpty() : "Empty tab id list provided";
            TabModel tabModel = tabModelSupplier.get();
            List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
            // Anchored tab(s) may have been moved to another window or closed between menu open
            // and item click. Drop the action if any tabs are no longer in this TabModel.
            if (tabs.size() < tabIds.size()) return;
            recordMenuAction(
                    menuId, tabs.size() > 1, tabModel.isIncognitoBranded(), tabStripLayout);

            if (menuId == R.id.add_to_tab_group) {
                if (tabGroupListBottomSheetCoordinator != null) {
                    addToTabGroupItemCallback(tabGroupListBottomSheetCoordinator, tabs);
                }
            } else if (menuId == R.id.add_to_new_tab_group) {
                addToNewTabGroupItemCallback(tabModel, tabs, tabGroupCreationCallback);
            } else if (menuId == R.id.remove_from_tab_group) {
                removeFromTabGroupItemCallback(tabModel, tabs);
            } else if (menuId == R.id.move_to_other_window_menu_id) {
                moveToOtherWindowItemCallback(multiInstanceManager, tabs);
            } else if (menuId == R.id.share_tab) {
                shareTabItemCallback(shareDelegateSupplier.get(), tabs);
            } else if (menuId == R.id.duplicate_tab_menu_id) {
                duplicateTabItemCallback(tabModel, tabs);
            } else if (menuId == R.id.pin_tab_menu_id) {
                pinTabItemCallback(tabModel, tabs);
            } else if (menuId == R.id.unpin_tab_menu_id) {
                unpinTabItemCallback(tabModel, tabs);
            } else if (menuId == R.id.mute_site_menu_id) {
                muteSiteItemCallback(tabModel, tabs);
            } else if (menuId == R.id.unmute_site_menu_id) {
                unmuteSiteItemCallback(tabModel, tabs);
            } else if (menuId == R.id.close_tab) {
                closeTabItemCallback(tabModel, tabs, listViewTouchTracker, tabClosingSource);
            } else if (menuId == R.id.close_other_tabs_menu_id) {
                closeOtherTabsItemCallback(
                        tabModel, tabIds, listViewTouchTracker, tabClosingSource);
            } else if (menuId == R.id.close_tabs_to_the_right_menu_id) {
                closeTabsToTheRightItemCallback(
                        tabModel, tabIds, listViewTouchTracker, tabClosingSource);
            } else if (menuId == R.id.new_tab_to_the_right_menu_id) {
                newTabToTheRightItemCallback(tabModel, anchorInfo);
            } else if (menuId == R.id.add_tab_to_reading_list_menu_id) {
                if (tabBookmarkerSupplier != null) {
                    addTabToReadingListItemCallback(tabBookmarkerSupplier, tabs);
                }
            } else if (menuId == R.id.send_to_your_device_menu_id) {
                sendTabToYourDeviceItemCallback(
                        tabModel,
                        anchorInfo,
                        windowAndroid,
                        activity,
                        snackbarManager,
                        activityResultTracker,
                        modalDialogManager);
            } else if (menuId == R.id.toggle_tab_layout_menu_id) {
                boolean isEnablingVerticalTabs = tabStripLayout == TabStripLayoutType.HORIZONTAL;
                VerticalTabUtils.recordLayoutToggle(
                        activity, LayoutSwitchEntryPoint.TAB_CONTEXT_MENU, isEnablingVerticalTabs);
                if (activity instanceof MenuOrKeyboardActionController controller) {
                    controller.onMenuOrKeyboardAction(
                            R.id.toggle_tab_layout_menu_id, /* fromMenu= */ false);
                }
            }
        };
    }

    private static void addToTabGroupItemCallback(
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator, List<Tab> tabs) {
        tabGroupListBottomSheetCoordinator.showBottomSheet(tabs);
    }

    private static void addToNewTabGroupItemCallback(
            TabModel tabModel, List<Tab> tabs, TabGroupCreationCallback tabGroupCreationCallback) {
        createNewGroupForTabs(
                tabs, tabModel, /* tabMovedCallback= */ null, tabGroupCreationCallback);
    }

    private static void removeFromTabGroupItemCallback(TabModel tabModel, List<Tab> tabs) {
        // Ungrouping in reverse to maintain the order of the tabs.
        Collections.reverse(tabs);
        tabModel.getTabUngrouper().ungroupTabs(tabs, /* trailing= */ true, /* allowDialog= */ true);
    }

    private static void moveToOtherWindowItemCallback(
            MultiInstanceManager multiInstanceManager, List<Tab> tabs) {
        moveAndCleanupSource(
                multiInstanceManager,
                () ->
                        MultiInstanceOrchestratorFactory.getInstance()
                                .moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU));
    }

    private static void shareTabItemCallback(
            @Nullable ShareDelegate shareDelegate, List<Tab> tabs) {
        assert tabs.size() == 1 : "Share is only available for single tab selection.";
        assumeNonNull(shareDelegate);
        shareDelegate.share(tabs.get(0), /* shareDirectly= */ false, TAB_STRIP_CONTEXT_MENU);
    }

    private static void duplicateTabItemCallback(TabModel tabModel, List<Tab> tabs) {
        for (Tab tab : tabs) {
            tabModel.duplicateTab(tab);
        }
        tabModel.clearMultiSelection(/* notifyObservers= */ true);
    }

    private static void pinTabItemCallback(TabModel tabModel, List<Tab> tabs) {
        for (Tab tab : tabs) {
            tabModel.pinTab(tab.getId(), /* showUngroupDialog= */ tabs.size() == 1);
        }
    }

    private static void unpinTabItemCallback(TabModel tabModel, List<Tab> tabs) {
        // Unpinning in reverse to maintain the order of the tabs.
        for (int i = tabs.size() - 1; i >= 0; i--) {
            tabModel.unpinTab(tabs.get(i).getId());
        }
    }

    private static void muteSiteItemCallback(TabModel tabModel, List<Tab> tabs) {
        tabModel.setMuteSetting(tabs, /* mute= */ true);
    }

    private static void unmuteSiteItemCallback(TabModel tabModel, List<Tab> tabs) {
        tabModel.setMuteSetting(tabs, /* mute= */ false);
    }

    private static void closeTabItemCallback(
            TabModel tabModel,
            List<Tab> tabs,
            @Nullable ListViewTouchTracker listViewTouchTracker,
            @TabClosingSource int tabClosingSource) {
        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(tabs)
                                .allowUndo(allowUndo)
                                .tabClosingSource(tabClosingSource)
                                .build(),
                        /* allowDialog= */ true);
    }

    private static void closeOtherTabsItemCallback(
            TabModel tabModel,
            List<Integer> tabIds,
            @Nullable ListViewTouchTracker listViewTouchTracker,
            @TabClosingSource int tabClosingSource) {
        List<Tab> otherTabs = new ArrayList<>();
        for (Tab tab : tabModel) {
            if (!tabIds.contains(tab.getId())) {
                otherTabs.add(tab);
            }
        }
        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(otherTabs)
                                .allowUndo(allowUndo)
                                .hideTabGroups(true)
                                .tabClosingSource(tabClosingSource)
                                .build(),
                        /* allowDialog= */ true);
    }

    private static void closeTabsToTheRightItemCallback(
            TabModel tabModel,
            List<Integer> tabIds,
            @Nullable ListViewTouchTracker listViewTouchTracker,
            @TabClosingSource int tabClosingSource) {
        List<Tab> otherTabs = new ArrayList<>();
        boolean foundPivot = false;
        for (Tab tab : tabModel) {
            if (tabIds.contains(tab.getId())) {
                foundPivot = true;
                // New pivot is to the right of the old pivot. Clear previously accumulated
                // tabs.
                otherTabs.clear();
            } else if (foundPivot) {
                otherTabs.add(tab);
            }
        }
        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(otherTabs)
                                .allowUndo(allowUndo)
                                .hideTabGroups(true)
                                .tabClosingSource(tabClosingSource)
                                .build(),
                        /* allowDialog= */ true);
    }

    private static void newTabToTheRightItemCallback(TabModel tabModel, AnchorInfo anchorInfo) {
        List<Tab> anchorTabs =
                TabModelUtils.getTabsById(
                        Collections.singletonList(anchorInfo.getAnchorTabId()),
                        tabModel,
                        /* allowClosing= */ false);
        if (anchorTabs.isEmpty()) return;
        Tab anchorTab = anchorTabs.get(0);
        if (anchorTab != null) {
            int position = tabModel.indexOf(anchorTab) + 1;
            UrlConstantResolver urlConstantResolver =
                    UrlConstantResolverFactory.getForProfile(assumeNonNull(tabModel.getProfile()));
            @TabLaunchType
            int launchType =
                    anchorTab.getTabGroupId() != null
                            ? TabLaunchType.FROM_TAB_GROUP_UI
                            : TabLaunchType.FROM_CHROME_UI;
            tabModel.getTabCreator()
                    .createNewTab(
                            new LoadUrlParams(urlConstantResolver.getNtpUrl()),
                            launchType,
                            anchorTab,
                            position);
        }
    }

    private static void addTabToReadingListItemCallback(
            Supplier<TabBookmarker> tabBookmarkerSupplier, List<Tab> tabs) {
        TabBookmarker tabBookmarker = tabBookmarkerSupplier.get();
        if (tabBookmarker != null) {
            tabBookmarker.addToReadingList(tabs);
        }
    }

    private static void sendTabToYourDeviceItemCallback(
            TabModel tabModel,
            AnchorInfo anchorInfo,
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager) {
        Tab tab = tabModel.getTabById(anchorInfo.getAnchorTabId());
        if (tab == null) return;

        GURL url = tab.getUrl();
        if (url == null || url.isEmpty()) return;

        Profile profile = tabModel.getProfile();
        if (profile == null) return;

        String title = tab.getTitle();

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return;

        DeviceLockActivityLauncher deviceLockActivityLauncher =
                DeviceLockActivityLauncherSupplier.get(windowAndroid);
        if (activityResultTracker == null
                || deviceLockActivityLauncher == null
                || modalDialogManager == null) {
            return;
        }

        SendTabToSelfCoordinator sttsCoordinator =
                sSendTabToSelfCreator.create(
                        activity,
                        windowAndroid,
                        url.getSpec(),
                        title,
                        bottomSheetController,
                        profile,
                        deviceLockActivityLauncher,
                        () -> tab,
                        activity,
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        activityResultTracker,
                        ObservableSuppliers.createMonotonic(modalDialogManager),
                        snackbarManager,
                        ShareEntryPoint.TAB_MENU);
        sttsCoordinator.show();
    }

    /**
     * Show the context menu for the given tabs.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param anchorInfo The {@link AnchorInfo} for the context menu to be shown.
     */
    public void showMenu(RectProvider anchorViewRectProvider, AnchorInfo anchorInfo) {
        createAndShowMenu(
                anchorViewRectProvider,
                anchorInfo,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                assumeNonNull(mWindowAndroid.getActivity().get()));
        TabStripMenuMetricsUtils.recordTabMenuUserAction(
                TabMenuAction.SHOWN, anchorInfo.getAllTabIds().size() > 1, mTabStripLayout);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, AnchorInfo anchorInfo) {
        List<Integer> ids = anchorInfo.getAllTabIds();
        assert !ids.isEmpty() : "Empty tab id list provided";
        TabModel tabModel = getTabModel();
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
        TabModel tabModel = getTabModel();
        @Nullable Tab tab = tabModel.getTabById(anchorInfo.getAllTabIds().get(0));
        if (tab == null) return false;
        int idx = tabModel.indexOf(tab);
        return tab.getIsPinned() ? idx > 0 : idx > tabModel.findFirstNonPinnedTabIndex();
    }

    @Override
    protected boolean canItemMoveTowardEnd(AnchorInfo anchorInfo) {
        List<Integer> tabs = anchorInfo.getAllTabIds();
        TabModel tabModel = getTabModel();
        @Nullable Tab tab = tabModel.getTabById(tabs.get(tabs.size() - 1));
        if (tab == null) return false;
        int idx = tabModel.indexOf(tab);
        return tab.getIsPinned()
                ? idx < tabModel.findFirstNonPinnedTabIndex() - 1
                : idx < tabModel.getCount() - 1;
    }

    private boolean canCloseTabsToTheRight(AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        TabModel tabModel = getTabModel();
        Tab lastTab = tabModel.getTabAt(tabModel.getCount() - 1);
        return lastTab != null && !tabIds.contains(lastTab.getId());
    }

    private void buildMenuActionItemsForSingleTab(
            ModelList itemList, AnchorInfo anchorInfo, List<Tab> tabs, boolean isIncognito) {
        itemList.add(createNewTabDirectionalItem(isIncognito));
        itemList.add(createMoveToTabGroupItem(tabs, isIncognito));
        if (TabGroupUtils.isAnyTabInGroup(tabs)) {
            itemList.add(createRemoveFromTabGroupItem(tabs, isIncognito));
        }
        if (shouldShowMoveToWindowItem(tabs, anchorInfo)) {
            itemList.add(createMoveToWindowItem(anchorInfo, isIncognito));
        }
        List<ListItem> reorderItems = createReorderItems(anchorInfo, isIncognito);
        // Need to check list is non-empty before calling addAll; otherwise we get assertion error.
        if (!reorderItems.isEmpty()) itemList.addAll(reorderItems);
        itemList.add(buildMenuDivider(isIncognito));
        if (ChromeFeatureList.sAndroidContextMenuDisabledMenuItems.isEnabled()
                && ShareUtils.shouldEnableShare(tabs.get(0))) {
            // Share is only available for single tab selection.
            itemList.add(createShareItem(isIncognito));
        }
        itemList.add(createDuplicateTabsItem(isIncognito));
        itemList.add(createPinUnpinTabItem(tabs, isIncognito));
        itemList.add(createMuteUnmuteSiteItem(tabs, isIncognito));
        itemList.add(buildMenuDivider(isIncognito));
        if (ChromeFeatureList.sAndroidContextMenuDisabledMenuItems.isEnabled() && !isIncognito) {
            itemList.add(createAddTabToReadingListItem(anchorInfo));
        }
        if (!isIncognito && shouldShowSendTabToSelfMenuItem(tabs.get(0))) {
            itemList.add(createSendTabToSelfMenuItem());
            itemList.add(buildMenuDivider(isIncognito));
        }
        addVerticalTabsItems(itemList, isIncognito);
        itemList.add(createCloseItem(isIncognito));
        if (getTabModel().getCount() > 1) {
            itemList.add(createCloseOtherTabsItem(isIncognito));
        }
        if (canCloseTabsToTheRight(anchorInfo)) {
            itemList.add(createCloseTabsDirectionalItem(isIncognito));
        }
    }

    private void buildMenuActionItemsForMultipleTabs(
            ModelList itemList, AnchorInfo anchorInfo, List<Tab> tabs, boolean isIncognito) {
        itemList.add(createNewTabDirectionalItem(isIncognito));
        itemList.add(createMoveToTabGroupItem(tabs, isIncognito));
        if (TabGroupUtils.isAnyTabInGroup(tabs)) {
            itemList.add(createRemoveFromTabGroupItem(tabs, isIncognito));
        }
        if (shouldShowMoveToWindowItem(tabs, anchorInfo)) {
            itemList.add(createMoveToWindowItem(anchorInfo, isIncognito));
        }
        List<ListItem> reorderItems = createReorderItems(anchorInfo, isIncognito);
        if (!reorderItems.isEmpty()) itemList.addAll(reorderItems);
        itemList.add(buildMenuDivider(isIncognito));
        itemList.add(createDuplicateTabsItem(isIncognito));
        itemList.add(createPinUnpinTabItem(tabs, isIncognito));
        itemList.add(createMuteUnmuteSiteItem(tabs, isIncognito));
        itemList.add(buildMenuDivider(isIncognito));
        if (ChromeFeatureList.sAndroidContextMenuDisabledMenuItems.isEnabled() && !isIncognito) {
            itemList.add(createAddTabToReadingListItem(anchorInfo));
        }
        addVerticalTabsItems(itemList, isIncognito);
        itemList.add(createCloseItem(isIncognito));
        if (getTabModel().getCount() > anchorInfo.getAllTabIds().size()) {
            itemList.add(createCloseOtherTabsItem(isIncognito));
        }
        if (canCloseTabsToTheRight(anchorInfo)) {
            itemList.add(createCloseTabsDirectionalItem(isIncognito));
        }
    }

    private boolean shouldShowMoveToWindowItem(List<Tab> tabs, AnchorInfo anchorInfo) {
        if (TabGroupUtils.isAnyTabInGroup(tabs)) return false;
        if (MultiWindowUtils.getInstanceCount(
                                getActiveInstanceTypeForProfileType(
                                        tabs.get(0).isIncognitoBranded()))
                        == 1
                && (getTabModel().getTabCountSupplier().get()
                        == anchorInfo.getAllTabIds().size())) {
            return false;
        }
        return MultiWindowUtils.isMultiInstanceApi31Enabled() && mMultiInstanceManager != null;
    }

    private static ListItem buildListItem(
            @StringRes int titleRes, @IdRes int menuId, boolean isIncognito) {
        return new ListItemBuilder()
                .withTitleRes(titleRes)
                .withMenuId(menuId)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createNewTabDirectionalItem(boolean isIncognito) {
        boolean isVerticalTabs = mTabStripLayout == TabStripLayoutType.VERTICAL;
        int stringId =
                isVerticalTabs
                        ? R.string.new_tab_below_menu_item
                        : R.string.new_tab_to_the_right_menu_item;
        String title = mActivity.getResources().getString(stringId);

        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.new_tab_to_the_right_menu_id)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createMoveToTabGroupItem(List<Tab> tabs, boolean isIncognito) {
        // Available tab groups.
        @Nullable Token groupToNotBeIncluded = tabs.get(0).getTabGroupId();
        List<ListItem> potentialGroups = getTabGroups(tabs, groupToNotBeIncluded, isIncognito);

        if (potentialGroups.isEmpty()) {
            String title =
                    mActivity
                            .getResources()
                            .getQuantityString(
                                    R.plurals.add_tab_to_new_group_menu_item, tabs.size());
            return new ListItemBuilder()
                    .withTitle(title)
                    .withMenuId(R.id.add_to_new_tab_group)
                    .withIsIncognito(isIncognito)
                    .build();
        }

        List<ListItem> submenuItems = new ArrayList<>();
        // "Add to new group" item
        submenuItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.create_new_group_row_title)
                        .withIsIncognito(isIncognito)
                        .withClickListener(
                                (v) -> {
                                    recordMenuAction(
                                            R.id.add_to_new_group_sub_menu_id,
                                            tabs.size() > 1,
                                            isIncognito,
                                            mTabStripLayout);
                                    createNewGroupForTabs(
                                            tabs,
                                            getTabModel(),
                                            /* tabMovedCallback= */ null,
                                            mTabGroupCreationCallback);
                                })
                        .build());
        // Add all the potential groups to the list afterwards.
        submenuItems.addAll(potentialGroups);

        String title =
                TabGroupUiUtils.getAddToGroupMenuItemTitle(
                        mActivity, groupToNotBeIncluded, tabs.size());
        return new ListItemBuilder()
                .withTitle(title)
                .withIsIncognito(isIncognito)
                .withSubmenuItems(submenuItems)
                .build();
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
        int totalTabCount = getTabModel().getTabCountSupplier().get();
        int moveTabCount = anchorInfo.getAllTabIds().size();
        boolean allowMoveToNewWindow = totalTabCount > moveTabCount;
        return createMoveToWindowItem(
                anchorInfo,
                isIncognito,
                moveTabCount > 1
                        ? R.plurals.move_tabs_to_another_window
                        : R.plurals.move_tab_to_another_window,
                R.id.move_to_other_window_menu_id,
                allowMoveToNewWindow);
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

    private ListItem createCloseTabsDirectionalItem(boolean isIncognito) {
        boolean isVerticalTabs = mTabStripLayout == TabStripLayoutType.VERTICAL;
        int stringId =
                isVerticalTabs
                        ? R.string.close_tabs_below_menu_item
                        : R.string.close_tabs_to_the_right_menu_item;
        String title = mActivity.getResources().getString(stringId);
        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.close_tabs_to_the_right_menu_id)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createCloseOtherTabsItem(boolean isIncognito) {
        String title = mActivity.getResources().getString(R.string.close_other_tabs_menu_item);
        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.close_other_tabs_menu_id)
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

    @VisibleForTesting
    boolean areAllTabsMuted(List<Tab> tabs) {
        TabModel tabModel = getTabModel();
        for (Tab tab : tabs) {
            GURL url = tab.getUrl();
            if (url.isEmpty()) continue;

            boolean isChromeScheme = UrlUtilities.isChromeScheme(url);

            if (isChromeScheme && tab.getWebContents() == null) continue;

            if (!tabModel.isMuted(tab)) {
                return false;
            }
        }
        return true;
    }

    private ListItem createAddTabToReadingListItem(AnchorInfo anchorInfo) {
        String title =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.add_tab_to_reading_list_menu_item,
                                anchorInfo.getAllTabIds().size());

        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.add_tab_to_reading_list_menu_id)
                .build();
    }

    private boolean shouldShowSendTabToSelfMenuItem(Tab tab) {
        GURL url = tab.getUrl();
        if (url == null || url.isEmpty()) return false;

        Profile profile = getTabModel().getProfile();
        if (profile == null) return false;

        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(profile, url.getSpec());
        return displayReason != null;
    }

    private ListItem createSendTabToSelfMenuItem() {
        String title =
                mActivity.getResources().getString(R.string.send_tab_to_self_context_menu_title);

        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.send_to_your_device_menu_id)
                .build();
    }

    private void addVerticalTabsItems(ModelList itemList, boolean isIncognito) {
        if (!VerticalTabUtils.isVerticalTabsEligible(mActivity)) return;
        if (itemList.isEmpty() || itemList.get(itemList.size() - 1).type != ListItemType.DIVIDER) {
            itemList.add(buildMenuDivider(isIncognito));
        }

        boolean isEnablingVerticalTabs = mTabStripLayout == TabStripLayoutType.HORIZONTAL;
        int layoutTitleRes =
                isEnablingVerticalTabs
                        ? R.string.show_tabs_vertically
                        : R.string.show_tabs_horizontally;

        boolean enabled =
                mCanActivateTabLayoutToggleMenuSupplier == null
                        || mCanActivateTabLayoutToggleMenuSupplier.getAsBoolean();

        boolean showNewBadge =
                isEnablingVerticalTabs
                        && VerticalTabUtils.shouldShowNewBadgeForVerticalTabs(
                                mActivity, getTabModel().getProfile());

        CharSequence title;
        if (showNewBadge) {
            title = VerticalTabUtils.getTitleWithNewBadge(mActivity, layoutTitleRes);
        } else {
            title = mActivity.getString(layoutTitleRes);
        }

        itemList.add(
                new ListItemBuilder()
                        .withTitle(title)
                        .withMenuId(R.id.toggle_tab_layout_menu_id)
                        .withIsIncognito(isIncognito)
                        .withEnabled(enabled)
                        .build());
        itemList.add(buildMenuDivider(isIncognito));
    }

    private ListItem createCloseItem(boolean isIncognito) {
        return buildListItem(R.string.close, R.id.close_tab, isIncognito);
    }

    private static void recordMenuAction(
            int menuId,
            boolean isMultipleTabs,
            boolean isIncognito,
            @TabStripLayoutType int tabStripLayout) {
        if (menuId == R.id.add_to_tab_group) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.ADD_TO_TAB_GROUP, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.add_to_new_tab_group) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.ADD_TO_NEW_TAB_GROUP, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.remove_from_tab_group) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.REMOVE_TAB_FROM_TAB_GROUP, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.move_to_other_window_menu_id) {
            if (MultiWindowUtils.getInstanceCount(getActiveInstanceTypeForProfileType(isIncognito))
                    == 1) {
                TabStripMenuMetricsUtils.recordTabMenuUserAction(
                        TabMenuAction.MOVE_TAB_TO_NEW_WINDOW, isMultipleTabs, tabStripLayout);
            } else {
                TabStripMenuMetricsUtils.recordTabMenuUserAction(
                        TabMenuAction.MOVE_TABS_TO_OTHER_WINDOW, isMultipleTabs, tabStripLayout);
            }
        } else if (menuId == R.id.share_tab) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.SHARE_TAB, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.pin_tab_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.PIN_TAB, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.unpin_tab_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.UNPIN_TAB, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.close_tab) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.CLOSE_TAB, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.add_to_new_group_sub_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.NEW_GROUP, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.add_to_group_sub_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.MOVE_TAB_TO_GROUP, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.add_to_group_incognito_sub_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.MOVE_TAB_TO_INCOGNITO_GROUP, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.move_to_new_window_sub_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.MOVE_TAB_TO_NEW_WINDOW, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.move_to_other_window_sub_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.MOVE_TAB_TO_OTHER_WINDOW, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.mute_site_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.MUTE_SITE, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.unmute_site_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.UNMUTE_SITE, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.duplicate_tab_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.DUPLICATE_TAB, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.close_all_tabs_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.CLOSE_ALL_TABS, /* isMultipleTabs= */ false, tabStripLayout);
        } else if (menuId == R.id.close_all_incognito_tabs_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.CLOSE_ALL_INCOGNITO_TABS,
                    /* isMultipleTabs= */ false,
                    tabStripLayout);
        } else if (menuId == R.id.close_other_tabs_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.CLOSE_OTHER_TABS, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.close_tabs_to_the_right_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    tabStripLayout == TabStripLayoutType.VERTICAL
                            ? TabMenuAction.CLOSE_TABS_BELOW
                            : TabMenuAction.CLOSE_TABS_TO_THE_RIGHT,
                    isMultipleTabs,
                    tabStripLayout);
        } else if (menuId == R.id.new_tab_to_the_right_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    tabStripLayout == TabStripLayoutType.VERTICAL
                            ? TabMenuAction.NEW_TAB_BELOW
                            : TabMenuAction.NEW_TAB_TO_THE_RIGHT,
                    /* isMultipleTabs= */ false,
                    tabStripLayout);
        } else if (menuId == R.id.add_tab_to_reading_list_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.ADD_TAB_TO_READING_LIST, isMultipleTabs, tabStripLayout);
        } else if (menuId == R.id.send_to_your_device_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.SEND_TO_YOUR_DEVICES,
                    /* isMultipleTabs= */ false,
                    tabStripLayout);
        } else if (menuId == R.id.toggle_tab_layout_menu_id) {
            TabStripMenuMetricsUtils.recordTabMenuUserAction(
                    TabMenuAction.TOGGLE_TAB_LAYOUT, /* isMultipleTabs= */ false, tabStripLayout);
        } else {
            assert false : "Unknown menu id: " + menuId;
        }
    }

    private List<ListItem> getTabGroups(
            List<Tab> tabs, @Nullable Token groupToNotBeIncluded, boolean isIncognito) {
        GroupWindowChecker windowChecker =
                new GroupWindowChecker(mActivity, mTabGroupSyncService, getTabModel());
        List<GroupWindowInfo> sortedTabGroups = windowChecker.getDefaultSortedGroupList();

        List<ListItem> result = new ArrayList<>();

        Set<Integer> activeInstanceIds = MultiWindowUtils.getUsableInstanceIds(ACTIVE);
        for (GroupWindowInfo tabGroup : sortedTabGroups) {
            if (tabGroup.localId == null) continue;
            if (Objects.equals(groupToNotBeIncluded, tabGroup.localId)) {
                continue;
            }
            Token groupId = tabGroup.localId;

            TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
            @WindowId int windowId = tabWindowManager.findWindowIdForTabGroup(groupId);
            if (!activeInstanceIds.contains(windowId)) {
                continue; // Skip groups w/o active window.
            }

            String label =
                    TabWindowManagerUtils.getTabGroupTitleInAnyWindow(
                            mActivity, tabWindowManager, groupId, isIncognito);
            // If no title could be found nor could a default be generated, skip the group
            if (label == null) continue;
            @TabGroupColorId
            int colorId =
                    TabWindowManagerUtils.getTabGroupColorInAnyWindow(
                            tabWindowManager, groupId, isIncognito);
            @IdRes
            int menuId =
                    isIncognito
                            ? R.id.add_to_group_incognito_sub_menu_id
                            : R.id.add_to_group_sub_menu_id;
            OnClickListener clickListener =
                    (v) -> {
                        recordMenuAction(menuId, tabs.size() > 1, isIncognito, mTabStripLayout);
                        TabGroupUiUtils.addTabsToGroup(
                                getTabModel(),
                                tabs,
                                tabGroup,
                                /* tabMovedCallback= */ null,
                                /* bringToFront= */ true);
                    };
            result.add(
                    new ListItemBuilder()
                            .withTitle(label)
                            .withClickListener(clickListener)
                            .withIsIncognito(isIncognito)
                            .withStartIconDrawable(
                                    TabGroupUtils.createColorDrawableForMenu(
                                            mActivity, colorId, isIncognito, mCircleSize))
                            .withStartIconWidth(mCircleSize)
                            .withShouldTintIcon(false)
                            .build());
        }
        return result;
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width);
    }

    @Override
    protected @Nullable String getCollaborationIdOrNull(AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        if (tabIds.size() != 1) return null;
        var tab = getTabModel().getTabById(tabIds.get(0));
        if (tab == null) return null;
        return TabShareUtils.getCollaborationIdOrNull(tab.getTabGroupId(), mTabGroupSyncService);
    }

    @Override
    @RequiresNonNull("mMultiInstanceManager")
    protected void moveToNewWindow(AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        if (tabIds.isEmpty()) return;
        TabModel tabModel = getTabModel();
        List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
        if (tabs.isEmpty()) return;
        ungroupTabs(tabs);
        recordMenuAction(
                R.id.move_to_new_window_sub_menu_id,
                tabs.size() > 1,
                tabModel.isIncognitoBranded(),
                mTabStripLayout);
        moveAndCleanupSource(
                mMultiInstanceManager,
                () ->
                        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                                mActivity,
                                tabs,
                                /* finalizeCallback= */ null,
                                NewWindowAppSource.MENU));
    }

    @Override
    @RequiresNonNull("mMultiInstanceManager")
    protected void moveToWindow(InstanceInfo instanceInfo, AnchorInfo anchorInfo) {
        List<Integer> tabIds = anchorInfo.getAllTabIds();
        if (tabIds.isEmpty()) return;
        TabModel tabModel = getTabModel();
        List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
        if (tabs.isEmpty()) return;
        ungroupTabs(tabs);
        recordMenuAction(
                R.id.move_to_other_window_sub_menu_id,
                tabs.size() > 1,
                tabModel.isIncognitoBranded(),
                mTabStripLayout);
        moveAndCleanupSource(
                mMultiInstanceManager,
                () ->
                        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                                instanceInfo.instanceId,
                                tabs,
                                /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                                /* bringToFront= */ true));
    }

    private List<ListItem> createReorderItems(AnchorInfo anchorInfo, boolean isIncognito) {
        boolean isVerticalTabs = mTabStripLayout == TabStripLayoutType.VERTICAL;
        int moveStartPlural = isVerticalTabs ? R.plurals.move_tabs_up : R.plurals.move_tabs_left;
        int moveEndPlural = isVerticalTabs ? R.plurals.move_tabs_down : R.plurals.move_tabs_right;
        int count = anchorInfo.getAllTabIds().size();
        return createReorderItems(
                anchorInfo,
                mActivity.getResources().getQuantityString(moveStartPlural, count),
                mActivity.getResources().getQuantityString(moveEndPlural, count),
                isIncognito,
                isVerticalTabs);
    }

    /** Ungroups any tabs in {@code tabs} which are currently in a group. */
    private void ungroupTabs(List<Tab> tabs) {
        List<Tab> groupedTabs = TabGroupUtils.getGroupedTabs(getTabModel(), tabs);
        if (!groupedTabs.isEmpty()) {
            // Ungroup all tabs before performing the move operation.
            getTabModel()
                    .getTabUngrouper()
                    .ungroupTabs(groupedTabs, /* trailing= */ true, /* allowDialog= */ false);
        }
    }
}
