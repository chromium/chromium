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

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
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
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
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
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncherSupplier;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Set;
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
                SnackbarManager snackbarManager);
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
    private final int mIconSize;
    private final int mRowHeight;
    private final float mVisualCenterOfTextY;
    private final float mVisualCenterOfTextYIncognito;

    private TabContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            BiConsumer<AnchorInfo, Boolean> reorderFunction,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager) {
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
                        modalDialogManager),
                tabModelSupplier,
                multiInstanceManager,
                tabGroupSyncService,
                collaborationService,
                activity,
                reorderFunction);
        mTabGroupCreationCallback = tabGroupCreationCallback;
        mWindowAndroid = windowAndroid;
        mActivity = activity;

        mCircleSize = getDimensionPixelSize(R.dimen.tab_group_nested_menu_color_icon_size);

        Context themedContext =
                new ContextThemeWrapper(mActivity, R.style.OverflowMenuThemeOverlay);
        TypedValue value = new TypedValue();
        themedContext.getTheme().resolveAttribute(R.attr.listItemIconSize, value, true);
        mIconSize =
                TypedValue.complexToDimensionPixelSize(
                        value.data, mActivity.getResources().getDisplayMetrics());

        themedContext.getTheme().resolveAttribute(R.attr.listItemHeight, value, true);
        mRowHeight =
                TypedValue.complexToDimensionPixelSize(
                        value.data, mActivity.getResources().getDisplayMetrics());

        mVisualCenterOfTextY =
                calculateVisualCenterOfTextY(
                        themedContext,
                        R.style.TextAppearance_BrowserUIListMenuItem,
                        mIconSize,
                        mRowHeight);
        mVisualCenterOfTextYIncognito =
                calculateVisualCenterOfTextY(
                        themedContext,
                        R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                        mIconSize,
                        mRowHeight);
    }

    /**
     * Calculates the visual center of a text appearance relative to the icon area.
     *
     * @param context The {@link Context} to use.
     * @param textAppearance The style resource for the text.
     * @param iconSize The size of the icon area.
     * @param rowHeight The height of the menu item row.
     * @return The Y coordinate of the visual center of the text.
     */
    private static float calculateVisualCenterOfTextY(
            Context context, int textAppearance, int iconSize, int rowHeight) {
        TextView textView = new TextView(context);
        textView.setTextAppearance(textAppearance);
        // Set text to ensure measure() and getBaseline() return accurate values.
        textView.setText("x");

        Rect bounds = new Rect();
        textView.getPaint().getTextBounds("x", 0, 1, bounds);
        // Visual center of text relative to its baseline.
        float visualCenterOffset = (bounds.top + bounds.bottom) / 2.0f;

        textView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        int tvBaseline = textView.getBaseline();
        int tvHeight = textView.getMeasuredHeight();

        // Android's LinearLayout with center_vertical floors the top margin.
        float textTopInRow = (float) Math.floor((rowHeight - tvHeight) / 2.0f);
        float iconTopInRow = (float) Math.floor((rowHeight - iconSize) / 2.0f);

        // Visual center relative to the row top.
        float visualCenterInRow = textTopInRow + tvBaseline + visualCenterOffset;

        // Return visual center relative to the icon area top.
        return (float) Math.floor(visualCenterInRow - iconTopInRow);
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
     */
    public static TabContextMenuCoordinator createContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            BiConsumer<AnchorInfo, Boolean> reorderFunction,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager) {
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
                modalDialogManager);
    }

    @VisibleForTesting
    static OnItemClickedCallback<AnchorInfo> getMenuItemClickedCallback(
            Supplier<TabModel> tabModelSupplier,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            TabGroupCreationCallback tabGroupCreationCallback,
            MultiInstanceManager multiInstanceManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            @Nullable ModalDialogManager modalDialogManager) {
        return (menuId, anchorInfo, collaborationId, listViewTouchTracker) -> {
            List<Integer> tabIds = anchorInfo.getAllTabIds();
            assert !tabIds.isEmpty() : "Empty tab id list provided";
            TabModel tabModel = tabModelSupplier.get();
            List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, /* allowClosing= */ false);
            assert !tabs.isEmpty() : "Empty tab list provided";
            recordMenuAction(menuId, tabs.size() > 1, tabModel.isIncognitoBranded());

            if (menuId == R.id.add_to_tab_group) {
                addToTabGroupItemCallback(tabGroupListBottomSheetCoordinator, tabs);
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
                closeTabItemCallback(tabModel, tabs, listViewTouchTracker);
            } else if (menuId == R.id.close_all_tabs_menu_id
                    || menuId == R.id.close_all_incognito_tabs_menu_id) {
                closeAllTabsItemCallback(tabModel);
            } else if (menuId == R.id.close_other_tabs_menu_id) {
                closeOtherTabsItemCallback(tabModel, tabIds);
            } else if (menuId == R.id.close_tabs_to_the_right_menu_id) {
                closeTabsToTheRightItemCallback(tabModel, tabIds);
            } else if (menuId == R.id.new_tab_to_the_right_menu_id) {
                newTabToTheRightItemCallback(tabModel, anchorInfo);
            } else if (menuId == R.id.add_tab_to_reading_list_menu_id) {
                addTabToReadingListItemCallback(tabBookmarkerSupplier, tabs);
            } else if (menuId == R.id.send_to_your_devices_menu_id) {
                sendToYourDevicesItemCallback(
                        tabModel,
                        anchorInfo,
                        windowAndroid,
                        activity,
                        snackbarManager,
                        activityResultTracker,
                        modalDialogManager);
            } else if (menuId == R.id.show_tabs_vertically_menu_id) {
                // Click/tab behavior will be added in a follow-up.
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
            @Nullable ListViewTouchTracker listViewTouchTracker) {
        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(tabs)
                                .allowUndo(allowUndo)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    private static void closeAllTabsItemCallback(TabModel tabModel) {
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    private static void closeOtherTabsItemCallback(TabModel tabModel, List<Integer> tabIds) {
        List<Tab> otherTabs = new ArrayList<>();
        for (Tab tab : tabModel) {
            if (!tabIds.contains(tab.getId())) {
                otherTabs.add(tab);
            }
        }
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(otherTabs)
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    private static void closeTabsToTheRightItemCallback(TabModel tabModel, List<Integer> tabIds) {
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
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(otherTabs)
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
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
            tabModel.getTabCreator()
                    .createNewTab(
                            new LoadUrlParams(urlConstantResolver.getNtpUrl()),
                            TabLaunchType.FROM_CHROME_UI,
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

    private static void sendToYourDevicesItemCallback(
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
                        snackbarManager);
        sttsCoordinator.show();
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
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            itemList.add(createNewTabToTheRightItem(isIncognito));
        }
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
        if (ShareUtils.shouldEnableShare(tabs.get(0))) {
            // Share is only available for single tab selection.
            itemList.add(createShareItem(isIncognito));
        }
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            itemList.add(createDuplicateTabsItem(isIncognito));
        }
        itemList.add(createPinUnpinTabItem(tabs, isIncognito));
        itemList.add(createMuteUnmuteSiteItem(tabs, isIncognito));
        if (ChromeFeatureList.sAndroidContextMenuDisabledMenuItems.isEnabled() && !isIncognito) {
            itemList.add(createAddTabToReadingListItem(anchorInfo));
        }
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled() && !isIncognito) {
            if (shouldShowSendToYourDevicesItem(tabs.get(0))) {
                itemList.add(createSendToYourDevicesItem());
            }
        }
        addVerticalTabsItems(itemList, isIncognito);
        itemList.add(createCloseItem(isIncognito));
        if (!ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            itemList.add(createCloseAllTabsItem(isIncognito));
        }
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            if (getTabModel().getCount() > 1) {
                itemList.add(createCloseOtherTabsItem(isIncognito));
            }
            if (canCloseTabsToTheRight(anchorInfo)) {
                itemList.add(createCloseTabsToTheRightItem(isIncognito));
            }
        }
    }

    private void buildMenuActionItemsForMultipleTabs(
            ModelList itemList, AnchorInfo anchorInfo, List<Tab> tabs, boolean isIncognito) {
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            itemList.add(createNewTabToTheRightItem(isIncognito));
        }
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
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            itemList.add(createDuplicateTabsItem(isIncognito));
        }
        itemList.add(createPinUnpinTabItem(tabs, isIncognito));
        itemList.add(createMuteUnmuteSiteItem(tabs, isIncognito));

        if (ChromeFeatureList.sAndroidContextMenuDisabledMenuItems.isEnabled() && !isIncognito) {
            itemList.add(createAddTabToReadingListItem(anchorInfo));
        }
        addVerticalTabsItems(itemList, isIncognito);
        itemList.add(createCloseItem(isIncognito));
        if (ChromeFeatureList.sAndroidContextMenuNewActions.isEnabled()) {
            if (getTabModel().getCount() > anchorInfo.getAllTabIds().size()) {
                itemList.add(createCloseOtherTabsItem(isIncognito));
            }
            if (canCloseTabsToTheRight(anchorInfo)) {
                itemList.add(createCloseTabsToTheRightItem(isIncognito));
            }
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

    private ListItem createNewTabToTheRightItem(boolean isIncognito) {
        String title = mActivity.getResources().getString(R.string.new_tab_to_the_right_menu_item);

        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.new_tab_to_the_right_menu_id)
                .withIsIncognito(isIncognito)
                .build();
    }

    private ListItem createMoveToTabGroupItem(List<Tab> tabs, boolean isIncognito) {
        // Available tab groups.
        @Nullable Token groupToNotBeIncluded = tabs.get(0).getTabGroupId();
        List<ListItem> potentialGroups =
                isIncognito
                        ? getIncognitoTabGroups(tabs, groupToNotBeIncluded)
                        : getRegularTabGroups(tabs, groupToNotBeIncluded);

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
                                            isIncognito);
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
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_group_menu_item, tabs.size());
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

    private ListItem createCloseTabsToTheRightItem(boolean isIncognito) {
        String title =
                mActivity.getResources().getString(R.string.close_tabs_to_the_right_menu_item);
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

    private boolean shouldShowSendToYourDevicesItem(Tab tab) {
        GURL url = tab.getUrl();
        if (url == null || url.isEmpty()) return false;

        Profile profile = getTabModel().getProfile();
        if (profile == null) return false;

        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(profile, url.getSpec());
        return displayReason != null;
    }

    private ListItem createSendToYourDevicesItem() {
        String title = mActivity.getResources().getString(R.string.send_to_your_devices_menu_item);

        return new ListItemBuilder()
                .withTitle(title)
                .withMenuId(R.id.send_to_your_devices_menu_id)
                .build();
    }

    private void addVerticalTabsItems(ModelList itemList, boolean isIncognito) {
        if (VerticalTabUtils.shouldShowVerticalTabsEntryPoint(mActivity)) {
            itemList.add(buildMenuDivider(isIncognito));
            itemList.add(
                    buildListItem(
                            R.string.show_tabs_vertically,
                            R.id.show_tabs_vertically_menu_id,
                            isIncognito));
            itemList.add(buildMenuDivider(isIncognito));
        }
    }

    private ListItem createCloseItem(boolean isIncognito) {
        return buildListItem(R.string.close, R.id.close_tab, isIncognito);
    }

    private ListItem createCloseAllTabsItem(boolean isIncognito) {
        int stringRes =
                isIncognito ? R.string.menu_close_all_incognito_tabs : R.string.menu_close_all_tabs;
        int menuRes =
                isIncognito ? R.id.close_all_incognito_tabs_menu_id : R.id.close_all_tabs_menu_id;
        return buildListItem(stringRes, menuRes, isIncognito);
    }

    private static void recordMenuAction(int menuId, boolean isMultipleTabs, boolean isIncognito) {
        if (menuId == R.id.add_to_tab_group) {
            recordUserAction("AddToTabGroup", isMultipleTabs);
        } else if (menuId == R.id.add_to_new_tab_group) {
            recordUserAction("AddToNewTabGroup", isMultipleTabs);
        } else if (menuId == R.id.remove_from_tab_group) {
            recordUserAction("RemoveTabFromTabGroup", isMultipleTabs);
        } else if (menuId == R.id.move_to_other_window_menu_id) {
            if (MultiWindowUtils.getInstanceCount(getActiveInstanceTypeForProfileType(isIncognito))
                    == 1) {
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
        } else if (menuId == R.id.close_all_tabs_menu_id) {
            recordUserAction("CloseAllTabs", /* isMultipleTabs= */ false);
        } else if (menuId == R.id.close_all_incognito_tabs_menu_id) {
            recordUserAction("CloseAllIncognitoTabs", /* isMultipleTabs= */ false);
        } else if (menuId == R.id.close_other_tabs_menu_id) {
            recordUserAction("CloseOtherTabs", isMultipleTabs);
        } else if (menuId == R.id.close_tabs_to_the_right_menu_id) {
            recordUserAction("CloseTabsToTheRight", isMultipleTabs);
        } else if (menuId == R.id.new_tab_to_the_right_menu_id) {
            recordUserAction("NewTabToTheRight", /* isMultipleTabs= */ false);
        } else if (menuId == R.id.add_tab_to_reading_list_menu_id) {
            recordUserAction("AddTabToReadingList", isMultipleTabs);
        } else if (menuId == R.id.send_to_your_devices_menu_id) {
            recordUserAction("SendToYourDevices", false);
        } else if (menuId == R.id.show_tabs_vertically_menu_id) {
            // Force false since switching to a vertical layout is a global UI state toggle
            // and doesn't benefit from distinguishing single vs multi-tab context.
            recordUserAction("ShowTabsVertically", /* isMultipleTabs= */ false);
        } else {
            assert false : "Unknown menu id: " + menuId;
        }
    }

    private List<ListItem> getRegularTabGroups(
            List<Tab> tabs, @Nullable Token groupToNotBeIncluded) {
        GroupWindowChecker windowChecker =
                new GroupWindowChecker(mTabGroupSyncService, getTabModel());
        List<SavedTabGroup> sortedTabGroups =
                windowChecker.getSortedGroupList(
                        groupWindowState ->
                                groupWindowState != IN_CURRENT_CLOSING
                                        && groupWindowState != GroupWindowState.HIDDEN,
                        (a, b) -> Long.compare(b.updateTimeMs, a.updateTimeMs));

        List<ListItem> result = new ArrayList<>();

        Set<Integer> activeInstanceIds = MultiWindowUtils.getUsableInstanceIds(ACTIVE);
        for (SavedTabGroup tabGroup : sortedTabGroups) {
            if (tabGroup.localId == null) continue;
            if (Objects.equals(groupToNotBeIncluded, tabGroup.localId.tabGroupId)) {
                continue;
            }
            Token groupId = tabGroup.localId.tabGroupId;

            TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
            @WindowId int windowId = tabWindowManager.findWindowIdForTabGroup(groupId);
            assumeNonNull(mMultiInstanceManager);
            boolean isGroupInCurrentWindow =
                    windowId == mMultiInstanceManager.getCurrentInstanceId();
            if (!activeInstanceIds.contains(windowId)) {
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
                        recordMenuAction(R.id.add_to_group_sub_menu_id, tabs.size() > 1, false);
                        if (isGroupInCurrentWindow) {
                            // If the tab is already in the current window,
                            // then just merge it to the group.
                            mergeTabsToDest(
                                    tabs,
                                    firstTabInGroupTabId,
                                    getTabModel(),
                                    /* tabMovedCallback= */ null);
                        } else {
                            mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                                    windowId,
                                    tabs,
                                    /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                                    /* destGroupTabId= */ firstTabInGroupTabId,
                                    /* bringToFront= */ true);
                        }
                    };
            result.add(
                    new ListItemBuilder()
                            .withTitle(label)
                            .withClickListener(clickListener)
                            .withIsIncognito(false)
                            .withStartIconDrawable(getCircleDrawable(colorId, false))
                            .withStartIconWidth(mCircleSize)
                            .withShouldTintIcon(false)
                            .build());
        }
        return result;
    }

    private List<ListItem> getIncognitoTabGroups(
            List<Tab> tabs, @Nullable Token groupToNotBeIncluded) {
        List<ListItem> result = new ArrayList<>();
        for (Token groupId : getTabModel().getAllTabGroupIds()) {
            if (Objects.equals(groupToNotBeIncluded, groupId)) {
                continue;
            }

            int tabIdInGroup = getTabModel().getGroupLastShownTabId(groupId);
            OnClickListener clickListener =
                    (v) -> {
                        recordMenuAction(
                                R.id.add_to_group_incognito_sub_menu_id, tabs.size() > 1, true);
                        mergeTabsToDest(
                                tabs, tabIdInGroup, getTabModel(), /* tabMovedCallback= */ null);
                    };
            result.add(
                    new ListItemBuilder()
                            .withTitle(
                                    TabGroupTitleUtils.getDisplayableTitle(
                                            mActivity, getTabModel(), groupId))
                            .withClickListener(clickListener)
                            .withIsIncognito(true)
                            .withStartIconDrawable(
                                    getCircleDrawable(
                                            getTabModel().getTabGroupColor(groupId), true))
                            .withStartIconWidth(mCircleSize)
                            .withShouldTintIcon(false)
                            .build());
        }
        return result;
    }

    private @Nullable Drawable getCircleDrawable(
            @TabGroupColorId int colorId, boolean isIncognito) {
        Drawable sourceDrawable = mActivity.getDrawable(R.drawable.tab_group_dialog_color_icon);

        if (sourceDrawable == null) return null;

        GradientDrawable circleDrawable = (GradientDrawable) sourceDrawable.mutate();
        @ColorInt int color = getTabGroupColorPickerItemColor(mActivity, colorId, isIncognito);
        circleDrawable.setColor(color);

        circleDrawable.setSize(mCircleSize, mCircleSize);

        // Center the circle on the appropriate visual center.
        float visualCenterOfTextY =
                isIncognito ? mVisualCenterOfTextYIncognito : mVisualCenterOfTextY;
        float topInsetFloat = visualCenterOfTextY - (mCircleSize / 2.0f);
        int topInset = (int) Math.floor(topInsetFloat);
        int bottomInset = (int) Math.ceil(mIconSize - (topInsetFloat + mCircleSize));
        int leftInset = 0;
        int rightInset = 0;

        return new InsetDrawable(circleDrawable, leftInset, topInset, rightInset, bottomInset);
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
                tabModel.isIncognitoBranded());
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
                tabModel.isIncognitoBranded());
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
        return createReorderItems(
                anchorInfo,
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.move_tabs_left, anchorInfo.getAllTabIds().size()),
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.move_tabs_right, anchorInfo.getAllTabIds().size()),
                isIncognito);
    }

    /** Ungroups any tabs in {@param tabs} which are currently in a group. */
    private void ungroupTabs(List<Tab> tabs) {
        List<Tab> groupedTabs = TabGroupUtils.getGroupedTabs(getTabModel(), tabs);
        if (!groupedTabs.isEmpty()) {
            // Ungroup all tabs before performing the move operation.
            getTabModel()
                    .getTabUngrouper()
                    .ungroupTabs(groupedTabs, /* trailing= */ true, /* allowDialog= */ false);
        }
    }

    private static void recordUserAction(String label, boolean isMultipleTabs) {
        String action = isMultipleTabs ? label + ".MultiTab" : label;
        RecordUserAction.record("MobileToolbarTabMenu." + action);
    }
}
