// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityActionHandler;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseUrlStatus;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingPreviewDataConfig;
import org.chromium.components.data_sharing.configs.DataSharingPreviewDetailsConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.components.data_sharing.configs.DataSharingStringConfig;
import org.chromium.components.data_sharing.configs.DataSharingUiConfig;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/**
 * This class is responsible for handling communication from the UI to multiple data sharing
 * services. This class is created once per {@link ChromeTabbedActivity}.
 */
public class DataSharingTabManager {
    private static final String TAG = "DataSharing";

    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Resources mResources;
    private final OneshotSupplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final Map</*collaborationId*/ String, SyncObserver> mSyncObserversList =
            new HashMap<>();
    private final LinkedList<Runnable> mTasksToRunOnProfileAvailable = new LinkedList<>();
    private final BulkFaviconUtil mBulkFaviconUtil = new BulkFaviconUtil();

    private @Nullable Profile mProfile;
    private @Nullable DataSharingService mDataSharingService;
    private @Nullable MessagingBackendService mMessagingBackendService;

    /** This class is responsible for observing sync tab activities. */
    private static class SyncObserver implements TabGroupSyncService.Observer {
        private final String mCollaborationId;
        private final TabGroupSyncService mTabGroupSyncService;
        private Callback<SavedTabGroup> mCallback;

        SyncObserver(
                String collaborationId,
                TabGroupSyncService tabGroupSyncService,
                Callback<SavedTabGroup> callback) {
            mCollaborationId = collaborationId;
            mTabGroupSyncService = tabGroupSyncService;
            mCallback = callback;

            mTabGroupSyncService.addObserver(this);
        }

        @Override
        public void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {
            if (mCollaborationId.equals(group.collaborationId)) {
                Callback<SavedTabGroup> callback = mCallback;
                destroy();
                callback.onResult(group);
            }
        }

        void destroy() {
            mTabGroupSyncService.removeObserver(this);
            mCallback = null;
        }
    }

    /**
     * Constructor for a new {@link DataSharingTabManager} object.
     *
     * @param tabSwitcherDelegate The delegate used to communicate with the tab switcher.
     * @param bottomSheetControllerSupplier The supplier of bottom sheet state controller.
     * @param shareDelegateSupplier The supplier of share delegate.
     * @param windowAndroid The window base class that has the minimum functionality.
     * @param resources Used to load localized android resources.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open tab groups
     *     locally.
     */
    public DataSharingTabManager(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            DataSharingTabSwitcherDelegate tabSwitcherDelegate,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Resources resources,
            OneshotSupplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mDataSharingTabSwitcherDelegate = tabSwitcherDelegate;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mResources = resources;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        assert mBottomSheetControllerSupplier != null;
        assert mShareDelegateSupplier != null;
    }

    /**
     * Initializes when profile is available.
     *
     * @param profile The loaded profile.
     * @param dataSharingService Data sharing service associated with the profile.
     * @param messagingBackendService The messaging backend used to show recent activity UI.
     */
    public void initWithProfile(
            @NonNull Profile profile,
            DataSharingService dataSharingService,
            MessagingBackendService messagingBackendService) {
        mProfile = profile;
        assert !mProfile.isOffTheRecord();
        mDataSharingService = dataSharingService;
        mMessagingBackendService = messagingBackendService;
        while (!mTasksToRunOnProfileAvailable.isEmpty()) {
            Runnable task = mTasksToRunOnProfileAvailable.removeFirst();
            task.run();
        }
    }

    /** Cleans up any outstanding resources. */
    public void destroy() {
        for (Map.Entry<String, SyncObserver> entry : mSyncObserversList.entrySet()) {
            entry.getValue().destroy();
        }
        mSyncObserversList.clear();
        mBulkFaviconUtil.destroy();
    }

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param dataSharingUrl The URL associated with the join invitation.
     */
    public void initiateJoinFlow(Activity activity, GURL dataSharingUrl) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.JOIN_TRIGGERED);
        if (mProfile != null) {
            initiateJoinFlowWithProfile(activity, dataSharingUrl);
            return;
        }

        mTasksToRunOnProfileAvailable.addLast(
                () -> {
                    initiateJoinFlowWithProfile(activity, dataSharingUrl);
                });
    }

    /**
     * Tracker to handle join flow interraction with UI delegate.
     *
     * <p>Tracks the finished callback from UI delegate and close the loading screen when the tab
     * group from sync is loaded. Handles edge case when sync fetches tab group before the join
     * callback is available.
     */
    private static class JoinFlowTracker {
        private Callback<Boolean> mFinishJoinLoading;
        private boolean mJoinedTabGroupOpened;
        private String mSessionId;
        private DataSharingUIDelegate mUiDelegate;

        JoinFlowTracker(DataSharingUIDelegate uiDelegate) {
            this.mUiDelegate = uiDelegate;
        }

        /** Set the session ID for join flow, used to destroy the flow. */
        void setSessionId(String id) {
            mSessionId = id;
        }

        /** Returns the session ID of the flow */
        String getSessionId() {
            return mSessionId;
        }

        /** Called to clean up the Join flow when tab group is fetched. */
        void onTabGroupOpened() {
            mJoinedTabGroupOpened = true;
            // Finish loading UI, when tab group is loaded.
            finishFlowIfNeeded();
        }

        /**
         * Called when people group joined with callback to end loading when tab group is fetched.
         */
        void onGroupJoined(Callback<Boolean> joinFinishedCallback) {
            // Store the callback and run it when the tab group is opened.
            mFinishJoinLoading = joinFinishedCallback;

            // If the group was already opened when people group join callback comes, then run
            // the loading finished call immediately.
            finishFlowIfNeeded();
        }

        private void finishFlowIfNeeded() {
            // Finish the flow only when both group is joined and tab group is opened.
            if (mFinishJoinLoading != null && mJoinedTabGroupOpened) {
                mFinishJoinLoading.onResult(true);
                mFinishJoinLoading = null;
                mUiDelegate.destroyFlow(mSessionId);
            }
        }
    }

    private GURL getTabGroupHelpUrl() {
        // TODO(ssid): Fix the right help URL link here.
        return new GURL("https://www.example.com/");
    }

    private void initiateJoinFlowWithProfile(Activity activity, GURL dataSharingUrl) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.PROFILE_AVAILABLE);
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        assert tabGroupSyncService != null;
        assert mDataSharingService != null;

        DataSharingService.ParseUrlResult parseResult =
                mDataSharingService.parseDataSharingUrl(dataSharingUrl);
        if (parseResult.status != ParseUrlStatus.SUCCESS) {
            showInvitationFailureDialog();
            DataSharingMetrics.recordJoinActionFlowState(
                    DataSharingMetrics.JoinActionStateAndroid.PARSE_URL_FAILED);
            return;
        }

        GroupToken groupToken = parseResult.groupToken;
        String collaborationId = groupToken.collaborationId;
        DataSharingUIDelegate uiDelegate = mDataSharingService.getUiDelegate();
        assert uiDelegate != null;
        JoinFlowTracker joinFlowTracker = new JoinFlowTracker(uiDelegate);

        // Verify that tab group does not already exist in sync.
        SavedTabGroup existingGroup =
                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                        collaborationId, tabGroupSyncService);
        if (existingGroup != null) {
            DataSharingMetrics.recordJoinActionFlowState(
                    DataSharingMetrics.JoinActionStateAndroid.SYNCED_TAB_GROUP_EXISTS);
            onSavedTabGroupAvailable(existingGroup);
            return;
        }

        long startTime = SystemClock.uptimeMillis();
        if (!mSyncObserversList.containsKey(collaborationId)) {
            SyncObserver syncObserver =
                    new SyncObserver(
                            collaborationId,
                            tabGroupSyncService,
                            (group) -> {
                                DataSharingMetrics.recordJoinFlowLatency(
                                        "SyncRequest", SystemClock.uptimeMillis() - startTime);
                                onSavedTabGroupAvailable(group);
                                mSyncObserversList.remove(group.collaborationId);
                                joinFlowTracker.onTabGroupOpened();
                            });

            mSyncObserversList.put(collaborationId, syncObserver);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID_V2)) {
            mDataSharingService.getSharedEntitiesPreview(
                    groupToken,
                    (previewData) -> {
                        showJoinUiInternal(activity, joinFlowTracker, groupToken, previewData);
                    });

            return;
        }

        mDataSharingService.addMember(
                groupToken.collaborationId,
                groupToken.accessToken,
                result -> {
                    if (result != PeopleGroupActionOutcome.SUCCESS) {
                        DataSharingMetrics.recordJoinActionFlowState(
                                DataSharingMetrics.JoinActionStateAndroid.ADD_MEMBER_FAILED);
                        showInvitationFailureDialog();
                        return;
                    }
                    DataSharingMetrics.recordJoinActionFlowState(
                            DataSharingMetrics.JoinActionStateAndroid.ADD_MEMBER_SUCCESS);
                });
    }

    private void showJoinUiInternal(
            Activity activity,
            JoinFlowTracker joinFlowTracker,
            GroupToken groupToken,
            DataSharingService.SharedDataPreviewOrFailureOutcome previewData) {
        if (previewData.sharedDataPreview == null
                || previewData.sharedDataPreview.sharedTabGroupPreview == null) {
            showInvitationFailureDialog();
            return;
        }
        SharedTabGroupPreview preview = previewData.sharedDataPreview.sharedTabGroupPreview;
        DataSharingStringConfig stringConfig =
                new DataSharingStringConfig.Builder()
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_TITLE,
                                R.plurals.collaboration_preview_dialog_title_multiple)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_TITLE_SINGLE,
                                R.string.collaboration_preview_dialog_title_single)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_DESCRIPTION,
                                R.string.collaboration_preview_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_DETAILS_TITLE,
                                R.string.collaboration_preview_dialog_details_title)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_DETAILS_HEADER,
                                R.string.collaboration_preview_dialog_details_tabs_in_group)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.TABS_COUNT_TITLE,
                                R.plurals.collaboration_preview_dialog_tabs)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_SHARED_TAB_GROUPS,
                                R.string.collaboration_learn_about_shared_groups)
                        .build();
        DataSharingUiConfig commonConfig =
                new DataSharingUiConfig.Builder()
                        .setActivity(activity)
                        .setTabGroupName(preview.title)
                        .setIsTablet(false)
                        .setLearnMoreHyperLink(getTabGroupHelpUrl())
                        .setDataSharingStringConfig(stringConfig)
                        .build();
        DataSharingJoinUiConfig.JoinCallback joinCallback =
                new DataSharingJoinUiConfig.JoinCallback() {
                    @Override
                    public void onGroupJoinedWithWait(
                            org.chromium.components.sync.protocol.GroupData groupData,
                            Callback<Boolean> onJoinFinished) {
                        joinFlowTracker.onGroupJoined(onJoinFinished);
                        DataSharingMetrics.recordJoinActionFlowState(
                                DataSharingMetrics.JoinActionStateAndroid.ADD_MEMBER_SUCCESS);
                        assert groupData.getGroupId().equals(groupToken.collaborationId);
                    }
                };
        joinFlowTracker.setSessionId(
                mDataSharingService
                        .getUiDelegate()
                        .showJoinFlow(
                                new DataSharingJoinUiConfig.Builder()
                                        .setCommonConfig(commonConfig)
                                        .setJoinCallback(joinCallback)
                                        .setGroupToken(groupToken)
                                        .setSharedDataPreview(previewData.sharedDataPreview)
                                        .build()));

        fetchFavicons(
                activity,
                joinFlowTracker,
                preview,
                /* fetchAll= */ false,
                () -> {
                    fetchFavicons(
                            activity, joinFlowTracker, preview, /* fetchAll= */ true, () -> {});
                });
    }

    private void fetchFavicons(
            Activity activity,
            JoinFlowTracker joinFlowTracker,
            SharedTabGroupPreview preview,
            boolean fetchAll,
            Runnable doneCallback) {
        int maxNumToFetch = fetchAll ? preview.tabs.size() : 4;
        int numToFetch = Math.min(maxNumToFetch, preview.tabs.size());
        List<GURL> urls = new ArrayList<>();
        for (int i = 0; i < numToFetch; ++i) {
            urls.add(preview.tabs.get(i).url);
        }
        mBulkFaviconUtil.fetchAsBitmap(
                activity,
                mProfile,
                urls,
                // TODO(haileywang): add this to resources when using it in service.
                /* size= */ 72,
                (favicons) -> {
                    if (fetchAll) {
                        updateAllFavicons(joinFlowTracker, preview, favicons);
                    } else {
                        updatePreviewImage(joinFlowTracker, favicons);
                    }
                    doneCallback.run();
                });
    }

    private void updateAllFavicons(
            JoinFlowTracker joinFlowTracker, SharedTabGroupPreview preview, List<Bitmap> favicons) {
        List<DataSharingPreviewDetailsConfig.TabPreview> tabPreviews = new ArrayList<>();
        for (int i = 0; i < favicons.size(); ++i) {
            tabPreviews.add(
                    new DataSharingPreviewDetailsConfig.TabPreview(
                            preview.tabs.get(i).displayUrl, favicons.get(i)));
        }
        DataSharingRuntimeDataConfig runtimeConfig =
                new DataSharingRuntimeDataConfig.Builder()
                        .setSessionId(joinFlowTracker.getSessionId())
                        .setDataSharingPreviewDetailsConfig(
                                new DataSharingPreviewDetailsConfig.Builder()
                                        .setTabPreviews(tabPreviews)
                                        .build())
                        .build();
        mDataSharingService
                .getUiDelegate()
                .updateRuntimeData(joinFlowTracker.getSessionId(), runtimeConfig);
    }

    private void updatePreviewImage(JoinFlowTracker joinFlowTracker, List<Bitmap> favicons) {
        // TODO(ssid): Make bitmap of the grid view.
        Bitmap previewImage = favicons.get(0);
        DataSharingRuntimeDataConfig runtimeConfig =
                new DataSharingRuntimeDataConfig.Builder()
                        .setSessionId(joinFlowTracker.getSessionId())
                        .setDataSharingPreviewDataConfig(
                                new DataSharingPreviewDataConfig.Builder()
                                        .setTabGroupPreviewImage(previewImage)
                                        .build())
                        .build();
        mDataSharingService
                .getUiDelegate()
                .updateRuntimeData(joinFlowTracker.getSessionId(), runtimeConfig);
    }

    private void showInvitationFailureDialog() {
        @Nullable ModalDialogManager modalDialogManager = mWindowAndroid.getModalDialogManager();
        if (modalDialogManager == null) return;

        ModalDialogUtils.showOneButtonConfirmation(
                modalDialogManager,
                mResources,
                R.string.data_sharing_invitation_failure_title,
                R.string.data_sharing_invitation_failure_description,
                R.string.data_sharing_invitation_failure_button);
    }

    /**
     * Switch the view to a currently opened tab group.
     *
     * @param tabId The tab id of the first tab in the group.
     */
    void switchToTabGroup(SavedTabGroup group) {
        Integer tabId = group.savedTabs.get(0).localId;
        assert tabId != null;
        // TODO(b/354003616): Verify that the loading dialog is gone.
        mDataSharingTabSwitcherDelegate.openTabGroupWithTabId(tabId);
    }

    /**
     * Called when a saved tab group is available.
     *
     * @param group The SavedTabGroup that became available.
     */
    void onSavedTabGroupAvailable(SavedTabGroup group) {
        // Check if tab is already opened in local tab group model.
        boolean isInLocalTabGroup = (group.localId != null);

        if (isInLocalTabGroup) {
            DataSharingMetrics.recordJoinActionFlowState(
                    DataSharingMetrics.JoinActionStateAndroid.LOCAL_TAB_GROUP_EXISTS);
            switchToTabGroup(group);
            return;
        }

        openLocalTabGroup(group);
    }

    void openLocalTabGroup(SavedTabGroup group) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);

        mTabGroupUiActionHandlerSupplier.runSyncOrOnAvailable(
                (tabGroupUiActionHandler) -> {
                    // Note: This does not switch the active tab to the opened tab.
                    tabGroupUiActionHandler.openTabGroup(group.syncId);
                    DataSharingMetrics.recordJoinActionFlowState(
                            DataSharingMetrics.JoinActionStateAndroid.LOCAL_TAB_GROUP_ADDED);
                    SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(group.syncId);
                    switchToTabGroup(savedTabGroup);
                    DataSharingMetrics.recordJoinActionFlowState(
                            DataSharingMetrics.JoinActionStateAndroid.LOCAL_TAB_GROUP_OPENED);
                });
    }

    /**
     * Stop observing a data sharing tab group from sync.
     *
     * @param observer The observer to be removed.
     */
    protected void deleteSyncObserver(SyncObserver observer) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);

        if (tabGroupSyncService != null) {
            tabGroupSyncService.removeObserver(observer);
        }
    }

    /**
     * Creates a collaboration group.
     *
     * @param activity The activity in which the group is to be created.
     * @param tabGroupDisplayName The title or display name of the tab group.
     * @param localTabGroupId The tab group ID of the tab in the local tab group model.
     * @param createGroupFinishedCallback Callback invoked when the creation flow is finished.
     */
    public void createGroupFlow(
            Activity activity,
            String tabGroupDisplayName,
            LocalTabGroupId localTabGroupId,
            Callback<Boolean> createGroupFinishedCallback) {
        DataSharingMetrics.recordShareActionFlowState(
                DataSharingMetrics.ShareActionStateAndroid.SHARE_TRIGGERED);
        assert mProfile != null;
        TabGroupSyncService tabGroupService = TabGroupSyncServiceFactory.getForProfile(mProfile);

        SavedTabGroup existingGroup = tabGroupService.getGroup(localTabGroupId);
        assert existingGroup != null : "Group not found in TabGroupSyncService.";
        if (existingGroup.collaborationId != null) {
            onShareClickExistingGroup(activity, mDataSharingService, existingGroup);
            return;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID_V2)) {
            DataSharingUIDelegate uiDelegate = mDataSharingService.getUiDelegate();
            DataSharingStringConfig stringConfig =
                    new DataSharingStringConfig.Builder()
                            .setResourceId(
                                    DataSharingStringConfig.StringKey.CREATE_TITLE,
                                    R.string.collaboration_share_group_title)
                            .setResourceId(
                                    DataSharingStringConfig.StringKey.CREATE_DESCRIPTION,
                                    R.string.collaboration_share_group_body)
                            .setResourceId(
                                    DataSharingStringConfig.StringKey.LEARN_ABOUT_SHARED_TAB_GROUPS,
                                    R.string.collaboration_learn_about_shared_groups)
                            .build();
            DataSharingUiConfig commonConfig =
                    new DataSharingUiConfig.Builder()
                            .setActivity(activity)
                            .setTabGroupName(tabGroupDisplayName)
                            .setLearnMoreHyperLink(getTabGroupHelpUrl())
                            .setIsTablet(false)
                            .setDataSharingStringConfig(stringConfig)
                            .build();

            DataSharingCreateUiConfig.CreateCallback createCallback =
                    new DataSharingCreateUiConfig.CreateCallback() {
                        @Override
                        public void onGroupCreated(
                                org.chromium.components.sync.protocol.GroupData result) {
                            tabGroupService.makeTabGroupShared(
                                    localTabGroupId, result.getGroupId());
                            createGroupFinishedCallback.onResult(true);
                            DataSharingMetrics.recordShareActionFlowState(
                                    DataSharingMetrics.ShareActionStateAndroid
                                            .GROUP_CREATE_SUCCESS);
                            showShareSheet(
                                    new GroupData(
                                            result.getGroupId(),
                                            result.getDisplayName(),
                                            /* members= */ null,
                                            result.getAccessToken()));
                        }

                        @Override
                        public void onCancelClicked() {
                            DataSharingMetrics.recordShareActionFlowState(
                                    DataSharingMetrics.ShareActionStateAndroid
                                            .BOTTOM_SHEET_DISMISSED);
                        }

                        @Override
                        public void getDataSharingUrl(
                                GroupToken tokenSecret, Callback<String> url) {}
                    };
            uiDelegate.showCreateFlow(
                    new DataSharingCreateUiConfig.Builder()
                            .setCommonConfig(commonConfig)
                            .setCreateCallback(createCallback)
                            .build());

            return;
        }

        Callback<DataSharingService.GroupDataOrFailureOutcome> createGroupCallback =
                (result) -> {
                    if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN
                            || result.groupData == null) {
                        Log.e(TAG, "Group creation failed " + result.actionFailure);
                        createGroupFinishedCallback.onResult(false);
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.GROUP_CREATE_FAILED);
                    } else {
                        tabGroupService.makeTabGroupShared(
                                localTabGroupId, result.groupData.groupToken.collaborationId);
                        createGroupFinishedCallback.onResult(true);

                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.GROUP_CREATE_SUCCESS);
                        showShareSheet(result.groupData);
                    }
                };

        mDataSharingService.createGroup(tabGroupDisplayName, createGroupCallback);
    }

    private void onShareClickExistingGroup(
            Activity activity,
            DataSharingService mDataSharingService,
            SavedTabGroup existingGroup) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID_V2)) {
            assert existingGroup.collaborationId != null;
            showManageSharing(activity, existingGroup.collaborationId);
            return;
        }
        mDataSharingService.ensureGroupVisibility(
                existingGroup.collaborationId,
                (result) -> {
                    if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN
                            || result.groupData == null) {
                        // TODO(ritikagup): Show error dialog telling failed to create access
                        // token.
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid
                                        .ENSURE_VISIBILITY_FAILED);
                    } else {
                        showShareSheet(result.groupData);
                    }
                });
    }

    private void showShareSheet(GroupData groupData) {
        GURL url = mDataSharingService.getDataSharingUrl(groupData);
        if (url == null) {
            // TODO(ritikagup) : Show error dialog showing fetching URL failed. Contact owner for
            // new link.
            DataSharingMetrics.recordShareActionFlowState(
                    DataSharingMetrics.ShareActionStateAndroid.URL_CREATION_FAILED);
            return;
        }
        DataSharingMetrics.recordShareActionFlowState(
                DataSharingMetrics.ShareActionStateAndroid.SHARE_SHEET_SHOWN);
        var chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.PAGE_INFO)
                        .build();
        // TODO (b/358666351) : Add correct text for Share URL based on UX.
        ShareParams shareParams =
                new ShareParams.Builder(mWindowAndroid, groupData.displayName, url.getSpec())
                        .setText("")
                        .build();

        mShareDelegateSupplier
                .get()
                .share(shareParams, chromeShareExtras, ShareDelegate.ShareOrigin.TAB_GROUP);
    }

    /**
     * Shows UI for manage sharing.
     *
     * @param activity The activity to show the UI for.
     * @param collaborationId The collaboration ID to show the UI for.
     */
    public void showManageSharing(Activity activity, String collaborationId) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID_V2)) {
            return;
        }

        assert mProfile != null;

        DataSharingUIDelegate uiDelegate = mDataSharingService.getUiDelegate();

        DataSharingStringConfig stringConfig =
                new DataSharingStringConfig.Builder()
                        .setResourceId(
                                DataSharingStringConfig.StringKey.MANAGE_DESCRIPTION,
                                R.string.collaboration_manage_group_description)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LET_ANYONE_JOIN_DESCRIPTION,
                                R.string.collaboration_manage_share_wisely)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_SHARED_TAB_GROUPS,
                                R.string.collaboration_learn_about_shared_groups)
                        .build();
        DataSharingUiConfig commonConfig =
                new DataSharingUiConfig.Builder()
                        .setActivity(activity)
                        .setIsTablet(false)
                        .setLearnMoreHyperLink(getTabGroupHelpUrl())
                        .setDataSharingStringConfig(stringConfig)
                        .build();

        DataSharingManageUiConfig.ManageCallback manageCallback =
                new DataSharingManageUiConfig.ManageCallback() {
                    @Override
                    public void onShareInviteLinkClicked(GroupToken groupToken) {
                        // TODO(ssid): Pass in the title and refactor showShareSheet to depend on
                        // GroupToken instead.
                        showShareSheet(
                                new GroupData(
                                        groupToken.collaborationId,
                                        "Tab Group",
                                        /* members= */ null,
                                        groupToken.accessToken));
                    }
                };
        DataSharingManageUiConfig manageConfig =
                new DataSharingManageUiConfig.Builder()
                        .setGroupToken(new GroupToken(collaborationId, null))
                        .setManageCallback(manageCallback)
                        .setCommonConfig(commonConfig)
                        .build();
        uiDelegate.showManageFlow(manageConfig);
    }

    /**
     * Shows UI for recent activity.
     *
     * @param activity The associated activity.
     * @param collaborationId The collaboration ID to show the UI for.
     */
    public void showRecentActivity(Activity activity, String collaborationId) {
        assert mProfile != null;
        assert mMessagingBackendService != null;

        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        SavedTabGroup existingGroup =
                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                        collaborationId, tabGroupSyncService);
        if (existingGroup == null) return;

        DataSharingAvatarProvider avatarProvider =
                new DataSharingAvatarProvider(activity, mDataSharingService.getUiDelegate());

        // TODO(crbug.com/380962101): Extract manage sharing into a different interface.
        Runnable manageSharingCallback = () -> showManageSharing(activity, collaborationId);
        RecentActivityActionHandler recentActivityActionHandler =
                new RecentActivityActionHandlerImpl(
                        tabGroupSyncService,
                        mTabModelSelectorSupplier.get(),
                        mDataSharingTabSwitcherDelegate,
                        collaborationId,
                        existingGroup.syncId,
                        manageSharingCallback);
        RecentActivityListCoordinator recentActivityListCoordinator =
                new RecentActivityListCoordinator(
                        activity,
                        mBottomSheetControllerSupplier.get(),
                        mMessagingBackendService,
                        new DataSharingFaviconProvider(activity, mProfile, mBulkFaviconUtil),
                        avatarProvider,
                        recentActivityActionHandler);
        recentActivityListCoordinator.requestShowUI(collaborationId);
    }

    protected BottomSheetContent showBottomSheet(
            Context context, Callback<Integer> onClosedCallback) {
        ViewGroup bottomSheetView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.data_sharing_bottom_sheet, null);
        TabGridDialogShareBottomSheetContent bottomSheetContent =
                new TabGridDialogShareBottomSheetContent(bottomSheetView);

        BottomSheetController controller = mBottomSheetControllerSupplier.get();
        controller.requestShowContent(bottomSheetContent, true);
        controller.addObserver(
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        controller.removeObserver(this);
                        Callback.runNullSafe(onClosedCallback, reason);
                    }
                });
        return bottomSheetContent;
    }

    BulkFaviconUtil getBulkFaviconUtilForTesting() {
        return mBulkFaviconUtil;
    }
}
