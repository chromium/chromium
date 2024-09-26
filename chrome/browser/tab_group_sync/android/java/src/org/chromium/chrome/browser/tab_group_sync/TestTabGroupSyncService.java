// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.EventDetails;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.OpeningSource;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Test implementation of {@link TabGroupSyncService} that can be used for unit tests. */
class TestTabGroupSyncService implements TabGroupSyncService {
    public static final String SYNC_ID_1 = "SYNC_ID_1";
    public static final String LOCAL_DEVICE_CACHE_GUID = "LocalDevice";

    private List<SavedTabGroup> mTabGroups = new ArrayList<>();

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}

    @Override
    public String createGroup(LocalTabGroupId groupId) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_ID_1;
        savedTabGroup.localId = groupId;
        mTabGroups.add(savedTabGroup);
        return savedTabGroup.syncId;
    }

    @Override
    public void removeGroup(LocalTabGroupId localTabGroupId) {}

    @Override
    public void removeGroup(String syncTabGroupId) {}

    @Override
    public void updateVisualData(LocalTabGroupId tabGroupId, @NonNull String title, int color) {}

    @Override
    public void addTab(
            LocalTabGroupId tabGroupId, int tabId, String title, GURL url, int position) {}

    @Override
    public void updateTab(
            LocalTabGroupId tabGroupId, int tabId, String title, GURL url, int position) {}

    @Override
    public void removeTab(LocalTabGroupId tabGroupId, int tabId) {}

    @Override
    public void moveTab(LocalTabGroupId tabGroupId, int tabId, int newIndexInGroup) {}

    @Override
    public void onTabSelected(LocalTabGroupId tabGroupId, int tabId) {}

    @Override
    public void makeTabGroupShared(LocalTabGroupId tabGroupId, @NonNull String collaborationId) {}

    @Override
    public String[] getAllGroupIds() {
        return new String[0];
    }

    @Override
    public SavedTabGroup getGroup(String syncGroupId) {
        for (SavedTabGroup group : mTabGroups) {
            if (syncGroupId.equals(group.syncId)) return group;
        }
        return null;
    }

    @Override
    public SavedTabGroup getGroup(LocalTabGroupId localGroupId) {
        for (SavedTabGroup group : mTabGroups) {
            if (localGroupId.equals(group.localId)) return group;
        }
        return null;
    }

    @Override
    public void updateLocalTabGroupMapping(
            String syncId, LocalTabGroupId localId, @OpeningSource int openingSource) {}

    @Override
    public void removeLocalTabGroupMapping(
            LocalTabGroupId localId, @ClosingSource int closingSource) {}

    @Override
    public List<LocalTabGroupId> getDeletedGroupIds() {
        return new ArrayList<>();
    }

    @Override
    public void updateLocalTabId(LocalTabGroupId localGroupId, String syncTabId, int localTabId) {}

    @Override
    public boolean isRemoteDevice(String syncCacheGuid) {
        boolean isLocal =
                TextUtils.isEmpty(syncCacheGuid)
                        || TextUtils.equals(LOCAL_DEVICE_CACHE_GUID, syncCacheGuid);
        return !isLocal;
    }

    @Override
    public void recordTabGroupEvent(EventDetails eventDetails) {}
}
