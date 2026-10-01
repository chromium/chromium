// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesView;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogUtils;

import java.util.List;

/** Static utilities for Tab UI. */
@NullMarked
public class TabUiUtils {

    /**
     * Closes a tab group and maybe shows a confirmation dialog.
     *
     * @param tabModel The {@link TabModel} to act on.
     * @param tabId The ID of one of the tabs in the tab group.
     * @param tabClosingSource The tab closing source, e.g. the tablet tab strip.
     * @param allowUndo Whether to allow undo of the tab group closure.
     * @param hideTabGroups Whether to hide or delete the tab group.
     * @param didCloseCallback Run after the close confirmation to indicate if a close happened.
     */
    public static void closeTabGroup(
            TabModel tabModel,
            int tabId,
            @TabClosingSource int tabClosingSource,
            boolean allowUndo,
            boolean hideTabGroups,
            @Nullable Callback<Boolean> didCloseCallback) {
        @Nullable Tab tab = tabModel.getTabById(tabId);
        if (tab == null) {
            Callback.runNullSafe(didCloseCallback, false);
            return;
        }
        TabClosureParams.Builder builder =
                TabClosureParams.forCloseTabGroup(tabModel, tab.getTabGroupId());
        if (builder == null) {
            Callback.runNullSafe(didCloseCallback, false);
            return;
        }
        TabClosureParams closureParams =
                builder.hideTabGroups(hideTabGroups)
                        .allowUndo(allowUndo)
                        .tabClosingSource(tabClosingSource)
                        .build();

        @Nullable TabModelActionListener listener = buildMaybeDidCloseTabListener(didCloseCallback);
        tabModel.getTabRemover().closeTabs(closureParams, /* allowDialog= */ true, listener);
    }

    /**
     * Returns a {@link TabModelActionListener} that will invoke {@code didCloseCallback} with
     * whether tabs were closed or null if {@code didCloseCallback} is null.
     */
    public static @Nullable TabModelActionListener buildMaybeDidCloseTabListener(
            @Nullable Callback<Boolean> didCloseCallback) {
        if (didCloseCallback == null) return null;

        return new TabModelActionListener() {
            @Override
            public void onConfirmationDialogResult(
                    @DialogType int dialogType, @ActionConfirmationResult int result) {
                // Cases:
                // - DialogType.NONE: will always close tabs as no interrupt happened.
                // - DialogType.COLLABORATION: will always perform the action. The dialog is used to
                //   confirm if the group should remain.
                // - DialogType.SYNC: a dialog interrupts the flow. If the action was positive the
                //   tabs will be closed.
                // The goal is to differentiate whether tabs were closed which can be inferred from
                // the combination of `dialogType` and `result`.
                boolean didCloseTabs =
                        dialogType != DialogType.SYNC
                                || result != ActionConfirmationResult.CONFIRMATION_NEGATIVE;
                didCloseCallback.onResult(didCloseTabs);
            }
        };
    }

    /**
     * Ungroups a tab group and maybe shows a confirmation dialog.
     *
     * @param tabModel The {@link TabModel} to act on.
     * @param tabGroupId The id of the tab group.
     */
    public static void ungroupTabGroup(TabModel tabModel, Token tabGroupId) {
        if (!tabModel.tabGroupExists(tabGroupId)) return;

        tabModel.getTabUngrouper()
                .ungroupTabGroup(tabGroupId, /* trailing= */ false, /* allowDialog= */ true);
    }

    /**
     * Reorders an entire tab group in the given direction (up/previous or down/next).
     *
     * @param tabModel The active {@link TabModel}.
     * @param groupId The {@link Token} ID of the tab group.
     * @param toPrevious Whether to move to previous (up) or next (down).
     * @return Whether the group was successfully reordered.
     */
    public static boolean reorderTabGroup(
            @Nullable TabModel tabModel, Token groupId, boolean toPrevious) {
        if (tabModel == null) return false;
        int newIndex =
                getIndexPastAdjacentTabOrGroup(
                        tabModel, tabModel.getTabsInGroup(groupId), toPrevious);
        if (newIndex == TabModel.INVALID_TAB_INDEX) return false;
        tabModel.moveGroupToIndex(groupId, newIndex);
        return true;
    }

    /**
     * Returns the {@link TabModel} index to move {@code tabsToMove} to so it jumps past the
     * adjacent tab or tab group, or {@link TabModel#INVALID_TAB_INDEX} if there is no adjacent item
     * on the same side of the pinned boundary.
     *
     * @param tabModel The {@link TabModel} containing the tabs.
     * @param tabsToMove The contiguous tabs being moved, either a whole tab group or one tab.
     * @param toPrevious Whether to move earlier (up / previous) or later (down / next).
     * @return The target {@link TabModel} index, or {@link TabModel#INVALID_TAB_INDEX} if the move
     *     is invalid.
     */
    static int getIndexPastAdjacentTabOrGroup(
            TabModel tabModel, List<Tab> tabsToMove, boolean toPrevious) {
        if (tabsToMove.isEmpty()) return TabModel.INVALID_TAB_INDEX;
        int boundaryIndex =
                toPrevious
                        ? TabGroupUtils.getFirstTabModelIndexForList(tabModel, tabsToMove)
                        : TabGroupUtils.getLastTabModelIndexForList(tabModel, tabsToMove);
        if (boundaryIndex == TabModel.INVALID_TAB_INDEX) return TabModel.INVALID_TAB_INDEX;
        int adjacentIndex = toPrevious ? boundaryIndex - 1 : boundaryIndex + 1;
        Tab adjacentTab = tabModel.getTabAt(adjacentIndex);
        // Never jump across the pinned / unpinned boundary. The model would clamp it to a no-op.
        if (adjacentTab == null || adjacentTab.getIsPinned() != tabsToMove.get(0).getIsPinned()) {
            return TabModel.INVALID_TAB_INDEX;
        }
        Token adjacentGroupId = adjacentTab.getTabGroupId();
        if (adjacentGroupId == null) return adjacentIndex;

        List<Tab> adjacentGroup = tabModel.getTabsInGroup(adjacentGroupId);
        if (adjacentGroup.isEmpty()) return TabModel.INVALID_TAB_INDEX;
        return toPrevious
                ? TabGroupUtils.getFirstTabModelIndexForList(tabModel, adjacentGroup)
                : TabGroupUtils.getLastTabModelIndexForList(tabModel, adjacentGroup);
    }

    /**
     * Update the tab group color.
     *
     * @param tabModel The {@link TabModel} to act on.
     * @param tabGroupId The group id of the interacting tab group.
     * @param newGroupColor The new group color being assigned to the tab group.
     * @return Whether the tab group color is updated.
     */
    public static boolean updateTabGroupColor(
            TabModel tabModel, Token tabGroupId, @TabGroupColorId int newGroupColor) {
        if (!tabModel.tabGroupExists(tabGroupId)) return false;

        int curGroupColor = tabModel.getTabGroupColor(tabGroupId);
        if (curGroupColor != newGroupColor) {
            tabModel.setTabGroupColor(tabGroupId, newGroupColor);
            return true;
        }
        return false;
    }

    /**
     * Update the tab group title.
     *
     * @param tabModel The {@link TabModel} to act on.
     * @param tabGroupId The group id of the interacting tab group.
     * @param newGroupTitle The new group title being assigned to the tab group.
     * @return Whether the tab group title is updated.
     */
    public static boolean updateTabGroupTitle(
            TabModel tabModel, Token tabGroupId, String newGroupTitle) {
        assert newGroupTitle != null;
        if (!tabModel.tabGroupExists(tabGroupId)) return false;

        String curGroupTitle = tabModel.getTabGroupTitle(tabGroupId);
        if (!newGroupTitle.equals(curGroupTitle)) {
            tabModel.setTabGroupTitle(tabGroupId, newGroupTitle);
            return true;
        }
        return false;
    }

    /**
     * Returns whether an IPH should be shown for Tab Group Sync for the given tab group ID.
     *
     * @param tabGroupSyncService The sync service to get tab group data form.
     * @param tabGroupId The local tab group ID.
     * @return Whether to show Tab Group Sync IPH.
     */
    public static boolean shouldShowIphForSync(
            @Nullable TabGroupSyncService tabGroupSyncService, @Nullable Token tabGroupId) {
        if (tabGroupSyncService == null || tabGroupId == null) return false;

        @Nullable SavedTabGroup savedTabGroup =
                tabGroupSyncService.getGroup(new LocalTabGroupId(tabGroupId));
        // Don't try to show the IPH if the group is:
        // 1) Not in TabGroupSyncService for some reason.
        // 2) A shared tab group.
        // 3) Created locally.
        if (savedTabGroup == null
                || TabShareUtils.isCollaborationIdValid(savedTabGroup.collaborationId)
                || !tabGroupSyncService.isRemoteDevice(savedTabGroup.creatorCacheGuid)) {
            return false;
        }
        return true;
    }

    /**
     * Leaves or deletes a given collaboration.
     *
     * @param context Used to load resources.
     * @param modalDialogManager Used to show error dialogs.
     * @param collaborationService Called to do the actual leave or delete action.
     * @param collaborationId Used to identify the collaboration.
     * @param memberRole Used to decide which way to exit the group.
     * @param finishedRunnable Invoked when the server RPC is complete.
     */
    public static void exitCollaborationWithoutWarning(
            Context context,
            ModalDialogManager modalDialogManager,
            CollaborationService collaborationService,
            String collaborationId,
            @MemberRole int memberRole,
            @Nullable Runnable finishedRunnable) {
        Callback<Boolean> callback =
                bindOnLeaveOrDeleteGroup(context, modalDialogManager, finishedRunnable);
        if (memberRole == MemberRole.OWNER) {
            collaborationService.deleteGroup(collaborationId, callback);
        } else if (memberRole == MemberRole.MEMBER) {
            collaborationService.leaveGroup(collaborationId, callback);
        } else {
            showGenericErrorDialog(context, modalDialogManager);
        }
    }

    /**
     * Create share flows to initiate tab group share.
     *
     * @param activity that contains the current tab group.
     * @param tabModel The {@link TabModel} to act on.
     * @param dataSharingTabManager The {@link DataSharingTabManager} managing communication between
     *     UI and DataSharing services.
     * @param tabId The local id of the tab.
     * @param tabGroupDisplayName The display name of the current group title.
     */
    public static void startShareTabGroupFlow(
            Activity activity,
            TabModel tabModel,
            DataSharingTabManager dataSharingTabManager,
            int tabId,
            String tabGroupDisplayName,
            @CollaborationServiceShareOrManageEntryPoint int entry) {
        Tab tab = tabModel.getTabById(tabId);
        // The tab may have been closed in parallel with the share starting. Skip if this happens.
        if (tab == null) return;

        LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
        if (localTabGroupId == null) return;

        dataSharingTabManager.createOrManageFlow(
                EitherGroupId.createLocalId(localTabGroupId), entry, CallbackUtils.emptyCallback());
    }

    /**
     * Attaches an {@link SharedImageTilesCoordinator} to a {@link FrameLayout}.
     *
     * @param sharedImageTilesCoordinator The {@link SharedImageTilesCoordinator} to attach.
     * @param container The {@link FrameLayout} to attach to.
     */
    public static void attachSharedImageTilesCoordinatorToFrameLayout(
            SharedImageTilesCoordinator sharedImageTilesCoordinator, FrameLayout container) {
        attachSharedImageTilesViewToFrameLayout(sharedImageTilesCoordinator.getView(), container);
    }

    /**
     * {@link #attachSharedImageTilesCoordinatorToFrameLayout(SharedImageTilesCoordinator,
     * FrameLayout)}
     */
    public static void attachSharedImageTilesViewToFrameLayout(
            SharedImageTilesView imageTilesView, FrameLayout container) {
        var layoutParams =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER);
        container.addView(imageTilesView, layoutParams);
    }

    private static Callback<Boolean> bindOnLeaveOrDeleteGroup(
            Context context,
            ModalDialogManager modalDialogManager,
            @Nullable Runnable finishedRunnable) {
        return (Boolean success) -> {
            // Invoke the runnable first since it may be necessary to hide the prior dialog before
            // showing the error.
            if (finishedRunnable != null) finishedRunnable.run();

            if (!Boolean.TRUE.equals(success)) {
                showGenericErrorDialog(context, modalDialogManager);
            }
        };
    }

    /**
     * Shows a generic error when a data sharing action fails.
     *
     * @param context Used to load resources.
     * @param modalDialogManager Used to show the dialog.
     */
    public static void showGenericErrorDialog(
            Context context, ModalDialogManager modalDialogManager) {
        ModalDialogUtils.showOneButtonConfirmation(
                modalDialogManager,
                context.getResources(),
                R.string.data_sharing_generic_failure_title,
                R.string.data_sharing_generic_failure_description,
                R.string.data_sharing_invitation_failure_button);
    }

    /**
     * Mark the tab switcher view as sensitive if at least one of the tabs in {@code tabList} has
     * sensitive content. Note that if all sensitive tabs are removed from the tab switcher, the tab
     * switcher will have to be closed and opened again to become not sensitive.
     *
     * @param tabList List of all tabs to be checked for sensitive content.
     * @param contentSensitivitySetter Function that sets the content sensitivity on the tab
     *     switcher view. The parameter of this function is a boolean, which is true if the content
     *     is sensitive.
     * @param histogram Boolean histogram that records the content sensitivity.
     */
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static void updateViewContentSensitivityForTabs(
            @Nullable TabList tabList,
            Callback<Boolean> contentSensitivitySetter,
            String histogram) {
        if (tabList == null) {
            return;
        }

        for (Tab tab : tabList) {
            if (tab.getTabHasSensitiveContent()) {
                contentSensitivitySetter.onResult(/* result= */ true);
                RecordHistogram.recordBooleanHistogram(histogram, /* sample= */ true);
                return;
            }
        }
        // If not marked as not sensitive, the tab switcher might remain sensitive from a previous
        // set of tabs.
        contentSensitivitySetter.onResult(/* result= */ false);
        RecordHistogram.recordBooleanHistogram(histogram, /* sample= */ false);
    }

    /** Returns whether any tabs have sensitive content. */
    public static boolean anySensitiveContent(List<Tab> tabs) {
        for (Tab tab : tabs) {
            if (tab.getTabHasSensitiveContent()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Mark the tab switcher view as sensitive if at least one of the tabs in {@code tabList} has
     * sensitive content. Note that if all sensitive tabs are removed from the tab switcher, the tab
     * switcher will have to be closed and opened again to become not sensitive.
     *
     * @param tabList List of all tabs to be checked for sensitive content.
     * @param contentSensitivitySetter Function that sets the content sensitivity on the tab
     *     switcher view. The parameter of this function is a boolean, which is true if the content
     *     is sensitive.
     * @param histogram Boolean histogram that records the content sensitivity.
     */
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static void updateViewContentSensitivityForTabs(
            @Nullable List<Tab> tabList,
            Callback<Boolean> contentSensitivitySetter,
            String histogram) {
        if (tabList == null) {
            return;
        }

        boolean isSensitive = anySensitiveContent(tabList);
        contentSensitivitySetter.onResult(isSensitive);
        RecordHistogram.recordBooleanHistogram(histogram, isSensitive);
    }

    /**
     * Returns whether the data sharing feature is allowed to be used. Returns true if the data
     * sharing or join only flag is enabled.
     */
    public static boolean isDataSharingFunctionalityEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_JOIN_ONLY);
    }

    /**
     * Returns the last updated timestamp for the {@link SavedTabGroup}, determined from the last
     * updated time on each {@link SavedTabGroupTab} within the group.
     *
     * @param savedTabGroup The saved tab group to retrieve the last updated timestamp for.
     */
    public static long getGroupLastUpdatedTimestamp(SavedTabGroup savedTabGroup) {
        long timestamp = 0;
        for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
            // TODO(crbug.com/432292097): Use navigation time from native when available.
            timestamp = Math.max(timestamp, savedTab.updateTimeMs);
        }
        return timestamp;
    }

    /**
     * Applies the backplate and adjusts the layout for the empty state view on XR devices.
     *
     * @param rootView The view containing the empty state UI.
     */
    public static void applyXrEmptyStateBackplate(View rootView) {
        if (!DeviceInfo.isXr()) {
            return;
        }

        View emptyStateContainer =
                rootView.findViewById(R.id.empty_state_container);
        View emptyStateIllustration =
                rootView.findViewById(R.id.empty_state_icon);

        if (emptyStateContainer instanceof ScrollView) {
            ScrollView scrollView = (ScrollView) emptyStateContainer;
            FrameLayout.LayoutParams scrollParams =
                    (FrameLayout.LayoutParams) scrollView.getLayoutParams();
            scrollParams.width = FrameLayout.LayoutParams.WRAP_CONTENT;
            scrollParams.height = FrameLayout.LayoutParams.WRAP_CONTENT;
            scrollParams.gravity = Gravity.CENTER;
            scrollView.setLayoutParams(scrollParams);
        }

        if (emptyStateIllustration != null
                && emptyStateIllustration.getParent() instanceof View) {
            View container = (View) emptyStateIllustration.getParent();
            container.setBackgroundResource(
                    R.drawable.xr_empty_state_backplate);
        }
    }


}
