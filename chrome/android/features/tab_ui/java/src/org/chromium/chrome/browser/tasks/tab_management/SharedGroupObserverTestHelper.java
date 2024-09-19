// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import org.mockito.ArgumentCaptor;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Test helpers for {@link SharedGroupObserver} tests. */
public class SharedGroupObserverTestHelper {
    public static final String EMAIL1 = "one@gmail.com";
    public static final String EMAIL2 = "two@gmail.com";
    public static final String GAIA_ID1 = "gaiaId1";
    public static final String GAIA_ID2 = "gaiaId2";
    public static final GroupMember GROUP_MEMBER1 =
            newGroupMember(GAIA_ID1, EMAIL1, MemberRole.OWNER);
    public static final GroupMember GROUP_MEMBER2 =
            newGroupMember(GAIA_ID2, EMAIL2, MemberRole.MEMBER);

    private final DataSharingService mDataSharingService;
    private final TabGroupSyncService mTabGroupSyncService;
    private final ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;

    /**
     * @param mockDataSharingService A mock {@link DataSharingService}.
     * @param mockTabGroupSyncService A mock {@link TabGroupSyncService}.
     */
    public SharedGroupObserverTestHelper(
            DataSharingService mockDataSharingService,
            TabGroupSyncService mockTabGroupSyncService,
            ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> readGroupCallbackCaptor) {
        mDataSharingService = mockDataSharingService;
        mTabGroupSyncService = mockTabGroupSyncService;
        mReadGroupCallbackCaptor = readGroupCallbackCaptor;
    }

    /** Creates a new group member. */
    private static GroupMember newGroupMember(
            String gaiaId, String email, @MemberRole int memberRole) {
        return new GroupMember(
                gaiaId, /* displayName= */ null, email, memberRole, /* avatarUrl= */ null);
    }

    /** Creates new group data. */
    public static GroupData newGroupData(String collaborationId, GroupMember... members) {
        return new GroupData(
                collaborationId, /* displayName= */ null, members, /* groupToken= */ null);
    }

    /** Responds to a readGroup call on the {@link DataSharingService}. */
    public void respondToReadGroup(String collaborationId, GroupMember... members) {
        verify(mDataSharingService, atLeastOnce())
                .readGroup(eq(collaborationId), mReadGroupCallbackCaptor.capture());
        GroupData groupData = newGroupData(collaborationId, members);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);
    }
}
