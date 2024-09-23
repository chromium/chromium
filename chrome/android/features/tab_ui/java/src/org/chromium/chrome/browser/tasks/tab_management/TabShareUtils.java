// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Objects;

/** Static utilities for interacting with shared tab groups. */
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

        LocalTabGroupId localTabGroupId = new LocalTabGroupId(localGroupId);
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
    public static boolean isCollaborationIdValid(String collaborationId) {
        return !TextUtils.isEmpty(collaborationId);
    }

    /**
     * @param outcome The result of a group read.
     * @return The state of the group.
     */
    public static @GroupSharedState int discernSharedGroupState(
            @Nullable GroupDataOrFailureOutcome outcome) {
        if (outcome == null || outcome.actionFailure != PeopleGroupActionFailure.UNKNOWN) {
            return GroupSharedState.NOT_SHARED;
        } else {
            return discernSharedGroupState(outcome.groupData);
        }
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
     * Tries to figure out if the signed in user account has a role in a given group, and if so,
     * which role they have.
     *
     * @param outcome The result of a readGroup call to the sharing service.
     * @param identityManager Used to fetch account information.
     * @return The role the currently signed in account has in the group.
     */
    public static @MemberRole int getSelfMemberRole(
            @Nullable GroupDataOrFailureOutcome outcome,
            @Nullable IdentityManager identityManager) {
        if (outcome == null || identityManager == null) return MemberRole.UNKNOWN;

        @Nullable
        CoreAccountInfo account = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (account == null) return MemberRole.UNKNOWN;

        return getSelfMemberRole(outcome.groupData, account.getGaiaId());
    }

    /**
     * Same as {@link #getSelfMemberRole(GroupDataOrFailureOutcome, IdentityManager)} but with a
     * supplied gaiaId.
     */
    public static @MemberRole int getSelfMemberRole(@Nullable GroupData groupData, String gaiaId) {
        if (groupData == null || groupData.members == null) {
            return MemberRole.UNKNOWN;
        }

        for (GroupMember member : groupData.members) {
            if (Objects.equals(gaiaId, member.gaiaId)) {
                return member.role;
            }
        }

        return MemberRole.UNKNOWN;
    }
}
