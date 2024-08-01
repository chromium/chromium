// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;

import java.lang.ref.WeakReference;

/** This class is responsible for observing tab activities for data sharing services. */
class DataSharingTabObserver implements TabGroupSyncService.Observer {
    private String mDataSharingGroupId;
    private WeakReference<DataSharingTabManager> mDataSharingTabManager;

    public DataSharingTabObserver(
            String dataSharingGroupId, DataSharingTabManager dataSharingTabManager) {
        mDataSharingGroupId = dataSharingGroupId;
        mDataSharingTabManager = new WeakReference<DataSharingTabManager>(dataSharingTabManager);
    }

    @Override
    public void onInitialized() {}

    @Override
    public void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {
        if (mDataSharingGroupId.equals(group.collaborationId)) {
            DataSharingTabManager dataSharingTabManager = mDataSharingTabManager.get();

            if (dataSharingTabManager != null) {
                Integer tabId = group.savedTabs.get(0).localId;
                assert tabId != null;
                dataSharingTabManager.openTabGroupWithTabId(tabId);
                dataSharingTabManager.deleteObserver(this);
            }
        }
    }

    @Override
    public void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {}

    @Override
    public void onTabGroupRemoved(LocalTabGroupId localId, @TriggerSource int source) {}

    @Override
    public void onTabGroupRemoved(String syncId, @TriggerSource int source) {}
}
