// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;

/**
 * Helper class to persist and retrieve sync ID associated with a tab group ID used by {@link
 * TabGroupSyncService}. Uses tab group root ID as the local ID.
 */
class TabGroupSyncIdUtils {
    private static final String TAB_GROUP_SYNC_IDS_FILE_NAME = "tab_group_sync_ids";

    /**
     * Called to get the sync ID associated with a local tab group root ID.
     *
     * @param localId The local ID associated with the group, which is the root ID.
     * @return The sync ID associated with the group.
     */
    @Nullable
    public static String getTabGroupSyncId(int localId) {
        return getSharedPreferences().getString(String.valueOf(localId), null);
    }

    /**
     * Called to persist the sync ID associated with a tab group root ID.
     *
     * @param localId The local ID associated with the group, which is the root ID.
     * @param syncId The sync ID associated with the tab group.
     */
    public static void putTabGroupSyncId(int localId, String syncId) {
        String tabGroupIdKey = String.valueOf(localId);
        getSharedPreferences().edit().putString(tabGroupIdKey, syncId).apply();
    }

    /**
     * This method deletes local to sync ID mapping stored for a given tab group {@code localId}.
     *
     * @param localId The local ID of the group for which the sync ID mapping will be deleted.
     */
    public static void deleteTabGroupSyncId(int localId) {
        String tabGroupIdKey = String.valueOf(localId);
        getSharedPreferences().edit().remove(tabGroupIdKey).apply();
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_SYNC_IDS_FILE_NAME, Context.MODE_PRIVATE);
    }
}
