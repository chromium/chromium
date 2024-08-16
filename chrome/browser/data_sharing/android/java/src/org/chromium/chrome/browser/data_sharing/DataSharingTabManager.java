// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
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
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for handling communication from the UI to multiple data sharing
 * services.
 */
public class DataSharingTabManager {
    private static final String TAG = "DataSharing";

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private List<DataSharingTabObserver> mTabGroupObserversList;
    private Callback<Profile> mProfileObserver;

    /**
     * Constructor for a new {@link DataSharingTabManager} object.
     *
     * @param tabSwitcherDelegate The delegate used to communicate with the tab switcher.
     * @param profileSupplier The supplier of the currently applicable profile.
     * @param bottomSheetControllerSupplier The supplier of bottom sheet state controller.
     * @param shareDelegateSupplier The supplier of dhare delegate.
     * @param windowAndroid The window base class that has the minimum functionality.
     */
    public DataSharingTabManager(
            DataSharingTabSwitcherDelegate tabSwitcherDelegate,
            ObservableSupplier<Profile> profileSupplier,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid) {
        mDataSharingTabSwitcherDelegate = tabSwitcherDelegate;
        mProfileSupplier = profileSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mTabGroupObserversList = new ArrayList<DataSharingTabObserver>();
        assert mProfileSupplier != null;
        assert mBottomSheetControllerSupplier != null;
        assert mShareDelegateSupplier != null;
    }

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param dataSharingURL The URL associated with the join invitation.
     */
    public void initiateJoinFlow(GURL dataSharingURL) {
        if (mProfileSupplier.get().getOriginalProfile() != null) {
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
            // TODO(b/354003616): Show error dialog.
            return;
        }

        GroupToken groupToken = parseResult.groupToken;
        // Verify that tabgroup does not already exist.
        SavedTabGroup existingGroup =
                getTabGroupForCollabId(groupToken.groupId, tabGroupSyncService);
        if (existingGroup != null) {
            Integer tabId = existingGroup.savedTabs.get(0).localId;
            assert tabId != null;
            openTabGroupWithTabId(tabId);
            return;
        }

        // TODO(b/354003616): Show loading dialog while waiting for tab.

        DataSharingTabObserver observer = new DataSharingTabObserver(groupToken.groupId, this);

        mTabGroupObserversList.add(observer);
        tabGroupSyncService.addObserver(observer);

        dataSharingService.addMember(
                groupToken.groupId,
                groupToken.accessToken,
                result -> {
                    if (result != PeopleGroupActionOutcome.SUCCESS) {
                        // TODO(b/354003616): Stop showing loading dialog. Show error dialog.
                        return;
                    }
                });
    }

    SavedTabGroup getTabGroupForCollabId(
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
     * Open a tab group.
     *
     * @param tabId The tab id of the first tab in the group.
     */
    void openTabGroupWithTabId(Integer tabId) {
        // TODO(b/354003616): Verify that the loading dialog is gone.
        mDataSharingTabSwitcherDelegate.openTabGroupWithTabId(tabId);
    }

    /**
     * Stop observing a data sharing tab group.
     *
     * @param observer The observer to be removed.
     */
    public void deleteObserver(DataSharingTabObserver observer) {
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(
                        mProfileSupplier.get().getOriginalProfile());
        mTabGroupObserversList.remove(observer);

        if (tabGroupSyncService != null) {
            tabGroupSyncService.removeObserver(observer);
        }
    }

    public void createGroupFlow(
            Activity activity,
            String tabGroupDisplayName,
            LocalTabGroupId localTabGroupId,
            Callback<Boolean> createGroupFinishedCallback) {
        Profile profile = mProfileSupplier.get().getOriginalProfile();
        assert profile != null;
        TabGroupSyncService tabGroupService = TabGroupSyncServiceFactory.getForProfile(profile);
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);

        SavedTabGroup existingGroup = tabGroupService.getGroup(localTabGroupId);
        if (existingGroup != null && existingGroup.collaborationId != null) {
            dataSharingService.ensureGroupVisibility(
                    existingGroup.collaborationId,
                    (result) -> {
                        if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN) {
                            // TODO(ritikagup): Show error dialog telling failed to create access
                            // token.
                        }
                        showShareSheet(result.groupData);
                    });
            return;
        }

        ViewGroup bottomSheetView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.data_sharing_bottom_sheet, null);
        TabGridDialogShareBottomSheetContent bottomSheetContent =
                new TabGridDialogShareBottomSheetContent(bottomSheetView);
        mBottomSheetControllerSupplier.get().requestShowContent(bottomSheetContent, true);
        mBottomSheetControllerSupplier
                .get()
                .addObserver(
                        new EmptyBottomSheetObserver() {
                            @Override
                            public void onSheetClosed(@StateChangeReason int reason) {
                                mBottomSheetControllerSupplier.get().removeObserver(this);
                                if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                                    createGroupFinishedCallback.onResult(false);
                                }
                            }
                        });

        Callback<DataSharingService.GroupDataOrFailureOutcome> createGroupCallback =
                (result) -> {
                    // TODO: SDK delegeta should run results on UI thread.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN
                                        || result.groupData == null) {
                                    Log.e(TAG, "Group creation failed " + result.actionFailure);
                                    createGroupFinishedCallback.onResult(false);
                                } else {
                                    tabGroupService.makeTabGroupShared(
                                            localTabGroupId, result.groupData.groupToken.groupId);
                                    createGroupFinishedCallback.onResult(true);

                                    showShareSheet(result.groupData);
                                }
                                mBottomSheetControllerSupplier
                                        .get()
                                        .hideContent(
                                                bottomSheetContent,
                                                /* animate= */ false,
                                                StateChangeReason.INTERACTION_COMPLETE);
                            });
                };

        Callback<List<String>> pickerCallback =
                (emails) -> {
                    dataSharingService.createGroup(tabGroupDisplayName, createGroupCallback);
                };
        DataSharingUIDelegate uiDelegate =
                DataSharingServiceFactory.getUIDelegate(
                        mProfileSupplier.get().getOriginalProfile());
        uiDelegate.showMemberPicker(
                activity,
                bottomSheetView,
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
            return;
        }
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
}
