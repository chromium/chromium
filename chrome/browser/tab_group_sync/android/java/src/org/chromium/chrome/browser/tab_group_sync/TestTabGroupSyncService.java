// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.EventDetails;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.OpeningSource;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Test implementation of {@link TabGroupSyncService} that can be used for unit tests. */
@NullMarked
class TestTabGroupSyncService implements TabGroupSyncService {
    public static final String LOCAL_DEVICE_CACHE_GUID = "LocalDevice";

    private final List<SavedTabGroup> mTabGroups = new ArrayList<>();
    private final VersioningMessageController mVersioningMessageController =
            new TestVersioningMessageController();

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}

    @Override
    public void addGroup(SavedTabGroup savedTabGroup) {
        mTabGroups.add(savedTabGroup);
    }

    @Override
    public void removeGroup(LocalTabGroupId localTabGroupId) {}

    @Override
    public void removeGroup(String syncTabGroupId) {}

    @Override
    public void updateVisualData(LocalTabGroupId tabGroupId, String title, int color) {}

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
    public void onTabSelected(@Nullable LocalTabGroupId tabGroupId, int tabId, String tabTitle) {}

    @Override
    public void makeTabGroupShared(
            LocalTabGroupId tabGroupId,
            String collaborationId,
            @Nullable Callback<Boolean> tabGroupSharingCallback) {}

    @Override
    public void aboutToUnShareTabGroup(
            LocalTabGroupId tabGroupId, @Nullable Callback<Boolean> callback) {}

    @Override
    public void onTabGroupUnShareComplete(LocalTabGroupId tabGroupId, boolean success) {}

    @Override
    public String[] getAllGroupIds() {
        return new String[0];
    }

    @Override
    public @Nullable SavedTabGroup getGroup(String syncGroupId) {
        for (SavedTabGroup group : mTabGroups) {
            if (syncGroupId.equals(group.syncId)) return group;
        }
        return null;
    }

    @Override
    public @Nullable SavedTabGroup getGroup(LocalTabGroupId localGroupId) {
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
    public void setLocalObservationMode(boolean observeLocalChanges) {}

    @Override
    public boolean isObservingLocalChanges() {
        return true;
    }

    @Override
    public boolean isRemoteDevice(@Nullable String syncCacheGuid) {
        boolean isLocal =
                TextUtils.isEmpty(syncCacheGuid)
                        || TextUtils.equals(LOCAL_DEVICE_CACHE_GUID, syncCacheGuid);
        return !isLocal;
    }

    @Override
    public boolean wasTabGroupClosedLocally(String syncTabGroupId) {
        return false;
    }

    @Override
    public void recordTabGroupEvent(EventDetails eventDetails) {}

    @Override
    public void updateArchivalStatus(String syncTabGroupId, boolean archivalStatus) {}

    @Override
    public VersioningMessageController getVersioningMessageController() {
        return mVersioningMessageController;
    }

    @Override
    public void setCollaborationAvailableInFinderForTesting(String collaborationId) {}

    private static class TestVersioningMessageController implements VersioningMessageController {
        @Override
        public boolean isInitialized() {
            return false;
        }

        @Override
        public boolean shouldShowMessageUi(int messageType) {
            return false;
        }

        @Override
        public void shouldShowMessageUiAsync(int messageType, Callback<Boolean> callback) {}

        @Override
        public void onMessageUiShown(int messageType) {}

        @Override
        public void onMessageUiDismissed(int messageType) {}
    }
}
