// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;

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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.LinkedList;

/**
 * This class is responsible for handling communication from the UI to multiple data sharing
 * services. This class is created once per {@link ChromeTabbedActivity}.
 */
public class DataSharingTabManager {
    private static final String TAG = "DataSharing";

    // Separator for description and link in share sheet.
    private static final String SHARED_TEXT_SEPARATOR = "";

    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Resources mResources;
    private final OneshotSupplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final LinkedList<Runnable> mTasksToRunOnProfileAvailable = new LinkedList<>();
    private final BulkFaviconUtil mBulkFaviconUtil = new BulkFaviconUtil();
    private final CollaborationControllerDelegateFactory mCollaborationControllerDelegateFactory;

    private @Nullable Profile mProfile;
    private @Nullable DataSharingService mDataSharingService;
    private @Nullable MessagingBackendService mMessagingBackendService;
    private @Nullable CollaborationService mCollaborationService;
    private @Nullable CollaborationControllerDelegate mCurrentDelegate;

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
     * @return The {@link DataSharingTabGroupsDelegate} instance associated with the tab manager.
     */
    public DataSharingTabGroupsDelegate getTabGroupsDelegate() {
        return mDataSharingTabGroupsDelegate;
    }

    /**
     * @return The {@link DataSharingService} instance associated with the tab manager.
     */
    public DataSharingService getDataSharingService() {
        return mDataSharingService;
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
    public void initiateJoinFlow(GURL dataSharingUrl) {
        initiateJoinFlow(dataSharingUrl, /* switchToTabSwitcherCallback= */ null);
    }

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param dataSharingUrl The URL associated with the join invitation.
     * @param switchToTabSwitcherCallback The callback to allow to switch to tab switcher view.
     */
    public void initiateJoinFlow(
            GURL dataSharingUrl, Callback<Runnable> switchToTabSwitcherCallback) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.JOIN_TRIGGERED);
        if (mProfile != null) {
            initiateJoinFlowWithProfile(dataSharingUrl, switchToTabSwitcherCallback);
            return;
        }

        mTasksToRunOnProfileAvailable.addLast(
                () -> {
                    initiateJoinFlowWithProfile(dataSharingUrl, switchToTabSwitcherCallback);
                });
    }

    private void initiateJoinFlowWithProfile(
            GURL dataSharingUrl, Callback<Runnable> switchToTabSwitcherCallback) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.PROFILE_AVAILABLE);

        mCurrentDelegate =
                mCollaborationControllerDelegateFactory.create(
                        FlowType.JOIN, switchToTabSwitcherCallback);
        mCollaborationService.startJoinFlow(mCurrentDelegate, dataSharingUrl);
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

        // TODO(haileywang): Ensure createGroupFinishedCallback is called when the creation is
        // finished.
        mCurrentDelegate =
                mCollaborationControllerDelegateFactory.create(
                        FlowType.SHARE_OR_MANAGE, /* switchToTabSwitcherCallback= */ null);
        mCollaborationService.startShareOrManageFlow(mCurrentDelegate, existingGroup.syncId);
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

    BulkFaviconUtil getBulkFaviconUtilForTesting() {
        return mBulkFaviconUtil;
    }
}
