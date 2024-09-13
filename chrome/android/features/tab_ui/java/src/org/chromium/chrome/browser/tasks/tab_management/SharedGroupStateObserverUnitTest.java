// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Unit tests for {@link SharedGroupStateObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedGroupStateObserverUnitTest {
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String COLLABORATION_ID1 = "collaborationId1";
    private static final String EMAIL1 = "one@gmail.com";
    private static final String EMAIL2 = "two@gmail.com";
    private static final String GAIA_ID1 = "gaiaId1";
    private static final String GAIA_ID2 = "gaiaId2";
    private static final GroupMember GROUP_MEMBER1 =
            newGroupMember(GAIA_ID1, EMAIL1, MemberRole.OWNER);
    private static final GroupMember GROUP_MEMBER2 =
            newGroupMember(GAIA_ID2, EMAIL2, MemberRole.MEMBER);

    private static GroupMember newGroupMember(
            String gaiaId, String email, @MemberRole int memberRole) {
        return new GroupMember(
                gaiaId, /* displayName= */ null, email, memberRole, /* avatarUrl= */ null);
    }

    private static GroupData newGroupData(GroupMember... members) {
        return new GroupData(
                COLLABORATION_ID1, /* displayName= */ null, members, /* groupToken= */ null);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DataSharingService mDataSharingService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Callback<Integer> mOnSharedGroupStateChanged;

    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;

    private void respondToReadGroup(GroupMember... members) {
        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupData groupData = newGroupData(members);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);
    }

    @Test
    public void testDestroy() {
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);
        verify(mDataSharingService).addObserver(any());

        observer.destroy();
        verify(mDataSharingService).removeObserver(any());
    }

    @Test
    public void testGet_nullGroup() {
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);
    }

    @Test
    public void testGet_noCollaborationId() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);
    }

    @Test
    public void testGet_failedReadGroup() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(
                        /* groupData= */ null, PeopleGroupActionFailure.TRANSIENT_FAILURE);
        verify(mDataSharingService).readGroup(any(), mReadGroupCallbackCaptor.capture());
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);
    }

    @Test
    public void testGet_collaborationOnly() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        respondToReadGroup(GROUP_MEMBER1);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.COLLABORATION_ONLY, state);
    }

    @Test
    public void testGet_hasOtherUsers() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        respondToReadGroup(GROUP_MEMBER1, GROUP_MEMBER2);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.HAS_OTHER_USERS, state);
    }

    @Test
    public void testOnChanged() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupStateObserver observer =
                new SharedGroupStateObserver(
                        TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        observer.getGroupSharedStateSupplier().addObserver(mOnSharedGroupStateChanged);
        ShadowLooper.runUiThreadTasks();
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.NOT_SHARED);
        Mockito.clearInvocations(mOnSharedGroupStateChanged);

        savedTabGroup.collaborationId = COLLABORATION_ID1;
        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        mSharingObserverCaptor.getValue().onGroupAdded(newGroupData(GROUP_MEMBER1));
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.COLLABORATION_ONLY);

        mSharingObserverCaptor
                .getValue()
                .onGroupChanged(newGroupData(GROUP_MEMBER1, GROUP_MEMBER2));
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.HAS_OTHER_USERS);

        savedTabGroup.collaborationId = COLLABORATION_ID1;
        mSharingObserverCaptor.getValue().onGroupRemoved(COLLABORATION_ID1);
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.NOT_SHARED);
    }
}
