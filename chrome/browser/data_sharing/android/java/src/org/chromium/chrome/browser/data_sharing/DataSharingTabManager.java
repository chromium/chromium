// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseURLStatus;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class is responsible for handling communication from the UI to multiple data sharing
 * services. This class is created once per {@link ChromeTabbedActivity}.
 */
public class DataSharingTabManager {
    private static final String TAG = "DataSharing";

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Resources mResources;
    private final OneshotSupplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private Callback<Profile> mProfileObserver;
    private Map</*collaborationId*/ String, SyncObserver> mSyncObserversList;

    /** This class is responsible for observing sync tab activities. */
    private static class SyncObserver implements TabGroupSyncService.Observer {
        private final String mDataSharingGroupId;
        private final TabGroupSyncService mTabGroupSyncService;
        private Callback<SavedTabGroup> mCallback;

        SyncObserver(
                String dataSharingGroupId,
                TabGroupSyncService tabGroupSyncService,
                Callback<SavedTabGroup> callback) {
            mDataSharingGroupId = dataSharingGroupId;
            mTabGroupSyncService = tabGroupSyncService;
            mCallback = callback;

            mTabGroupSyncService.addObserver(this);
        }

        @Override
        public void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {
            if (mDataSharingGroupId.equals(group.collaborationId)) {
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
     * @param profileSupplier The supplier of the currently applicable profile.
     * @param bottomSheetControllerSupplier The supplier of bottom sheet state controller.
     * @param shareDelegateSupplier The supplier of share delegate.
     * @param windowAndroid The window base class that has the minimum functionality.
     * @param resources Used to load localized android resources.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open tab groups
     *     locally.
     */
    public DataSharingTabManager(
            DataSharingTabSwitcherDelegate tabSwitcherDelegate,
            ObservableSupplier<Profile> profileSupplier,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Resources resources,
            OneshotSupplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier) {
        mDataSharingTabSwitcherDelegate = tabSwitcherDelegate;
        mProfileSupplier = profileSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mResources = resources;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mSyncObserversList = new HashMap<>();
        assert mProfileSupplier != null;
        assert mBottomSheetControllerSupplier != null;
        assert mShareDelegateSupplier != null;
    }

    /** Cleans up any outstanding resources. */
    public void destroy() {
        for (Map.Entry<String, SyncObserver> entry : mSyncObserversList.entrySet()) {
            entry.getValue().destroy();
        }
        mSyncObserversList.clear();
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(JoinActionStateAndroid)
    @IntDef({
        JoinActionStateAndroid.JOIN_TRIGGERED,
        JoinActionStateAndroid.PROFILE_AVAILABLE,
        JoinActionStateAndroid.PARSE_URL_FAILED,
        JoinActionStateAndroid.SYNCED_TAB_GROUP_EXISTS,
        JoinActionStateAndroid.LOCAL_TAB_GROUP_EXISTS,
        JoinActionStateAndroid.LOCAL_TAB_GROUP_ADDED,
        JoinActionStateAndroid.LOCAL_TAB_GROUP_OPENED,
        JoinActionStateAndroid.ADD_MEMBER_FAILED,
        JoinActionStateAndroid.ADD_MEMBER_SUCCESS,
        JoinActionStateAndroid.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface JoinActionStateAndroid {
        int JOIN_TRIGGERED = 0;
        int PROFILE_AVAILABLE = 1;
        int PARSE_URL_FAILED = 2;
        int SYNCED_TAB_GROUP_EXISTS = 3;
        int LOCAL_TAB_GROUP_EXISTS = 4;
        int LOCAL_TAB_GROUP_ADDED = 5;
        int LOCAL_TAB_GROUP_OPENED = 6;
        int ADD_MEMBER_FAILED = 7;
        int ADD_MEMBER_SUCCESS = 8;
        int COUNT = 9;
    }

    // LINT.ThenChange(//tools/metrics/histograms/data_sharing/enums.xml:JoinActionStateAndroid)

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param dataSharingURL The URL associated with the join invitation.
     */
    public void initiateJoinFlow(GURL dataSharingURL) {
        RecordHistogram.recordEnumeratedHistogram(
                "DataSharing.Android.JoinActionFlowState",
                JoinActionStateAndroid.JOIN_TRIGGERED,
                JoinActionStateAndroid.COUNT);
        if (mProfileSupplier.hasValue() && mProfileSupplier.get().getOriginalProfile() != null) {
            initiateJoinFlowWithProfile(dataSharingURL);
            return;
        }

        assert mProfileObserver == null;
        mProfileObserver =
                profile -> {
                    mProfileSupplier.removeObserver(mProfileObserver);
                    initiateJoinFlowWithProfile(dataSharingURL);
                };

        mProfileSupplier.addObserver(mProfileObserver);
    }

    private void initiateJoinFlowWithProfile(GURL dataSharingURL) {
        RecordHistogram.recordEnumeratedHistogram(
                "DataSharing.Android.JoinActionFlowState",
                JoinActionStateAndroid.PROFILE_AVAILABLE,
                JoinActionStateAndroid.COUNT);
        Profile originalProfile = mProfileSupplier.get().getOriginalProfile();
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(originalProfile);
        DataSharingService dataSharingService =
                DataSharingServiceFactory.getForProfile(originalProfile);
        assert tabGroupSyncService != null;
        assert dataSharingService != null;

        DataSharingService.ParseURLResult parseResult =
                dataSharingService.parseDataSharingURL(dataSharingURL);
        if (parseResult.status != ParseURLStatus.SUCCESS) {
            showInvitationFailureDialog();
            RecordHistogram.recordEnumeratedHistogram(
                    "DataSharing.Android.JoinActionFlowState",
                    JoinActionStateAndroid.PARSE_URL_FAILED,
                    JoinActionStateAndroid.COUNT);
            return;
        }

        GroupToken groupToken = parseResult.groupToken;
        String groupId = groupToken.groupId;
        // Verify that tab group does not already exist in sync.
        SavedTabGroup existingGroup = getTabGroupForCollabIdFromSync(groupId, tabGroupSyncService);
        if (existingGroup != null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "DataSharing.Android.JoinActionFlowState",
                    JoinActionStateAndroid.SYNCED_TAB_GROUP_EXISTS,
                    JoinActionStateAndroid.COUNT);
            onSavedTabGroupAvailable(existingGroup);
            return;
        }

        // TODO(b/354003616): Show loading dialog while waiting for tab.
        if (!mSyncObserversList.containsKey(groupId)) {
            SyncObserver syncObserver =
                    new SyncObserver(
                            groupId,
                            tabGroupSyncService,
                            (group) -> {
                                onSavedTabGroupAvailable(group);
                                mSyncObserversList.remove(group.collaborationId);
                            });

            mSyncObserversList.put(groupId, syncObserver);
        }

        dataSharingService.addMember(
                groupToken.groupId,
                groupToken.accessToken,
                result -> {
                    if (result != PeopleGroupActionOutcome.SUCCESS) {
                        RecordHistogram.recordEnumeratedHistogram(
                                "DataSharing.Android.JoinActionFlowState",
                                JoinActionStateAndroid.ADD_MEMBER_FAILED,
                                JoinActionStateAndroid.COUNT);
                        showInvitationFailureDialog();
                    }
                    RecordHistogram.recordEnumeratedHistogram(
                            "DataSharing.Android.JoinActionFlowState",
                            JoinActionStateAndroid.ADD_MEMBER_SUCCESS,
                            JoinActionStateAndroid.COUNT);
                });
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

    SavedTabGroup getTabGroupForCollabIdFromSync(
            String collaborationId, TabGroupSyncService tabGroupSyncService) {
        for (String syncGroupId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(syncGroupId);
            assert !savedTabGroup.savedTabs.isEmpty();
            if (savedTabGroup.collaborationId != null
                    && savedTabGroup.collaborationId.equals(collaborationId)) {
                return savedTabGroup;
            }
        }

        return null;
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
            RecordHistogram.recordEnumeratedHistogram(
                    "DataSharing.Android.JoinActionFlowState",
                    JoinActionStateAndroid.LOCAL_TAB_GROUP_EXISTS,
                    JoinActionStateAndroid.COUNT);
            switchToTabGroup(group);
            return;
        }

        openLocalTabGroup(group);
    }

    void openLocalTabGroup(SavedTabGroup group) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(
                        mProfileSupplier.get().getOriginalProfile());

        mTabGroupUiActionHandlerSupplier.runSyncOrOnAvailable(
                (tabGroupUiActionHandler) -> {
                    // Note: This does not switch the active tab to the opened tab.
                    tabGroupUiActionHandler.openTabGroup(group.syncId);
                    RecordHistogram.recordEnumeratedHistogram(
                            "DataSharing.Android.JoinActionFlowState",
                            JoinActionStateAndroid.LOCAL_TAB_GROUP_ADDED,
                            JoinActionStateAndroid.COUNT);
                    SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(group.syncId);
                    switchToTabGroup(savedTabGroup);
                    RecordHistogram.recordEnumeratedHistogram(
                            "DataSharing.Android.JoinActionFlowState",
                            JoinActionStateAndroid.LOCAL_TAB_GROUP_OPENED,
                            JoinActionStateAndroid.COUNT);
                });
    }

    /**
     * Stop observing a data sharing tab group from sync.
     *
     * @param observer The observer to be removed.
     */
    protected void deleteSyncObserver(SyncObserver observer) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(
                        mProfileSupplier.get().getOriginalProfile());

        if (tabGroupSyncService != null) {
            tabGroupSyncService.removeObserver(observer);
        }
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(ShareActionStateAndroid)
    @IntDef({
        ShareActionStateAndroid.SHARE_TRIGGERED,
        ShareActionStateAndroid.GROUP_EXISTS,
        ShareActionStateAndroid.ENSURE_VISIBILITY_FAILED,
        ShareActionStateAndroid.BOTTOM_SHEET_DISMISSED,
        ShareActionStateAndroid.GROUP_CREATE_SUCCESS,
        ShareActionStateAndroid.GROUP_CREATE_FAILED,
        ShareActionStateAndroid.URL_CREATION_FAILED,
        ShareActionStateAndroid.SHARE_SHEET_SHOWN,
        ShareActionStateAndroid.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ShareActionStateAndroid {
        int SHARE_TRIGGERED = 0;
        int GROUP_EXISTS = 1;
        int ENSURE_VISIBILITY_FAILED = 2;
        int BOTTOM_SHEET_DISMISSED = 3;
        int GROUP_CREATE_SUCCESS = 4;
        int GROUP_CREATE_FAILED = 5;
        int URL_CREATION_FAILED = 6;
        int SHARE_SHEET_SHOWN = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/data_sharing/enums.xml:ShareActionStateAndroid)

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
        RecordHistogram.recordEnumeratedHistogram(
                "DataSharing.Android.ShareActionFlowState",
                ShareActionStateAndroid.SHARE_TRIGGERED,
                ShareActionStateAndroid.COUNT);
        Profile profile = mProfileSupplier.get().getOriginalProfile();
        assert profile != null;
        TabGroupSyncService tabGroupService = TabGroupSyncServiceFactory.getForProfile(profile);
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);

        SavedTabGroup existingGroup = tabGroupService.getGroup(localTabGroupId);
        assert existingGroup != null : "Group not found in TabGroupSyncService.";
        if (existingGroup.collaborationId != null) {
            dataSharingService.ensureGroupVisibility(
                    existingGroup.collaborationId,
                    (result) -> {
                        if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN
                                || result.groupData == null) {
                            // TODO(ritikagup): Show error dialog telling failed to create access
                            // token.
                            RecordHistogram.recordEnumeratedHistogram(
                                    "DataSharing.Android.ShareActionFlowState",
                                    ShareActionStateAndroid.ENSURE_VISIBILITY_FAILED,
                                    ShareActionStateAndroid.COUNT);
                        } else {
                            showShareSheet(result.groupData);
                        }
                    });
            return;
        }

        Callback<Integer> onClosedCallback =
                (reason) -> {
                    if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                        RecordHistogram.recordEnumeratedHistogram(
                                "DataSharing.Android.ShareActionFlowState",
                                ShareActionStateAndroid.BOTTOM_SHEET_DISMISSED,
                                ShareActionStateAndroid.COUNT);
                        createGroupFinishedCallback.onResult(false);
                    }
                };
        BottomSheetContent bottomSheetContent = showBottomSheet(activity, onClosedCallback);

        Callback<DataSharingService.GroupDataOrFailureOutcome> createGroupCallback =
                (result) -> {
                    if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN
                            || result.groupData == null) {
                        Log.e(TAG, "Group creation failed " + result.actionFailure);
                        createGroupFinishedCallback.onResult(false);
                        RecordHistogram.recordEnumeratedHistogram(
                                "DataSharing.Android.ShareActionFlowState",
                                ShareActionStateAndroid.GROUP_CREATE_FAILED,
                                ShareActionStateAndroid.COUNT);
                    } else {
                        tabGroupService.makeTabGroupShared(
                                localTabGroupId, result.groupData.groupToken.groupId);
                        createGroupFinishedCallback.onResult(true);

                        RecordHistogram.recordEnumeratedHistogram(
                                "DataSharing.Android.ShareActionFlowState",
                                ShareActionStateAndroid.GROUP_CREATE_SUCCESS,
                                ShareActionStateAndroid.COUNT);
                        showShareSheet(result.groupData);
                    }
                    mBottomSheetControllerSupplier
                            .get()
                            .hideContent(
                                    bottomSheetContent,
                                    /* animate= */ false,
                                    StateChangeReason.INTERACTION_COMPLETE);
                };

        Callback<List<String>> pickerCallback =
                (emails) -> {
                    dataSharingService.createGroup(tabGroupDisplayName, createGroupCallback);
                };
        DataSharingUIDelegate uiDelegate = dataSharingService.getUIDelegate();
        assert uiDelegate != null;

        uiDelegate.showMemberPicker(
                activity,
                (ViewGroup) bottomSheetContent.getContentView(),
                new MemberPickerListenerImpl(pickerCallback),
                /* config= */ null);
    }

    private void showShareSheet(GroupData groupData) {
        DataSharingService dataSharingService =
                DataSharingServiceFactory.getForProfile(
                        mProfileSupplier.get().getOriginalProfile());
        GURL url = dataSharingService.getDataSharingURL(groupData);
        if (url == null) {
            // TODO(ritikagup) : Show error dialog showing fetching URL failed. Contact owner for
            // new link.
            RecordHistogram.recordEnumeratedHistogram(
                    "DataSharing.Android.ShareActionFlowState",
                    ShareActionStateAndroid.URL_CREATION_FAILED,
                    ShareActionStateAndroid.COUNT);
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "DataSharing.Android.ShareActionFlowState",
                ShareActionStateAndroid.SHARE_SHEET_SHOWN,
                ShareActionStateAndroid.COUNT);
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
        BottomSheetContent bottomSheetContent =
                showBottomSheet(activity, /* onClosedCallback= */ null);

        Profile profile = mProfileSupplier.get().getOriginalProfile();
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);
        DataSharingUIDelegate uiDelegate = dataSharingService.getUIDelegate();

        // This API is likely to change in the future.
        uiDelegate.createGroupMemberListView(
                activity,
                (ViewGroup) bottomSheetContent.getContentView(),
                collaborationId,
                /* tokenSecret= */ null,
                /* config= */ null);
    }

    /**
     * Shows UI for recent activity.
     *
     * @param collaborationId The collaboration ID to show the UI for.
     */
    public void showRecentActivity(String collaborationId) {}

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
}
