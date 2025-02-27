// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.collaboration.CollaborationControllerDelegateFactory;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityActionHandler;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseUrlStatus;
import org.chromium.components.data_sharing.SharedDataPreview;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.TabPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingPreviewDetailsConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.components.data_sharing.configs.DataSharingStringConfig;
import org.chromium.components.data_sharing.configs.DataSharingUiConfig;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.base.DeviceFormFactor;
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
    private static final String LEARN_MORE_SHARED_TAB_GROUP_PAGE_URL =
            "https://support.google.com/chrome/?p=chrome_collaboration";
    private static final String LEARN_ABOUT_BLOCKED_ACCOUNTS_URL =
            "https://support.google.com/accounts/answer/6388749";
    private static final String ACTIVITY_LOGS_URL =
            "https://myactivity.google.com/product/chrome_shared_tab_group_activity?utm_source=chrome_collab";

    // Separator for description and link in share sheet.
    private static final String SHARED_TEXT_SEPARATOR = "";

    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Resources mResources;
    private final OneshotSupplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final Map</*collaborationId*/ String, SyncObserver> mSyncObserversList =
            new HashMap<>();
    private final LinkedList<Runnable> mTasksToRunOnProfileAvailable = new LinkedList<>();
    private final BulkFaviconUtil mBulkFaviconUtil = new BulkFaviconUtil();
    private final CollaborationControllerDelegateFactory mCollaborationControllerDelegateFactory;

    private @Nullable Profile mProfile;
    private @Nullable DataSharingService mDataSharingService;
    private @Nullable MessagingBackendService mMessagingBackendService;
    private @Nullable CollaborationService mCollaborationService;
    private @Nullable CollaborationControllerDelegate mCurrentDelegate;

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
     * @param tabGroupsDelegate The delegate used to communicate with the tab switcher.
     * @param bottomSheetControllerSupplier The supplier of bottom sheet state controller.
     * @param shareDelegateSupplier The supplier of share delegate.
     * @param windowAndroid The window base class that has the minimum functionality.
     * @param resources Used to load localized android resources.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open tab groups
     *     locally.
     * @param collaborationControllerDelegateFactory The factory to create a {@link
     *     CollaborationControllerDelegate}
     */
    public DataSharingTabManager(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            DataSharingTabGroupsDelegate tabGroupsDelegate,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Resources resources,
            OneshotSupplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            CollaborationControllerDelegateFactory collaborationControllerDelegateFactory) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mDataSharingTabGroupsDelegate = tabGroupsDelegate;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mResources = resources;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mCollaborationControllerDelegateFactory = collaborationControllerDelegateFactory;
        assert mBottomSheetControllerSupplier != null;
        assert mShareDelegateSupplier != null;
    }

    /**
     * @return The {@link Profile} instance associated with the tab manager.
     */
    public Profile getProfile() {
        return mProfile;
    }

    /**
     * @return The {@link WindowAndroid} instance associated with the tab manager.
     */
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * @return The {@link DataSharingUiDelegate} instance associated with the tab manager.
     */
    public DataSharingUIDelegate getUiDelegate() {
        return mDataSharingService.getUiDelegate();
    }

    /**
     * Initializes when profile is available.
     *
     * @param profile The loaded profile.
     * @param dataSharingService Data sharing service associated with the profile.
     * @param messagingBackendService The messaging backend used to show recent activity UI.
     * @param collaborationService The collaboration service to manage collaboration flows.
     */
    public void initWithProfile(
            @NonNull Profile profile,
            DataSharingService dataSharingService,
            MessagingBackendService messagingBackendService,
            CollaborationService collaborationService) {
        mProfile = profile;
        assert !mProfile.isOffTheRecord();
        mDataSharingService = dataSharingService;
        mMessagingBackendService = messagingBackendService;
        mCollaborationService = collaborationService;
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

        if (mCurrentDelegate != null) {
            mCurrentDelegate.destroy();
            mCurrentDelegate = null;
        }
    }

    /** Cleans up the current collaboration delegate reference. */
    public void onCollaborationDelegateFlowFinished() {
        mCurrentDelegate = null;
    }

    /** Returns whether the current session supports creating collaborations. */
    public boolean isCreationEnabled() {
        // Collaboration service may still be null if the DATA_SHARING feature is disabled or
        // initWithProfile() has not been called. If this is the case do not allow creation yet.
        // See https://crbug.com/392053335.
        if (mCollaborationService == null) return false;

        return mCollaborationService.getServiceStatus().isAllowedToCreate();
    }

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param activity The current tabbed activity.
     * @param dataSharingUrl The URL associated with the join invitation.
     */
    public void initiateJoinFlow(Activity activity, GURL dataSharingUrl) {
        initiateJoinFlow(activity, dataSharingUrl, /* switchToTabSwitcherCallback= */ null);
    }

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param activity The current tabbed activity.
     * @param dataSharingUrl The URL associated with the join invitation.
     * @param switchToTabSwitcherCallback The callback to allow to switch to tab switcher view.
     */
    public void initiateJoinFlow(
            Activity activity,
            GURL dataSharingUrl,
            Callback<Runnable> switchToTabSwitcherCallback) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.JOIN_TRIGGERED);
        if (mProfile != null) {
            initiateJoinFlowWithProfile(activity, dataSharingUrl, switchToTabSwitcherCallback);
            return;
        }

        mTasksToRunOnProfileAvailable.addLast(
                () -> {
                    initiateJoinFlowWithProfile(
                            activity, dataSharingUrl, switchToTabSwitcherCallback);
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
        return new GURL(LEARN_MORE_SHARED_TAB_GROUP_PAGE_URL);
    }

    private GURL getLearnAboutBlockedAccountsUrl() {
        return new GURL(LEARN_ABOUT_BLOCKED_ACCOUNTS_URL);
    }

    private GURL getActivityLogsUrl() {
        return new GURL(ACTIVITY_LOGS_URL);
    }

    private void initiateJoinFlowWithProfile(
            Activity activity,
            GURL dataSharingUrl,
            Callback<Runnable> switchToTabSwitcherCallback) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.PROFILE_AVAILABLE);
        if (!mCollaborationService.getServiceStatus().isAllowedToJoin()) {
            showInvitationFailureDialog();
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.COLLABORATION_FLOW_ANDROID)
                && mCollaborationControllerDelegateFactory != null) {
            assert mCollaborationService != null;
            mCurrentDelegate =
                    mCollaborationControllerDelegateFactory.create(
                            FlowType.JOIN, switchToTabSwitcherCallback);
            mCollaborationService.startJoinFlow(mCurrentDelegate, dataSharingUrl);
            return;
        }

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

        mDataSharingService.getSharedEntitiesPreview(
                groupToken,
                (previewData) -> {
                    showJoinUiInternal(activity, joinFlowTracker, groupToken, previewData);
                });
    }

    private void showJoinUiInternal(
            Activity activity,
            JoinFlowTracker joinFlowTracker,
            GroupToken groupToken,
            DataSharingService.SharedDataPreviewOrFailureOutcome previewData) {
        if (previewData.sharedDataPreview == null
                || previewData.sharedDataPreview.sharedTabGroupPreview == null) {
            DataSharingMetrics.recordJoinActionFlowState(
                    DataSharingMetrics.JoinActionStateAndroid.PREVIEW_PERMISSION_DENIED);

            showInvitationFailureDialog();
            return;
        }
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.PREVIEW_FETCHED);
        SharedTabGroupPreview preview = previewData.sharedDataPreview.sharedTabGroupPreview;

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
                showJoinScreenWithPreview(activity, groupToken, preview, joinCallback));
    }

    /**
     * Show the join UI with preview data.
     *
     * @param activity The current tabbed activity.
     * @param token The {@link GroupToken} for the tab group.
     * @param previewTabGroupData The {@link SharedTabGroupPreview} for the tab group.
     * @param joinCallback The callbacks for the join ui.
     * @return The session id of the join screen.
     */
    public String showJoinScreenWithPreview(
            Activity activity,
            GroupToken token,
            SharedTabGroupPreview previewTabGroupData,
            DataSharingJoinUiConfig.JoinCallback joinCallback) {
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
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_GROUP_IS_FULL_ERROR_TITLE,
                                R.string.collaboration_group_is_full_error_dialog_header)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_GROUP_IS_FULL_ERROR_BODY,
                                R.string.collaboration_group_is_full_error_dialog_body)
                        .build();

        String tabGroupName = previewTabGroupData.title;
        if (TextUtils.isEmpty(tabGroupName)) {
            tabGroupName =
                    TabGroupTitleUtils.getDefaultTitle(activity, previewTabGroupData.tabs.size());
        }

        String sessionId =
                mDataSharingService
                        .getUiDelegate()
                        .showJoinFlow(
                                new DataSharingJoinUiConfig.Builder()
                                        .setCommonConfig(
                                                getCommonConfig(
                                                        activity, tabGroupName, stringConfig))
                                        .setJoinCallback(joinCallback)
                                        .setGroupToken(token)
                                        .setSharedDataPreview(
                                                new SharedDataPreview(previewTabGroupData))
                                        .build());

        fetchFavicons(
                activity, sessionId, previewTabGroupData.tabs, previewTabGroupData.tabs.size());
        return sessionId;
    }

    private void fetchFavicons(
            Activity activity, String sessionId, List<TabPreview> tabs, int maxFaviconsToFetch) {
        // First fetch favicons for up to 4 tabs, then fetch favicons for the remaining tabs.
        int previewImageSize = 4;
        Runnable fetchAll =
                () -> {
                    fetchFaviconsInternal(
                            activity,
                            sessionId,
                            tabs,
                            /* maxTabs= */ maxFaviconsToFetch,
                            () -> {
                                DataSharingMetrics.recordJoinActionFlowState(
                                        DataSharingMetrics.JoinActionStateAndroid
                                                .ALL_FAVICONS_FETCHED);
                            });
                };

        if (tabs.size() <= previewImageSize) {
            fetchAll.run();
            return;
        }
        Runnable onFetched =
                () -> {
                    DataSharingMetrics.recordJoinActionFlowState(
                            DataSharingMetrics.JoinActionStateAndroid.PREVIEW_FAVICONS_FETCHED);
                    fetchAll.run();
                };
        fetchFaviconsInternal(
                activity, sessionId, tabs, /* maxTabs= */ previewImageSize, onFetched);
    }

    private void fetchFaviconsInternal(
            Activity activity,
            String sessionId,
            List<TabPreview> tabs,
            int maxTabs,
            Runnable doneCallback) {
        List<GURL> urls = new ArrayList<>();
        List<String> displayUrls = new ArrayList<>();

        // Fetch URLs for favicons (up to maxTabs).
        for (int i = 0; i < Math.min(maxTabs, tabs.size()); i++) {
            urls.add(tabs.get(i).url);
        }

        // Always collect all display URLs.
        for (TabPreview tab : tabs) {
            displayUrls.add(tab.displayUrl);
        }
        mBulkFaviconUtil.fetchAsBitmap(
                activity,
                mProfile,
                urls,
                // TODO(haileywang): add this to resources when using it in service.
                /* size= */ 72,
                (favicons) -> {
                    updateFavicons(sessionId, displayUrls, favicons);
                    doneCallback.run();
                });
    }

    private void updateFavicons(String sessionId, List<String> displayUrls, List<Bitmap> favicons) {
        List<DataSharingPreviewDetailsConfig.TabPreview> tabsPreviewList = new ArrayList<>();
        for (int i = 0; i < displayUrls.size(); i++) {
            tabsPreviewList.add(
                    new DataSharingPreviewDetailsConfig.TabPreview(
                            displayUrls.get(i), i < favicons.size() ? favicons.get(i) : null));
        }
        DataSharingRuntimeDataConfig runtimeConfig =
                new DataSharingRuntimeDataConfig.Builder()
                        .setSessionId(sessionId)
                        .setDataSharingPreviewDetailsConfig(
                                new DataSharingPreviewDetailsConfig.Builder()
                                        .setTabPreviews(tabsPreviewList)
                                        .build())
                        .build();
        mDataSharingService.getUiDelegate().updateRuntimeData(sessionId, runtimeConfig);
    }

    private List<TabPreview> convertToTabsPreviewList(List<SavedTabGroupTab> savedTabs) {
        int tabsCount = savedTabs.size();
        List<TabPreview> preview = new ArrayList<>();
        for (int i = 0; i < tabsCount; ++i) {
            // displayUrl field is not used in the create or manage UI where local tab group is
            // available.
            preview.add(new TabPreview(savedTabs.get(i).url, /* displayUrl= */ ""));
        }
        return preview;
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
        TabGroupModelFilter filter =
                mTabModelSelectorSupplier
                        .get()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(false);
        int rootId = filter.getRootIdFromTabGroupId(group.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;
        mDataSharingTabGroupsDelegate.openTabGroupWithTabId(rootId);
    }

    /**
     * Open and focus on the tab group.
     *
     * @param collaborationId The collaboration id of the shared tab group.
     */
    public void promoteTabGroup(String collaborationId) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        SavedTabGroup existingGroup =
                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                        collaborationId, tabGroupSyncService);
        assert existingGroup != null;

        onSavedTabGroupAvailable(existingGroup);
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
     * Create a tab group with the tab and then start the create group flow.
     *
     * @param activity The current tabbed activity.
     * @param tab The tab to create group and share.
     * @param createGroupFinishedCallback Callback when the UI flow is finished with result.
     */
    public void createTabGroupAndShare(
            Activity activity, Tab tab, Callback<Boolean> createGroupFinishedCallback) {
        TabGroupModelFilter filter =
                mTabModelSelectorSupplier
                        .get()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(false);
        if (tab.getTabGroupId() == null) {
            filter.createSingleTabGroup(tab);
        }
        createOrManageFlow(
                activity,
                /* syncId= */ null,
                new LocalTabGroupId(tab.getTabGroupId()),
                createGroupFinishedCallback);
    }

    /**
     * Creates a collaboration group.
     *
     * @param activity The activity in which the group is to be created.
     * @param syncId The sync ID of the tab group.
     * @param localTabGroupId The tab group ID of the tab in the local tab group model.
     * @param createGroupFinishedCallback Callback invoked when the creation flow is finished.
     */
    public void createOrManageFlow(
            Activity activity,
            String syncId,
            LocalTabGroupId localTabGroupId,
            Callback<Boolean> createGroupFinishedCallback) {
        DataSharingMetrics.recordShareActionFlowState(
                DataSharingMetrics.ShareActionStateAndroid.SHARE_TRIGGERED);

        SavedTabGroup existingGroup = getSavedTabGroupForEitherId(syncId, localTabGroupId);
        assert existingGroup != null : "Group not found in TabGroupSyncService.";

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.COLLABORATION_FLOW_ANDROID)
                && mCollaborationControllerDelegateFactory != null) {
            assert mCollaborationService != null;
            // TODO(haileywang): Ensure createGroupFinishedCallback is called when the creation is
            // finished.
            mCurrentDelegate =
                    mCollaborationControllerDelegateFactory.create(
                            FlowType.SHARE_OR_MANAGE, /* switchToTabSwitcherCallback= */ null);
            mCollaborationService.startShareOrManageFlow(mCurrentDelegate, existingGroup.syncId);
            return;
        }

        if (existingGroup.collaborationId != null) {
            DataSharingMetrics.recordShareActionFlowState(
                    DataSharingMetrics.ShareActionStateAndroid.GROUP_EXISTS);
            showManageSharing(activity, existingGroup.collaborationId, /* finishRunnable= */ null);
            return;
        }

        assert mProfile != null;
        TabGroupSyncService tabGroupService = TabGroupSyncServiceFactory.getForProfile(mProfile);
        DataSharingCreateUiConfig.CreateCallback createCallback =
                new DataSharingCreateUiConfig.CreateCallback() {
                    @Override
                    public void onGroupCreatedWithWait(
                            org.chromium.components.sync.protocol.GroupData result,
                            Callback<Boolean> onCreateFinished) {
                        tabGroupService.makeTabGroupShared(localTabGroupId, result.getGroupId());
                        createGroupFinishedCallback.onResult(true);
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.GROUP_CREATE_SUCCESS);
                        // Consider using an utility to convert result to GroupData.
                        GURL url =
                                mDataSharingService.getDataSharingUrl(
                                        new GroupData(
                                                result.getGroupId(),
                                                /* displayName= */ null,
                                                /* members= */ null,
                                                result.getAccessToken()));
                        if (url == null) {
                            Callback.runNullSafe(onCreateFinished, false);
                            DataSharingMetrics.recordShareActionFlowState(
                                    DataSharingMetrics.ShareActionStateAndroid.URL_CREATION_FAILED);
                            return;
                        }
                        showShareSheet(activity, result.getGroupId(), url, onCreateFinished);
                    }

                    @Override
                    public void onCancelClicked() {
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.BOTTOM_SHEET_DISMISSED);
                    }

                    @Override
                    public void onSessionFinished() {
                        // TODO(haileywang) : Implement this.
                    }
                };
        showShareDialog(activity, existingGroup.title, existingGroup, createCallback);
    }

    /**
     * Show the share dialog screen.
     *
     * @param activity The activity to show the UI for.
     * @param tabGroupDisplayName The title of the tab group.
     * @param existingGroup The {@link SavedTabGroup} instance of the tab group.
     * @param createCallback The callbacks for the share ui.
     * @return The session id of the share screen.
     */
    public String showShareDialog(
            Activity activity,
            String tabGroupDisplayName,
            SavedTabGroup existingGroup,
            DataSharingCreateUiConfig.CreateCallback createCallback) {
        DataSharingUIDelegate uiDelegate = mDataSharingService.getUiDelegate();

        if (TextUtils.isEmpty(tabGroupDisplayName)) {
            tabGroupDisplayName =
                    TabGroupTitleUtils.getDefaultTitle(activity, existingGroup.savedTabs.size());
        }

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

        String sessionId =
                uiDelegate.showCreateFlow(
                        new DataSharingCreateUiConfig.Builder()
                                .setCommonConfig(
                                        getCommonConfig(
                                                activity, tabGroupDisplayName, stringConfig))
                                .setCreateCallback(createCallback)
                                .build());
        fetchFavicons(
                activity,
                sessionId,
                convertToTabsPreviewList(existingGroup.savedTabs),
                /* maxFaviconsToFetch= */ 4);

        return sessionId;
    }

    /**
     * Show share sheet UI.
     *
     * @param context The context where to show the share sheet.
     * @param collaborationId The group id for the tab group.
     * @param url The {@link GURL} of the tab group invitation.
     * @param onShareSheetShown The callback to run when share sheet opens.
     */
    public void showShareSheet(
            Context context,
            String collaborationId,
            GURL url,
            Callback<Boolean> onShareSheetShown) {
        mDataSharingTabGroupsDelegate.getPreviewBitmap(
                collaborationId,
                ShareHelper.getTextPreviewImageSizePx(mResources),
                (preview) -> {
                    showShareSheetWithPreview(
                            context, collaborationId, url, preview, onShareSheetShown);
                });
    }

    private void showShareSheetWithPreview(
            Context context,
            String collaborationId,
            GURL url,
            Bitmap preview,
            Callback<Boolean> onShareSheetShown) {
        DataSharingMetrics.recordShareActionFlowState(
                DataSharingMetrics.ShareActionStateAndroid.SHARE_SHEET_SHOWN);
        var chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.TAB_GROUP_LINK)
                        .build();
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        SavedTabGroup tabGroup =
                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                        collaborationId, tabGroupSyncService);
        String tabGroupName = null;
        // TODO(ssid): The tab group should not be null, if we wait for makeTabGroupShared() to
        // finish. Remove this check when its integrated.
        if (tabGroup != null) {
            tabGroupName = tabGroup.title;
        }
        if (TextUtils.isEmpty(tabGroupName)) {
            tabGroupName =
                    context.getString(R.string.collaboration_share_sheet_tab_group_fallback_name);
        }
        // TODO(ssid): Share delegate adds another separator, fix the formatting.
        String text =
                context.getString(R.string.collaboration_share_sheet_message, tabGroupName)
                        + SHARED_TEXT_SEPARATOR;
        ShareParams.Builder shareParamsBuilder =
                new ShareParams.Builder(
                                mWindowAndroid,
                                context.getString(R.string.collaboration_share_sheet_title),
                                url.getSpec())
                        .setText(text);

        if (preview != null) {
            shareParamsBuilder.setPreviewImageBitmap(preview);
        }
        mShareDelegateSupplier
                .get()
                .share(
                        shareParamsBuilder.build(),
                        chromeShareExtras,
                        ShareDelegate.ShareOrigin.TAB_GROUP);
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Callback.runNullSafe(onShareSheetShown, true);
                });
    }

    /**
     * Shows UI for manage sharing.
     *
     * @param activity The activity to show the UI for.
     * @param collaborationId The collaboration ID to show the UI for.
     * @param finishRunnable The runnable to run when the session is finished.
     * @return The session id associated with the UI instance.
     */
    public String showManageSharing(
            Activity activity, String collaborationId, @Nullable Runnable finishRunnable) {
        assert mProfile != null;

        DataSharingUIDelegate uiDelegate = mDataSharingService.getUiDelegate();
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        String tabGroupName =
                DataSharingTabGroupUtils.getTabGroupTitle(
                        activity, collaborationId, tabGroupSyncService);

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
                        .setResourceId(
                                DataSharingStringConfig.StringKey.BLOCK_MESSAGE,
                                R.string.collaboration_owner_block_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.BLOCK_AND_LEAVE_GROUP_MESSAGE,
                                R.string.collaboration_block_leave_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_BLOCKED_ACCOUNTS,
                                R.string.collaboration_block_leave_learn_more)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.REMOVE_MESSAGE,
                                R.string.collaboration_owner_remove_member_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEAVE_GROUP_MESSAGE,
                                R.string.collaboration_leave_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.STOP_SHARING_MESSAGE,
                                R.string.collaboration_owner_stop_sharing_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey
                                        .LET_ANYONE_JOIN_GROUP_WHEN_FULL_DESCRIPTION,
                                R.string.collaboration_group_is_full_description)
                        .build();

        DataSharingManageUiConfig.ManageCallback manageCallback =
                new DataSharingManageUiConfig.ManageCallback() {
                    @Override
                    public void onShareInviteLinkClicked(GroupToken groupToken) {
                        onShareInviteLinkClickedWithWait(groupToken, null);
                    }

                    @Override
                    public void onShareInviteLinkClickedWithWait(
                            GroupToken groupToken, Callback<Boolean> onFinished) {
                        GURL url =
                                mDataSharingService.getDataSharingUrl(
                                        new GroupData(
                                                groupToken.collaborationId,
                                                tabGroupName,
                                                /* members= */ null,
                                                groupToken.accessToken));
                        if (url == null) {
                            Callback.runNullSafe(onFinished, false);
                            DataSharingMetrics.recordShareActionFlowState(
                                    DataSharingMetrics.ShareActionStateAndroid.URL_CREATION_FAILED);
                            return;
                        }
                        showShareSheet(activity, groupToken.collaborationId, url, onFinished);
                    }

                    @Override
                    public void onStopSharingInitiated(Callback<Boolean> readyToStopSharing) {
                        SavedTabGroup existingGroup =
                                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                                        collaborationId, tabGroupSyncService);
                        tabGroupSyncService.aboutToUnShareTabGroup(
                                existingGroup.localId, readyToStopSharing);
                    }

                    @Override
                    public void onStopSharingCompleted(boolean success) {
                        SavedTabGroup existingGroup =
                                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                                        collaborationId, tabGroupSyncService);
                        tabGroupSyncService.onTabGroupUnShareComplete(
                                existingGroup.localId, success);
                    }

                    @Override
                    public void onSessionFinished() {
                        if (finishRunnable != null) {
                            finishRunnable.run();
                        }
                    }
                };
        DataSharingManageUiConfig manageConfig =
                new DataSharingManageUiConfig.Builder()
                        .setGroupToken(new GroupToken(collaborationId, null))
                        .setManageCallback(manageCallback)
                        .setLearnAboutBlockedAccounts(getLearnAboutBlockedAccountsUrl())
                        .setActivityLogsUrl(getActivityLogsUrl())
                        .setCommonConfig(getCommonConfig(activity, tabGroupName, stringConfig))
                        .build();
        return uiDelegate.showManageFlow(manageConfig);
    }

    private DataSharingUiConfig getCommonConfig(
            Activity activity, String tabGroupName, DataSharingStringConfig stringConfig) {
        DataSharingUiConfig.DataSharingCallback dataSharingCallback =
                new DataSharingUiConfig.DataSharingCallback() {
                    @Override
                    public void onClickOpenChromeCustomTab(Context context, GURL url) {
                        mDataSharingTabGroupsDelegate.openUrlInChromeCustomTab(context, url);
                    }
                };
        DataSharingUiConfig.Builder commonConfig =
                new DataSharingUiConfig.Builder()
                        .setActivity(activity)
                        .setIsTablet(DeviceFormFactor.isWindowOnTablet(mWindowAndroid))
                        .setLearnMoreHyperLink(getTabGroupHelpUrl())
                        .setDataSharingStringConfig(stringConfig)
                        .setDataSharingCallback(dataSharingCallback);
        if (tabGroupName != null) {
            commonConfig.setTabGroupName(tabGroupName);
        }
        return commonConfig.build();
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
        Runnable manageSharingCallback =
                () ->
                        createOrManageFlow(
                                activity,
                                existingGroup.syncId,
                                /* localTabGroupId= */ null,
                                /* createGroupFinishedCallback= */ null);
        RecentActivityActionHandler recentActivityActionHandler =
                new RecentActivityActionHandlerImpl(
                        tabGroupSyncService,
                        mTabModelSelectorSupplier.get(),
                        mDataSharingTabGroupsDelegate,
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

    /**
     * Gets the {@link SavedTabGroup} instance given either sync or local ID.
     *
     * @param syncId The associated sync ID.
     * @param localId The associated local ID.
     */
    public SavedTabGroup getSavedTabGroupForEitherId(
            @Nullable String syncId, @Nullable LocalTabGroupId localId) {
        assert syncId != null || localId != null;
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        assert tabGroupSyncService != null;

        SavedTabGroup existingGroup = null;
        if (syncId != null) {
            existingGroup = tabGroupSyncService.getGroup(syncId);
        } else {
            existingGroup = tabGroupSyncService.getGroup(localId);
        }
        assert existingGroup != null;

        return existingGroup;
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
