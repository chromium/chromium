// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** Handles the actions for the tab group related menu items in the app menu. */
@NullMarked
public class TabGroupMenuActionHandler {
    private final Context mContext;
    private final TabModel mTabModel;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final @Nullable TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final Profile mProfile;
    private final TabGroupListBottomSheetCoordinatorFactory mFactory;

    private final SettableMonotonicObservableSupplier<TabGroupListBottomSheetCoordinator>
            mTabGroupListBottomSheetCoordinatorSupplier = ObservableSuppliers.createMonotonic();

    /**
     * @param context The context for the app menu.
     * @param tabModel Used to interact with tab groups.
     * @param bottomSheetController For interacting with the bottom sheet.
     * @param modalDialogManager For showing the tab group creation dialog.
     * @param uiActionHandler For UI actions on tab groups.
     * @param profile The current profile.
     */
    public TabGroupMenuActionHandler(
            Context context,
            TabModel tabModel,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            @Nullable TabGroupUiActionHandler uiActionHandler,
            Profile profile) {
        this(
                context,
                tabModel,
                bottomSheetController,
                modalDialogManager,
                uiActionHandler,
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile),
                profile,
                TabGroupListBottomSheetCoordinator::new);
    }

    @VisibleForTesting
    TabGroupMenuActionHandler(
            Context context,
            TabModel tabModel,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            @Nullable TabGroupUiActionHandler uiActionHandler,
            @Nullable TabGroupSyncService syncService,
            Profile profile,
            TabGroupListBottomSheetCoordinatorFactory factory) {
        mContext = context;
        mTabModel = tabModel;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mTabGroupUiActionHandler = uiActionHandler;
        mTabGroupSyncService = syncService;
        mProfile = profile;
        mFactory = factory;
    }

    /**
     * Handles the "Add to group" action for the given tab.
     *
     * <p>If no tab groups exist, it creates a new group with the given tab.
     *
     * <p>If tab groups exist, it shows a bottom sheet allowing the user to select an existing group
     * or create a new one.
     *
     * @param tab The tab to be added to a group.
     * @return Whether existing groups were present and a bottom sheet was shown (true), or false if
     *     no other groups were present and a new group was created directly.
     */
    public boolean handleAddToGroupAction(Tab tab) {
        GroupWindowChecker windowChecker =
                new GroupWindowChecker(mContext, mTabGroupSyncService, mTabModel);
        if (!windowChecker.hasOtherGroups(tab.getTabGroupId())) {
            TabGroupUtils.createNewGroupForTabs(
                    List.of(tab),
                    mTabModel,
                    /* tabMovedCallback= */ null,
                    this::onTabGroupCreation);
            return false;
        } else {
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator =
                    mFactory.create(
                            mContext,
                            mProfile,
                            this::onTabGroupCreation,
                            /* tabMovedCallback= */ null,
                            mTabModel,
                            mBottomSheetController,
                            /* supportsShowNewGroup= */ true,
                            /* destroyOnHide= */ true,
                            tab.getWindowAndroid(),
                            mTabGroupUiActionHandler);
            mTabGroupListBottomSheetCoordinatorSupplier.set(tabGroupListBottomSheetCoordinator);
            tabGroupListBottomSheetCoordinator.showBottomSheet(List.of(tab));
            return true;
        }
    }

    /**
     * Called after a tab group creation event. Shows the tab group creation dialog and cleans
     * unused classes if required.
     */
    private void onTabGroupCreation(Token tabGroupId) {
        TabGroupCreationDialogManager manager =
                new TabGroupCreationDialogManager(mContext, mModalDialogManager, null);
        manager.showDialog(tabGroupId, mTabModel);
    }

    /**
     * Handles the "Add to existing group" action for the given tab and group ID.
     *
     * @param tab The tab to be added to an existing group.
     * @param groupId The target tab group ID.
     * @param syncGroupId The target sync group ID for remote groups.
     * @return Whether the action was handled.
     */
    public boolean handleAddToExistingGroupAction(
            Tab tab, @Nullable Token groupId, @Nullable String syncGroupId) {
        RecordUserAction.record("MobileMenuAddToExistingGroup");
        GroupWindowInfo destinationGroup =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncGroupId);
        if (!TabGroupUiUtils.isValidDestination(
                destinationGroup, mTabGroupSyncService, mTabGroupUiActionHandler)) {
            return false;
        }

        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(tab),
                destinationGroup,
                mTabGroupSyncService,
                mTabGroupUiActionHandler,
                /* tabMovedCallback= */ null,
                /* bringToFront= */ true);
        return true;
    }

    /**
     * Handles the "Open group" action for the given sync group ID.
     *
     * @param syncGroupId The target sync group ID for remote groups.
     * @return Whether the action was handled.
     */
    public boolean handleOpenGroupAction(@Nullable String syncGroupId) {
        if (syncGroupId == null) {
            return false;
        }
        RecordUserAction.record("MobileMenuOpenGroup");
        TabGroupUiUtils.openTabGroup(
                mContext, mTabModel, mTabGroupSyncService, mTabGroupUiActionHandler, syncGroupId);
        return true;
    }

    /**
     * Handles the "Open group in new window" action for the given tab group.
     *
     * @param groupId The local tab group ID.
     * @param syncGroupId The target sync group ID for remote groups.
     * @param multiInstanceManager The {@link MultiInstanceManager} used to open in a new window.
     * @return Whether the action was handled.
     */
    public boolean handleOpenGroupInNewWindowAction(
            @Nullable Token groupId,
            @Nullable String syncGroupId,
            MultiInstanceManager multiInstanceManager) {
        GroupWindowInfo groupInfo =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncGroupId);
        if (groupInfo == null) {
            return false;
        }
        RecordUserAction.record("MobileMenuOpenGroupInNewWindow");
        return TabGroupUiUtils.openTabGroupInNewWindow(
                mTabModel,
                groupInfo,
                mTabGroupSyncService,
                mTabGroupUiActionHandler,
                multiInstanceManager);
    }

    /**
     * Handles the "Delete group" action for the given tab group.
     *
     * @param groupId The local tab group ID.
     * @param syncGroupId The target sync group ID for remote groups.
     * @return Whether the action was handled.
     */
    public boolean handleDeleteGroupAction(@Nullable Token groupId, @Nullable String syncGroupId) {
        GroupWindowInfo groupInfo =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncGroupId);
        if (groupInfo == null) {
            return false;
        }
        RecordUserAction.record("MobileMenuDeleteGroup");
        @GroupWindowState int state = groupInfo.groupWindowState;
        if (!shouldConfirmDeleteGroup(state)) {
            TabGroupUiUtils.deleteTabGroup(
                    mTabModel, mTabGroupSyncService, groupInfo, state, /* allowDialog= */ true);
            return true;
        }
        new ActionConfirmationManager(mProfile, mContext, mModalDialogManager)
                .processDeleteGroupAttempt(
                        result -> onDeleteGroupConfirmed(result, groupId, syncGroupId));
        return true;
    }

    private static boolean shouldConfirmDeleteGroup(@GroupWindowState int state) {
        return state == GroupWindowState.HIDDEN
                || (state == GroupWindowState.IN_ANOTHER
                        && TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled());
    }

    private void onDeleteGroupConfirmed(
            @ActionConfirmationResult int result,
            @Nullable Token groupId,
            @Nullable String syncGroupId) {
        if (result == ActionConfirmationResult.CONFIRMATION_NEGATIVE) return;

        GroupWindowInfo updatedInfo =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncGroupId);
        if (updatedInfo == null) return;

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                updatedInfo,
                updatedInfo.groupWindowState,
                /* allowDialog= */ false);
    }
}
