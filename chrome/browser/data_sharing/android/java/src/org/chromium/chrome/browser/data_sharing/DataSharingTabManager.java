// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.CollaborationControllerDelegateFactory;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityActionHandler;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceLeaveOrDeleteEntryPoint;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.Outcome;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
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
import org.chromium.components.data_sharing.configs.DataSharingUiConfig.DataSharingUserAction;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

/**
 * This class is responsible for handling communication from the UI to multiple data sharing
 * services. This class is created once per {@link ChromeTabbedActivity}.
 */
@NullMarked
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
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Resources mResources;
    private final OneshotSupplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final LinkedList<Runnable> mTasksToRunOnProfileAvailable = new LinkedList<>();
    private final BulkFaviconUtil mBulkFaviconUtil = new BulkFaviconUtil();
    private final CollaborationControllerDelegateFactory mCollaborationControllerDelegateFactory;

    private @MonotonicNonNull Profile mProfile;
    private @MonotonicNonNull DataSharingService mDataSharingService;
    private @Nullable MessagingBackendService mMessagingBackendService;
    private @MonotonicNonNull CollaborationService mCollaborationService;
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
    public @Nullable Profile getProfile() {
        return mProfile;
    }

    /**
     * @return The {@link WindowAndroid} instance associated with the tab manager.
     */
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * @return The {@link DataSharingUIDelegate} instance associated with the tab manager.
     */
    public @Nullable DataSharingUIDelegate getUiDelegate() {
        if (mDataSharingService == null) return null;
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
    @Initializer
    public void initWithProfile(
            Profile profile,
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
            GURL dataSharingUrl, @Nullable Callback<Runnable> switchToTabSwitcherCallback) {
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
            GURL dataSharingUrl, @Nullable Callback<Runnable> switchToTabSwitcherCallback) {
        DataSharingMetrics.recordJoinActionFlowState(
                DataSharingMetrics.JoinActionStateAndroid.PROFILE_AVAILABLE);

        mCurrentDelegate =
                mCollaborationControllerDelegateFactory.create(
                        FlowType.JOIN, switchToTabSwitcherCallback);
        assumeNonNull(mCollaborationService);
        mCollaborationService.startJoinFlow(mCurrentDelegate, dataSharingUrl);
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
    public @Nullable String showJoinScreenWithPreview(
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
        List<TabPreview> tabs = assumeNonNull(previewTabGroupData.tabs);
        if (TextUtils.isEmpty(tabGroupName)) {
            tabGroupName = TabGroupTitleUtils.getDefaultTitle(activity, tabs.size());
        }

        assumeNonNull(mDataSharingService);
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
        fetchFavicons(activity, sessionId, tabs, tabs.size());
        return sessionId;
    }

    private void fetchFavicons(
            Activity activity,
            @Nullable String sessionId,
            List<TabPreview> tabs,
            int maxFaviconsToFetch) {
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
            @Nullable String sessionId,
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
                assumeNonNull(mProfile),
                urls,
                activity.getResources()
                        .getDimensionPixelSize(R.dimen.shared_tab_group_favicon_bitmap_size),
                (favicons) -> {
                    updateFavicons(sessionId, displayUrls, favicons);
                    doneCallback.run();
                });
    }

    private void updateFavicons(
            @Nullable String sessionId, List<String> displayUrls, List<Bitmap> favicons) {
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
        assumeNonNull(runtimeConfig);
        assumeNonNull(mDataSharingService);
        mDataSharingService.getUiDelegate().updateRuntimeData(sessionId, runtimeConfig);
    }

    private List<TabPreview> convertToTabsPreviewList(List<SavedTabGroupTab> savedTabs) {
        int tabsCount = savedTabs.size();
        List<TabPreview> preview = new ArrayList<>();
        for (int i = 0; i < tabsCount; ++i) {
            // displayUrl field is not used in the create or manage UI where local tab group is
            // available.
            SavedTabGroupTab savedTab = savedTabs.get(i);
            preview.add(new TabPreview(assumeNonNull(savedTab.url), /* displayUrl= */ ""));
        }
        return preview;
    }

    private TabGroupModelFilter getTabGroupModelFilter() {
        return assumeNonNull(
                mTabModelSelectorSupplier
                        .get()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false));
    }

    /**
     * Switch the view to a currently opened tab group.
     *
     * @param group The copy of the sync group. May not be part of the current tab model.
     */
    void displayTabGroupUi(SavedTabGroup group) {
        mDataSharingTabGroupsDelegate.openTabGroup(assumeNonNull(group.localId).tabGroupId);
    }

    /**
     * Open and focus on the tab group. May switch windows.
     *
     * @param collaborationId The collaboration id of the shared tab group.
     * @param isFromInviteFlow If the call is from the invite flow, used for metrics.
     * @return If the attempt to show the tab group may be successful or we know it failed.
     */
    public boolean displayTabGroupAnywhere(String collaborationId, boolean isFromInviteFlow) {
        // TODO(https://crbug.com/414873807): Move this logic to /collaboration/.
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(assumeNonNull(mProfile));
        SavedTabGroup syncGroup =
                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                        collaborationId, assumeNonNull(tabGroupSyncService));
        assumeNonNull(syncGroup);
        assumeNonNull(syncGroup.syncId);

        TabGroupModelFilter filter = getTabGroupModelFilter();
        if (syncGroup.localId == null) {
            openTabGroupInLocalAndShow(syncGroup);
        } else if (TabGroupSyncUtils.isInCurrentWindow(filter, syncGroup.localId)) {
            if (isFromInviteFlow) {
                DataSharingMetrics.recordJoinActionFlowState(
                        DataSharingMetrics.JoinActionStateAndroid.LOCAL_TAB_GROUP_EXISTS);
            }
            displayTabGroupUi(syncGroup);
        } else {
            // Because syncGroup.localId is non-null, we can assume the tab group exists in another
            // window. Now need to fire an intent to switch to the right window. Depending on the
            // approach our delegates take, this could chain between windows searching.
            @WindowId
            int windowId =
                    mDataSharingTabGroupsDelegate.findWindowIdForTabGroup(
                            syncGroup.localId.tabGroupId);
            DataSharingUIDelegate uiDelegate = getUiDelegate();
            if (windowId == INVALID_WINDOW_ID || uiDelegate == null) return false;

            // This may be switching from invite flow to manage. But that's okay when the group
            // already exists on the device. They'll both end up just opening the group dialog.
            Context context = ContextUtils.getApplicationContext();
            Intent intent = DataSharingIntentUtils.createManageIntent(context, syncGroup.syncId);
            mDataSharingTabGroupsDelegate.launchIntentInMaybeClosedWindow(intent, windowId);
        }
        return true;
    }

    void openTabGroupInLocalAndShow(SavedTabGroup group) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(assumeNonNull(mProfile));

        mTabGroupUiActionHandlerSupplier.runSyncOrOnAvailable(
                (tabGroupUiActionHandler) -> {
                    // Note: This does not switch the active tab to the opened tab.
                    String syncId = assertNonNull(group.syncId);
                    tabGroupUiActionHandler.openTabGroup(syncId);
                    DataSharingMetrics.recordJoinActionFlowState(
                            DataSharingMetrics.JoinActionStateAndroid.LOCAL_TAB_GROUP_ADDED);
                    SavedTabGroup savedTabGroup =
                            assumeNonNull(tabGroupSyncService).getGroup(syncId);
                    displayTabGroupUi(assumeNonNull(savedTabGroup));
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
            Activity activity,
            Tab tab,
            @CollaborationServiceShareOrManageEntryPoint int entryPoint,
            Callback<Boolean> createGroupFinishedCallback) {
        if (tab.getTabGroupId() == null) {
            getTabGroupModelFilter().createSingleTabGroup(tab);
        }
        createOrManageFlow(
                EitherGroupId.createLocalId(
                        new LocalTabGroupId(assumeNonNull(tab.getTabGroupId()))),
                entryPoint,
                createGroupFinishedCallback);
    }

    /**
     * Creates or manage a collaboration group.
     *
     * @param eitherId The sync ID or local tab group ID of the tab group.
     * @param entry The entry point of the flow.
     * @param createGroupFinishedCallback Callback invoked when the creation flow is finished.
     */
    public void createOrManageFlow(
            EitherGroupId eitherId,
            @CollaborationServiceShareOrManageEntryPoint int entry,
            @Nullable Callback<Boolean> createGroupFinishedCallback) {
        DataSharingMetrics.recordShareActionFlowState(
                DataSharingMetrics.ShareActionStateAndroid.SHARE_TRIGGERED);

        // TODO(haileywang): Ensure createGroupFinishedCallback is called when the creation is
        // finished.
        mCurrentDelegate =
                mCollaborationControllerDelegateFactory.create(
                        FlowType.SHARE_OR_MANAGE, /* switchToTabSwitcherCallback= */ null);
        assumeNonNull(mCollaborationService);
        mCollaborationService.startShareOrManageFlow(mCurrentDelegate, eitherId, entry);
    }

    /**
     * Leave or delete a collaboration group.
     *
     * @param eitherId The sync ID or local tab group ID of the tab group.
     * @param entry The entry point of the flow.
     */
    public void leaveOrDeleteFlow(
            EitherGroupId eitherId, @CollaborationServiceLeaveOrDeleteEntryPoint int entry) {
        mCurrentDelegate =
                mCollaborationControllerDelegateFactory.create(
                        FlowType.LEAVE_OR_DELETE, /* switchToTabSwitcherCallback= */ null);
        assumeNonNull(mCollaborationService);
        mCollaborationService.startLeaveOrDeleteFlow(mCurrentDelegate, eitherId, entry);
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
    public @Nullable String showShareDialog(
            Activity activity,
            @Nullable String tabGroupDisplayName,
            SavedTabGroup existingGroup,
            DataSharingCreateUiConfig.CreateCallback createCallback) {
        assumeNonNull(mDataSharingService);
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
            @Nullable Callback<Boolean> onShareSheetShown) {
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
            @Nullable Callback<Boolean> onShareSheetShown) {
        DataSharingMetrics.recordShareActionFlowState(
                DataSharingMetrics.ShareActionStateAndroid.SHARE_SHEET_SHOWN);
        var chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.TAB_GROUP_LINK)
                        .build();
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(assumeNonNull(mProfile));
        assumeNonNull(tabGroupSyncService);
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
    public @Nullable String showManageSharing(
            Activity activity,
            String collaborationId,
            @Nullable Callback<@Outcome Integer> outcomeCallback) {
        assert mProfile != null;

        assumeNonNull(mDataSharingService);
        DataSharingUIDelegate uiDelegate = mDataSharingService.getUiDelegate();
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfile);
        assumeNonNull(tabGroupSyncService);
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
                        .setResourceId(
                                DataSharingStringConfig.StringKey.ACTIVITY_LOGS_TITLE,
                                R.string.data_sharing_shared_tab_groups_activity)
                        .build();

        DataSharingManageUiConfig.ManageCallback manageCallback =
                new DataSharingManageUiConfig.ManageCallback() {
                    private @Nullable Callback<@Outcome Integer> mOutcomeCallback;

                    {
                        mOutcomeCallback = outcomeCallback;
                    }

                    @Override
                    public void onShareInviteLinkClicked(GroupToken groupToken) {
                        onShareInviteLinkClickedWithWait(groupToken, null);
                    }

                    @Override
                    public void onShareInviteLinkClickedWithWait(
                            GroupToken groupToken, @Nullable Callback<Boolean> onFinished) {
                        GURL url =
                                mDataSharingService.getDataSharingUrl(
                                        new GroupData(
                                                groupToken.collaborationId,
                                                assumeNonNull(tabGroupName),
                                                /* members= */ null,
                                                assumeNonNull(groupToken.accessToken)));
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
                        assumeNonNull(existingGroup);
                        tabGroupSyncService.aboutToUnShareTabGroup(
                                assumeNonNull(existingGroup.localId), readyToStopSharing);
                    }

                    @Override
                    public void onStopSharingCompleted(boolean success) {
                        SavedTabGroup existingGroup =
                                assumeNonNull(
                                        DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                                                collaborationId, tabGroupSyncService));
                        tabGroupSyncService.onTabGroupUnShareComplete(
                                assumeNonNull(existingGroup.localId), success);
                    }

                    @Override
                    public void onLeaveGroup() {
                        Callback<@Outcome Integer> callback = mOutcomeCallback;
                        mOutcomeCallback = null;

                        // TODO(haileywang): remove assert if we don't observe any crash
                        assert callback != null;
                        if (callback != null) {
                            callback.onResult(Outcome.GROUP_LEFT_OR_DELETED);
                        }
                    }

                    @Override
                    public void onSessionFinished() {
                        if (mOutcomeCallback != null) {
                            mOutcomeCallback.onResult(Outcome.SUCCESS);
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
            Activity activity,
            @Nullable String tabGroupName,
            DataSharingStringConfig stringConfig) {
        DataSharingUiConfig.DataSharingCallback dataSharingCallback =
                new DataSharingUiConfig.DataSharingCallback() {
                    @Override
                    public void onClickOpenChromeCustomTab(Context context, GURL url) {
                        mDataSharingTabGroupsDelegate.openUrlInChromeCustomTab(context, url);
                    }

                    @Override
                    public void recordUserActionClicks(
                            @DataSharingUserAction int dataSharingUserAction) {
                        DataSharingMetrics.recordUserActionClicks(dataSharingUserAction);
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
                        collaborationId, assumeNonNull(tabGroupSyncService));
        if (existingGroup == null) return;

        assumeNonNull(mDataSharingService);
        DataSharingAvatarProvider avatarProvider =
                new DataSharingAvatarProvider(activity, mDataSharingService.getUiDelegate());

        // TODO(crbug.com/380962101): Extract manage sharing into a different interface.
        Runnable manageSharingCallback =
                () ->
                        createOrManageFlow(
                                EitherGroupId.createSyncId(assumeNonNull(existingGroup.syncId)),
                                CollaborationServiceShareOrManageEntryPoint.RECENT_ACTIVITY,
                                /* createGroupFinishedCallback= */ null);
        assumeNonNull(existingGroup.syncId);
        RecentActivityActionHandler recentActivityActionHandler =
                new RecentActivityActionHandlerImpl(
                        tabGroupSyncService,
                        mTabModelSelectorSupplier.get(),
                        mDataSharingTabGroupsDelegate,
                        collaborationId,
                        existingGroup.syncId,
                        manageSharingCallback);

        Runnable showFullActivityRunnable =
                () -> {
                    mDataSharingTabGroupsDelegate.openUrlInChromeCustomTab(
                            activity, new GURL(ACTIVITY_LOGS_URL));
                };
        RecentActivityListCoordinator recentActivityListCoordinator =
                new RecentActivityListCoordinator(
                        collaborationId,
                        activity,
                        mBottomSheetControllerSupplier.get(),
                        mMessagingBackendService,
                        tabGroupSyncService,
                        new DataSharingFaviconProvider(activity, mProfile, mBulkFaviconUtil),
                        avatarProvider,
                        recentActivityActionHandler,
                        showFullActivityRunnable);
        recentActivityListCoordinator.requestShowUI();
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
                TabGroupSyncServiceFactory.getForProfile(assumeNonNull(mProfile));
        assert tabGroupSyncService != null;

        SavedTabGroup existingGroup = null;
        if (syncId != null) {
            existingGroup = tabGroupSyncService.getGroup(syncId);
        } else {
            existingGroup = tabGroupSyncService.getGroup(assumeNonNull(localId));
        }
        assert existingGroup != null;

        return existingGroup;
    }

    BulkFaviconUtil getBulkFaviconUtilForTesting() {
        return mBulkFaviconUtil;
    }

    /** Override ShareDelegateSupplier for testing. */
    public void setShareDelegateSupplierForTesting(
            ObservableSupplier<ShareDelegate> shareDelegateSupplier) {
        mShareDelegateSupplier = shareDelegateSupplier;
    }
}
