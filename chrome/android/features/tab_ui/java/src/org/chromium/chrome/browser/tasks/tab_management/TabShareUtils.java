// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;

/** Static utilities for interacting with shared tab groups. */
@NullMarked
public class TabShareUtils {

    /**
     * Tries to get the collaboration id from a tab's group. If anything is null or the tab doesn't
     * have a collaboration id, then null is returned.
     *
     * @param tabId The id of the tab.
     * @param tabModel The tab model to look up the tab in.
     * @param tabGroupSyncService The sync service with tab group data.
     * @return The collaboration id.
     */
    public static @Nullable String getCollaborationIdOrNull(
            int tabId,
            @Nullable TabModel tabModel,
            @Nullable TabGroupSyncService tabGroupSyncService) {
        if (tabModel == null || tabGroupSyncService == null) return null;

        @Nullable Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return null;

        Token localGroupId = tab.getTabGroupId();
        if (localGroupId == null) return null;

        return getCollaborationIdOrNull(localGroupId, tabGroupSyncService);
    }

    /**
     * Tries to get the collaboration id from a tab's group.
     *
     * @param tabGroupId The id of the tab group.
     * @param tabGroupSyncService The sync service with tab group data.
     * @return The collaboration id or null.
     */
    public static @Nullable String getCollaborationIdOrNull(
            @Nullable Token tabGroupId, @Nullable TabGroupSyncService tabGroupSyncService) {
        if (tabGroupId == null || tabGroupSyncService == null) return null;

        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tabGroupId);
        SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(localTabGroupId);
        return savedTabGroup == null ? null : savedTabGroup.collaborationId;
    }

    /**
     * Determines whether the collaboration id is valid by checking that it is non null and not
     * empty.
     *
     * @param collaborationId The collaboration id for the tab group in question.
     * @return Whether the provided collaboration id is valid or not.
     */
    @Contract("null -> false")
    public static boolean isCollaborationIdValid(@Nullable String collaborationId) {
        return !TextUtils.isEmpty(collaborationId);
    }

    /**
     * @param groupData The shared group data.
     * @return The state of the group.
     */
    public static @GroupSharedState int discernSharedGroupState(@Nullable GroupData groupData) {
        if (groupData == null) {
            return GroupSharedState.NOT_SHARED;
        } else {
            if (groupData.members == null || groupData.members.size() <= 1) {
                return GroupSharedState.COLLABORATION_ONLY;
            } else {
                return GroupSharedState.HAS_OTHER_USERS;
            }
        }
    }

    /**
     * @param groupData The shared group data.
     * @return The members of the group or null
     */
    public static @Nullable List<GroupMember> getGroupMembers(@Nullable GroupData groupData) {
        if (groupData == null) {
            return null;
        } else {
            @Nullable List<GroupMember> members = groupData.members;
            if (members == null) return null;
            return members.isEmpty() ? null : members;
        }
    }
}
