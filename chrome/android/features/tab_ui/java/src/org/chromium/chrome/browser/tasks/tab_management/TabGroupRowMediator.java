// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.core.util.Supplier;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowView.TabGroupRowViewTitleData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceLeaveOrDeleteEntryPoint;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** Contains the logic to set the state of the model and react to actions. */
@NullMarked
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
    private final ActionConfirmationManager mActionConfirmationManager;
    private final Supplier<@GroupWindowState Integer> mFetchGroupState;
    private final PropertyModel mPropertyModel;
    private final DataSharingTabManager mDataSharingTabManager;

    private @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;

    /**
     * @param context Used to load resources and create views.
     * @param tabGroupModelFilter Used to read current tab groups.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param dataSharingService Used to fetch shared group data.
     * @param collaborationService Used to fetch collaboration group data.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param fetchGroupState Used to fetch which window the group is in.
     * @param enableContainment Whether the tab group row is in a container.
     * @param dataSharingTabManager The {@link} DataSharingTabManager to start collaboration flows.
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
            ActionConfirmationManager actionConfirmationManager,
            FaviconResolver faviconResolver,
            Supplier<@GroupWindowState Integer> fetchGroupState,
            boolean enableContainment,
            DataSharingTabManager dataSharingTabManager) {
        mContext = context;
        mSavedTabGroup = savedTabGroup;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mPaneManager = paneManager;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mActionConfirmationManager = actionConfirmationManager;
        mFetchGroupState = fetchGroupState;
        mDataSharingTabManager = dataSharingTabManager;

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        int numberOfTabs = savedTabGroup.savedTabs.size();

        List<GURL> urlList = TabGroupFaviconCluster.buildUrlListFromSyncGroup(savedTabGroup);
        ClusterData clusterData = new ClusterData(faviconResolver, numberOfTabs, urlList);
        builder.with(TabGroupRowProperties.CLUSTER_DATA, clusterData);
        builder.with(TabGroupRowProperties.COLOR_INDEX, savedTabGroup.color);

        String userTitle = savedTabGroup.title;
        TabGroupRowViewTitleData titleData =
                new TabGroupRowViewTitleData(
                        userTitle, numberOfTabs, R.plurals.tab_group_row_accessibility_text);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);
        builder.with(
                TabGroupRowProperties.TIMESTAMP_EVENT,
                getTabGroupTimeAgoTimestampEvent(savedTabGroup));
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
        setSharedProperties(sharedState, groupData, enableContainment, savedTabGroup);
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
            @GroupSharedState int sharedState,
            @Nullable GroupData groupData,
            boolean enableContainment,
            SavedTabGroup savedTabGroup) {
        if (sharedState == GroupSharedState.NOT_SHARED) {
            mPropertyModel.set(DELETE_RUNNABLE, this::processDeleteGroup);
            mPropertyModel.set(LEAVE_RUNNABLE, null);
            mPropertyModel.set(TabGroupRowProperties.DISPLAY_AS_SHARED, false);
            mPropertyModel.set(TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW, null);
            return;
        }

        assumeNonNull(groupData);
        String collaborationId = groupData.groupToken.collaborationId;
        @MemberRole
        int memberRole = mCollaborationService.getCurrentUserRoleForGroup(collaborationId);
        if (memberRole == MemberRole.OWNER) {
            mPropertyModel.set(
                    DELETE_RUNNABLE, () -> processLeaveOrDeleteShareGroup(savedTabGroup));
            mPropertyModel.set(LEAVE_RUNNABLE, null);
        } else {
            // TODO(crbug.com/365852281): Leave action should look like a delete if there are no
            // other users.
            mPropertyModel.set(DELETE_RUNNABLE, null);
            mPropertyModel.set(LEAVE_RUNNABLE, () -> processLeaveOrDeleteShareGroup(savedTabGroup));
        }

        if (sharedState == GroupSharedState.HAS_OTHER_USERS
                || sharedState == GroupSharedState.COLLABORATION_ONLY) {
            mPropertyModel.set(TabGroupRowProperties.DISPLAY_AS_SHARED, true);
            if (mSharedImageTilesCoordinator == null) {
                final @ColorInt int backgroundColor;
                if (enableContainment) {
                    backgroundColor = SemanticColorUtils.getColorSurfaceBright(mContext);
                } else {
                    backgroundColor =
                            TabUiThemeProvider.getTabGridDialogBackgroundColor(
                                    mContext, /* isIncognito= */ false);
                }
                SharedImageTilesConfig config =
                        new SharedImageTilesConfig.Builder(mContext)
                                .setBackgroundColor(backgroundColor)
                                .setBorderColor(backgroundColor)
                                .setTextColor(SemanticColorUtils.getDefaultTextColor(mContext))
                                .build();
                mSharedImageTilesCoordinator =
                        new SharedImageTilesCoordinator(
                                mContext, config, mDataSharingService, mCollaborationService);
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
            assumeNonNull(syncId);
            boolean isTabGroupArchived = savedTabGroup.archivalTimeMs != null;
            mTabGroupUiActionHandler.openTabGroup(syncId);
            if (isTabGroupArchived) {
                RecordUserAction.record("TabGroups.RestoreFromTabGroupPane");
                RecordHistogram.recordCount1000Histogram(
                        "TabGroups.RestoreFromTabGroupPane.TabCount",
                        savedTabGroup.savedTabs.size());
            }
            savedTabGroup = mTabGroupSyncService.getGroup(syncId);
        }

        assumeNonNull(savedTabGroup);
        if (savedTabGroup.localId == null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TabGroupSync.WindowStateOnFailedOpen", state, GroupWindowState.COUNT);
            return;
        }

        int tabId = mTabGroupModelFilter.getGroupLastShownTabId(savedTabGroup.localId.tabGroupId);
        assert tabId != Tab.INVALID_TAB_ID;
        mPaneManager.focusPane(PaneId.TAB_SWITCHER);
        TabSwitcherPaneBase tabSwitcherPaneBase =
                (TabSwitcherPaneBase) mPaneManager.getPaneForId(PaneId.TAB_SWITCHER);
        assumeNonNull(tabSwitcherPaneBase);
        boolean success = tabSwitcherPaneBase.requestOpenTabGroupDialog(tabId);
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

    private void processLeaveOrDeleteShareGroup(SavedTabGroup savedTabGroup) {
        EitherGroupId eitherId;
        if (savedTabGroup.syncId != null) {
            eitherId = EitherGroupId.createSyncId(savedTabGroup.syncId);
        } else {
            assumeNonNull(savedTabGroup.localId);
            eitherId = EitherGroupId.createLocalId(savedTabGroup.localId);
        }

        mDataSharingTabManager.leaveOrDeleteFlow(
                eitherId, CollaborationServiceLeaveOrDeleteEntryPoint.ANDROID_TAB_GROUP_ROW);
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
            mTabGroupSyncService.removeGroup(assumeNonNull(mSavedTabGroup.syncId));
        } else if (state == GroupWindowState.IN_CURRENT) {
            assumeNonNull(mSavedTabGroup.localId);
            mTabGroupModelFilter
                    .getTabModel()
                    .getTabRemover()
                    .closeTabs(
                            assumeNonNull(
                                            TabClosureParams.forCloseTabGroup(
                                                    mTabGroupModelFilter,
                                                    mSavedTabGroup.localId.tabGroupId))
                                    .allowUndo(false)
                                    .build(),
                            allowDialog);
        } else {
            assert !allowDialog : "A dialog should have already been shown.";
            mTabGroupSyncService.removeGroup(assumeNonNull(mSavedTabGroup.syncId));
        }
    }

    /** Determine the last used timestamp from {@link SavedTabGroupTab} update times. */
    private TabGroupTimeAgo getTabGroupTimeAgoTimestampEvent(SavedTabGroup savedTabGroup) {
        if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()) {
            return new TabGroupTimeAgo(
                    TabUiUtils.getGroupLastUpdatedTimestamp(savedTabGroup), TimestampEvent.UPDATED);
        } else {
            return new TabGroupTimeAgo(savedTabGroup.creationTimeMs, TimestampEvent.CREATED);
        }
    }
}
