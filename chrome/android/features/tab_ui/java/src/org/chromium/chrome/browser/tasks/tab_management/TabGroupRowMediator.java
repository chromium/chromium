// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import androidx.core.util.Supplier;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesColor;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Contains the logic to set the state of the model and react to actions. */
class TabGroupRowMediator {
    private final CallbackController mCallbackController = new CallbackController();
    private final Context mContext;
    private final SavedTabGroup mSavedTabGroup;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ModalDialogManager mModalDialogManager;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final LazyOneshotSupplier<CoreAccountInfo> mCoreAccountInfoSupplier;
    private final Supplier<Integer> mFetchGroupState;
    private final PropertyModel mPropertyModel;

    private SharedImageTilesCoordinator mSharedImageTilesCoordinator;

    /**
     * @param context Used to load resources and create views.
     * @param tabGroupModelFilter Used to read current tab groups.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param dataSharingService Used to fetch shared group data.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param modalDialogManager Used to show error dialogs.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param coreAccountInfoSupplier Used to fetch current account information.
     * @param fetchGroupState Used to fetch which window the group is in.
     */
    public TabGroupRowMediator(
            Context context,
            SavedTabGroup savedTabGroup,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            DataSharingService dataSharingService,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ModalDialogManager modalDialogManager,
            ActionConfirmationManager actionConfirmationManager,
            FaviconResolver faviconResolver,
            LazyOneshotSupplier<CoreAccountInfo> coreAccountInfoSupplier,
            Supplier<Integer> fetchGroupState) {
        mContext = context;
        mSavedTabGroup = savedTabGroup;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mPaneManager = paneManager;
        mDataSharingService = dataSharingService;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mModalDialogManager = modalDialogManager;
        mActionConfirmationManager = actionConfirmationManager;
        mCoreAccountInfoSupplier = coreAccountInfoSupplier;
        mFetchGroupState = fetchGroupState;

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        List<SavedTabGroupTab> savedTabs = savedTabGroup.savedTabs;
        int numberOfTabs = savedTabs.size();
        int urlCount = Math.min(TabGroupFaviconCluster.CORNER_COUNT, numberOfTabs);
        List<GURL> urlList = new ArrayList<>();
        for (int i = 0; i < urlCount; i++) {
            urlList.add(savedTabs.get(i).url);
        }

        ClusterData clusterData = new ClusterData(faviconResolver, numberOfTabs, urlList);
        builder.with(TabGroupRowProperties.CLUSTER_DATA, clusterData);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            builder.with(TabGroupRowProperties.COLOR_INDEX, savedTabGroup.color);
        }

        String userTitle = savedTabGroup.title;
        Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);

        builder.with(TabGroupRowProperties.CREATION_MILLIS, savedTabGroup.creationTimeMs);
        builder.with(TabGroupRowProperties.OPEN_RUNNABLE, this::openGroup);
        builder.with(TabGroupRowProperties.DESTROYABLE, this::destroy);
        mPropertyModel = builder.build();

        String collaborationId = savedTabGroup.collaborationId;
        if (mDataSharingService != null
                && ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                && TabShareUtils.isCollaborationIdValid(savedTabGroup.collaborationId)) {
            mDataSharingService.readGroup(
                    collaborationId, mCallbackController.makeCancelable(this::onReadGroup));
        } else {
            setSharedProperties(GroupSharedState.NOT_SHARED, /* groupData= */ null);
        }
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
        }
    }

    private void onReadGroup(@NonNull GroupDataOrFailureOutcome outcome) {
        @GroupSharedState int sharedState = TabShareUtils.discernSharedGroupState(outcome);
        setSharedProperties(sharedState, outcome.groupData);
    }

    private void setSharedProperties(
            @GroupSharedState int sharedState, @Nullable GroupData groupData) {
        if (sharedState == GroupSharedState.NOT_SHARED) {
            mPropertyModel.set(DELETE_RUNNABLE, this::processDeleteGroup);
            mPropertyModel.set(LEAVE_RUNNABLE, null);
            mPropertyModel.set(TabGroupRowProperties.DISPLAY_AS_SHARED, false);
            mPropertyModel.set(TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW, null);
            return;
        }

        String gaiaId = mCoreAccountInfoSupplier.get().getGaiaId();
        @MemberRole int memberRole = TabShareUtils.getSelfMemberRole(groupData, gaiaId);
        if (memberRole == MemberRole.OWNER) {
            mPropertyModel.set(
                    DELETE_RUNNABLE,
                    () ->
                            processDeleteSharedGroup(
                                    groupData.displayName, groupData.groupToken.groupId));
            mPropertyModel.set(LEAVE_RUNNABLE, null);
        } else {
            // TODO(crbug.com/365852281): Leave action should look like a delete if there are no
            // other users.
            mPropertyModel.set(DELETE_RUNNABLE, null);
            mPropertyModel.set(
                    LEAVE_RUNNABLE,
                    () -> processLeaveGroup(groupData.displayName, groupData.groupToken.groupId));
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
                                SharedImageTilesColor.DYNAMIC,
                                mDataSharingService);
            }
            mSharedImageTilesCoordinator.updateCollaborationId(mSavedTabGroup.collaborationId);
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
            assert savedTabGroup.localId != null;
        }

        int rootId = mTabGroupModelFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;
        mPaneManager.focusPane(PaneId.TAB_SWITCHER);
        TabSwitcherPaneBase tabSwitcherPaneBase =
                (TabSwitcherPaneBase) mPaneManager.getPaneForId(PaneId.TAB_SWITCHER);
        boolean success = tabSwitcherPaneBase.requestOpenTabGroupDialog(rootId);
        assert success;
    }

    private void processDeleteGroup() {
        mActionConfirmationManager.processDeleteGroupAttempt(
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        deleteGroup();
                    }
                });
    }

    private void processDeleteSharedGroup(String groupTitle, String groupId) {
        // TODO(crbug.com/365852281): Confirmation should look like a non-shared delete if there are
        // no other users.
        mActionConfirmationManager.processDeleteSharedGroupAttempt(
                groupTitle,
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        mDataSharingService.deleteGroup(groupId, this::onLeaveOrDeleteGroup);
                    }
                });
    }

    private void processLeaveGroup(String groupTitle, String groupId) {
        // TODO(crbug.com/365852281): Confirmation should look like a non-shared delete if there are
        // no other users.
        mActionConfirmationManager.processLeaveGroupAttempt(
                groupTitle,
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        String memberEmail = mCoreAccountInfoSupplier.get().getEmail();
                        mDataSharingService.removeMember(
                                groupId, memberEmail, this::onLeaveOrDeleteGroup);
                    }
                });
    }

    private void onLeaveOrDeleteGroup(@PeopleGroupActionOutcome int outcome) {
        if (outcome == PeopleGroupActionOutcome.SUCCESS) {
            // TODO(crbug.com/345854578): Do we need to actively remove things from the UI?
        } else {
            ModalDialogUtils.showOneButtonConfirmation(
                    mModalDialogManager,
                    mContext.getResources(),
                    R.string.data_sharing_generic_failure_title,
                    R.string.data_sharing_generic_failure_description,
                    R.string.data_sharing_invitation_failure_button);
        }
    }

    private void deleteGroup() {
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
                    mTabGroupModelFilter.getRootIdFromStableId(mSavedTabGroup.localId.tabGroupId);
            List<Tab> tabsToClose = mTabGroupModelFilter.getRelatedTabListForRootId(rootId);
            mTabGroupModelFilter.closeTabs(
                    TabClosureParams.closeTabs(tabsToClose).allowUndo(false).build());
        } else {
            mTabGroupSyncService.removeGroup(mSavedTabGroup.syncId);
        }
    }
}
