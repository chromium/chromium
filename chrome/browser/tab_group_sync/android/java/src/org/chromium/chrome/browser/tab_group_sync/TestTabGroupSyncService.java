// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import androidx.annotation.NonNull;

import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Test implementation of {@link TabGroupSyncService} that can be used for unit tests. */
class TestTabGroupSyncService implements TabGroupSyncService {
    public static final String SYNC_ID_1 = "SYNC_ID_1";

    private List<SavedTabGroup> mTabGroups = new ArrayList<>();

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}

    @Override
    public String createGroup(int groupId) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_ID_1;
        savedTabGroup.localId = groupId;
        mTabGroups.add(savedTabGroup);
        return savedTabGroup.syncId;
    }

    @Override
    public void removeGroup(int groupId) {}

    @Override
    public void updateVisualData(int tabGroupId, @NonNull String title, int color) {}

    @Override
    public void addTab(int tabGroupId, int tabId, String title, GURL url, int position) {}

    @Override
    public void updateTab(int tabGroupId, int tabId, String title, GURL url, int position) {}

    @Override
    public void removeTab(int tabGroupId, int tabId) {}

    @Override
    public String[] getAllGroupIds() {
        return new String[0];
    }

    @Override
    public SavedTabGroup getGroup(String syncGroupId) {
        return mTabGroups.isEmpty() ? null : mTabGroups.get(0);
    }

    @Override
    public SavedTabGroup getGroup(int localGroupId) {
        return mTabGroups.isEmpty() ? null : mTabGroups.get(0);
    }

    @Override
    public void updateLocalTabGroupId(String syncId, int localId) {}

    @Override
    public void updateLocalTabId(int localGroupId, String syncTabId, int localTabId) {}
}
