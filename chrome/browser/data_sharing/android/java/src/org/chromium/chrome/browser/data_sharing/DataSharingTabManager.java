// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseURLStatus;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for handling communication from the UI to multiple data sharing
 * services.
 */
public class DataSharingTabManager {
    private ObservableSupplier<Profile> mProfileSupplier;
    private DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    private List<DataSharingTabObserver> mTabGroupObserversList;
    private Callback<Profile> mProfileObserver;

    /**
     * Constructor for a new {@link DataSharingTabManager} object.
     *
     * @param tabSwitcherDelegate The delegate used to communicate with the tab switcher.
     * @param profileSupplier The supplier of the currently applicable profile.
     */
    public DataSharingTabManager(
            DataSharingTabSwitcherDelegate tabSwitcherDelegate,
            ObservableSupplier<Profile> profileSupplier) {
        mDataSharingTabSwitcherDelegate = tabSwitcherDelegate;
        mTabGroupObserversList = new ArrayList<DataSharingTabObserver>();

        mProfileSupplier = profileSupplier;
        assert mProfileSupplier != null;
    }

    /**
     * Initiate the join flow. If successful, the associated tab group view will be opened.
     *
     * @param dataSharingURL The URL associated with the join invitation.
     */
    public void initiateJoinFlow(GURL dataSharingURL) {
        if (mProfileSupplier.get() != null) {
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
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(mProfileSupplier.get());
        DataSharingService dataSharingService =
                DataSharingServiceFactory.getForProfile(mProfileSupplier.get());
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
                TabGroupSyncServiceFactory.getForProfile(mProfileSupplier.get());
        mTabGroupObserversList.remove(observer);

        if (tabGroupSyncService != null) {
            tabGroupSyncService.removeObserver(observer);
        }
    }
}
