// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import androidx.core.util.Supplier;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesColor;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesType;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** Contains the logic to set the state of the model and react to actions. */
class TabGroupRowMediator {
    private final CallbackController mCallbackController = new CallbackController();
    private final Context mContext;
    private final SavedTabGroup mSavedTabGroup;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ModalDialogManager mModalDialogManager;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final Supplier<@GroupWindowState Integer> mFetchGroupState;
    private final PropertyModel mPropertyModel;

    private SharedImageTilesCoordinator mSharedImageTilesCoordinator;

    /**
     * @param context Used to load resources and create views.
     * @param tabGroupModelFilter Used to read current tab groups.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param dataSharingService Used to fetch shared group data.
     * @param collaborationService Used to fetch collaboration group data.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param modalDialogManager Used to show error dialogs.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param fetchGroupState Used to fetch which window the group is in.
     */
    public TabGroupRowMediator(
            Context context,
            SavedTabGroup savedTabGroup,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            DataSharingService dataSharingService,
            CollaborationService collaborationService,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ModalDialogManager modalDialogManager,
            ActionConfirmationManager actionConfirmationManager,
            FaviconResolver faviconResolver,
            Supplier<@GroupWindowState Integer> fetchGroupState) {
        mContext = context;
        mSavedTabGroup = savedTabGroup;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mPaneManager = paneManager;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mModalDialogManager = modalDialogManager;
        mActionConfirmationManager = actionConfirmationManager;
        mFetchGroupState = fetchGroupState;

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        int numberOfTabs = savedTabGroup.savedTabs.size();

        List<GURL> urlList = TabGroupFaviconCluster.buildUrlListFromSyncGroup(savedTabGroup);
        ClusterData clusterData = new ClusterData(faviconResolver, numberOfTabs, urlList);
        builder.with(TabGroupRowProperties.CLUSTER_DATA, clusterData);
        builder.with(TabGroupRowProperties.COLOR_INDEX, savedTabGroup.color);

        String userTitle = savedTabGroup.title;
        Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);

        builder.with(
                TabGroupRowProperties.TIMESTAMP_EVENT,
                new TabGroupTimeAgo(savedTabGroup.creationTimeMs, TimestampEvent.CREATED));
        builder.with(TabGroupRowProperties.OPEN_RUNNABLE, this::openGroup);
        builder.with(TabGroupRowProperties.ROW_CLICK_RUNNABLE, this::openGroup);
        builder.with(TabGroupRowProperties.DESTROYABLE, this::destroy);
        mPropertyModel = builder.build();

        String collaborationId = savedTabGroup.collaborationId;
        GroupData groupData = null;
        @GroupSharedState int sharedState = GroupSharedState.NOT_SHARED;
        if (mCollaborationService.getServiceStatus().isAllowedToJoin()
                && TabShareUtils.isCollaborationIdValid(savedTabGroup.collaborationId)) {
            groupData = mCollaborationService.getGroupData(collaborationId);
            sharedState = TabShareUtils.discernSharedGroupState(groupData);
        }
        setSharedProperties(sharedState, groupData, numberOfTabs);
    }

    /**
     * Note that this model may contain a {@link TabGroupRowProperties.DESTROYABLE} that needs to be
     * cleaned up.
     */
    public PropertyModel getModel() {
        return mPropertyModel;
    }

    private void destroy() {
        mCallbackController.destroy();
        if (mSharedImageTilesCoordinator != null) {
            mSharedImageTilesCoordinator.destroy();
            mSharedImageTilesCoordinator = null;
        }
    }

    private void setSharedProperties(
            @GroupSharedState int sharedState, @Nullable GroupData groupData, int numberOfTabs) {
        if (sharedState == GroupSharedState.NOT_SHARED) {
            mPropertyModel.set(DELETE_RUNNABLE, this::processDeleteGroup);
            mPropertyModel.set(LEAVE_RUNNABLE, null);
            mPropertyModel.set(TabGroupRowProperties.DISPLAY_AS_SHARED, false);
            mPropertyModel.set(TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW, null);
            return;
        }

        String collaborationId = groupData.groupToken.collaborationId;
        String groupTitle = groupTitleWithFallback(groupData, numberOfTabs);
        @MemberRole
        int memberRole = mCollaborationService.getCurrentUserRoleForGroup(collaborationId);
        if (memberRole == MemberRole.OWNER) {
            mPropertyModel.set(
                    DELETE_RUNNABLE, () -> processDeleteSharedGroup(groupTitle, collaborationId));
            mPropertyModel.set(LEAVE_RUNNABLE, null);
        } else {
            // TODO(crbug.com/365852281): Leave action should look like a delete if there are no
            // other users.
            mPropertyModel.set(DELETE_RUNNABLE, null);
            mPropertyModel.set(
                    LEAVE_RUNNABLE, () -> processLeaveGroup(groupTitle, collaborationId));
        }

        if (sharedState == GroupSharedState.COLLABORATION_ONLY) {
            mPropertyModel.set(TabGroupRowProperties.DISPLAY_AS_SHARED, false);
            mPropertyModel.set(TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW, null);
        } else if (sharedState == GroupSharedState.HAS_OTHER_USERS) {
            mPropertyModel.set(TabGroupRowProperties.DISPLAY_AS_SHARED, true);
            if (mSharedImageTilesCoordinator == null) {
                mSharedImageTilesCoordinator =
                        new SharedImageTilesCoordinator(
                                mContext,
                                SharedImageTilesType.DEFAULT,
                                new SharedImageTilesColor(SharedImageTilesColor.Style.DYNAMIC),
                                mDataSharingService,
                                mCollaborationService);
            }
            mSharedImageTilesCoordinator.fetchImagesForCollaborationId(
                    mSavedTabGroup.collaborationId);
            mPropertyModel.set(
                    TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW,
                    mSharedImageTilesCoordinator.getView());
        }
    }

    private void openGroup() {
        @GroupWindowState int state = mFetchGroupState.get();
        if (state == GroupWindowState.IN_ANOTHER) {
            return;
        }

        if (state == GroupWindowState.HIDDEN) {
            RecordUserAction.record("SyncedTabGroup.OpenNewLocal");
        } else {
            RecordUserAction.record("SyncedTabGroup.OpenExistingLocal");
        }

        SavedTabGroup savedTabGroup = mSavedTabGroup;
        if (state == GroupWindowState.IN_CURRENT_CLOSING) {
            for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
                if (savedTab.localId != null) {
                    mTabGroupModelFilter.getTabModel().cancelTabClosure(savedTab.localId);
                }
            }
        } else if (state == GroupWindowState.HIDDEN) {
            String syncId = savedTabGroup.syncId;
            mTabGroupUiActionHandler.openTabGroup(syncId);
            savedTabGroup = mTabGroupSyncService.getGroup(syncId);
        }

        if (savedTabGroup.localId == null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TabGroupSync.WindowStateOnFailedOpen", state, GroupWindowState.COUNT);
            return;
        }

        int rootId = mTabGroupModelFilter.getRootIdFromTabGroupId(savedTabGroup.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;
        mPaneManager.focusPane(PaneId.TAB_SWITCHER);
        TabSwitcherPaneBase tabSwitcherPaneBase =
                (TabSwitcherPaneBase) mPaneManager.getPaneForId(PaneId.TAB_SWITCHER);
        boolean success = tabSwitcherPaneBase.requestOpenTabGroupDialog(rootId);
        assert success;
    }

    private void processDeleteGroup() {
        @GroupWindowState int state = mFetchGroupState.get();
        if (state == GroupWindowState.HIDDEN) {
            // A hidden group needs to show a dialog here because the TabRemover is not used.
            mActionConfirmationManager.processDeleteGroupAttempt(
                    (@ActionConfirmationResult Integer result) -> {
                        if (result != ActionConfirmationResult.CONFIRMATION_NEGATIVE) {
                            // A dialog already happened so we can bypass it. We shouldn't assume
                            // the group is still in the HIDDEN state though so call deleteGroup and
                            // do whatever is appropriate based on the current state.
                            deleteGroup(/* allowDialog= */ false);
                        }
                    });
        } else {
            // TabRemover used in deleteGroup will handle the dialog if required.
            deleteGroup(/* allowDialog= */ true);
        }
    }

    private void processDeleteSharedGroup(String groupTitle, String collaborationId) {
        // TODO(crbug.com/365852281): Confirmation should look like a non-shared delete if there are
        // no other users.
        mActionConfirmationManager.processDeleteSharedGroupAttempt(
                groupTitle,
                (result) -> {
                    exitCollaborationWithoutWarningWrapper(
                            collaborationId, result, MemberRole.OWNER);
                });
    }

    private void processLeaveGroup(String groupTitle, String collaborationId) {
        // TODO(crbug.com/365852281): Confirmation should look like a non-shared delete if there are
        // no other users.
        mActionConfirmationManager.processLeaveGroupAttempt(
                groupTitle,
                (result) -> {
                    exitCollaborationWithoutWarningWrapper(
                            collaborationId, result, MemberRole.MEMBER);
                });
    }

    private void exitCollaborationWithoutWarningWrapper(
            String collaborationId,
            MaybeBlockingResult maybeBlockingResult,
            @MemberRole int memberRole) {
        if (maybeBlockingResult.result != ActionConfirmationResult.CONFIRMATION_NEGATIVE) {
            assert maybeBlockingResult.finishBlocking != null;
            TabUiUtils.exitCollaborationWithoutWarning(
                    mContext,
                    mModalDialogManager,
                    mCollaborationService,
                    collaborationId,
                    memberRole,
                    maybeBlockingResult.finishBlocking);
        } else if (maybeBlockingResult.finishBlocking != null) {
            assert false : "Should not be reachable.";
            // Do the safe thing and run the runnable anyway.
            maybeBlockingResult.finishBlocking.run();
        }
    }

    private void deleteGroup(boolean allowDialog) {
        @GroupWindowState int state = mFetchGroupState.get();
        if (state == GroupWindowState.IN_ANOTHER) {
            return;
        }

        if (state == GroupWindowState.HIDDEN) {
            RecordUserAction.record("SyncedTabGroup.DeleteWithoutLocal");
        } else {
            RecordUserAction.record("SyncedTabGroup.DeleteWithLocal");
        }

        if (state == GroupWindowState.IN_CURRENT_CLOSING) {
            // No need to show a dialog for this since the closure already started.
            for (SavedTabGroupTab savedTab : mSavedTabGroup.savedTabs) {
                if (savedTab.localId != null) {
                    mTabGroupModelFilter.getTabModel().commitTabClosure(savedTab.localId);
                }
            }
            // Because the pending closure might have been hiding or part of a closure containing
            // more tabs we need to forcibly remove the group.
            mTabGroupSyncService.removeGroup(mSavedTabGroup.syncId);
        } else if (state == GroupWindowState.IN_CURRENT) {
            int rootId =
                    mTabGroupModelFilter.getRootIdFromTabGroupId(mSavedTabGroup.localId.tabGroupId);
            mTabGroupModelFilter
                    .getTabModel()
                    .getTabRemover()
                    .closeTabs(
                            TabClosureParams.forCloseTabGroup(mTabGroupModelFilter, rootId)
                                    .allowUndo(false)
                                    .build(),
                            allowDialog);
        } else {
            assert !allowDialog : "A dialog should have already been shown.";
            mTabGroupSyncService.removeGroup(mSavedTabGroup.syncId);
        }
    }

    private String groupTitleWithFallback(GroupData groupData, int numberOfTabs) {
        String groupTitle = groupData.displayName;
        if (TextUtils.isEmpty(groupTitle)) {
            return TabGroupTitleUtils.getDefaultTitle(mContext, numberOfTabs);
        } else {
            return groupTitle;
        }
    }
}
