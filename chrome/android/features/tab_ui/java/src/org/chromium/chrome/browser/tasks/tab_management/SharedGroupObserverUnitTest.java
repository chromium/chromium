// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import androidx.annotation.Nullable;

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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;

import java.util.List;

/** Unit tests for {@link SharedGroupObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedGroupObserverUnitTest {
    private static final Token TAB_GROUP_ID = Token.createRandom();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private Callback<Integer> mOnSharedGroupStateChanged;

    @Captor private ArgumentCaptor<TabGroupSyncService.Observer> mSyncObserverCaptor;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;

    @Test
    public void testDestroy() {
        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);
        verify(mDataSharingService).addObserver(any());

        observer.destroy();
        verify(mDataSharingService).removeObserver(any());
    }

    @Test
    public void testGet_nullGroup() {
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);

        assertNull(observer.getGroupMembersSupplier().get());

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertNull(collaborationId);
    }

    @Test
    public void testGet_noCollaborationId() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);

        assertNull(observer.getGroupMembersSupplier().get());

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertNull(collaborationId);
    }

    @Test
    public void testGet_emptyGetGroupData() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        when(mCollaborationService.getGroupData(COLLABORATION_ID1)).thenReturn(null);

        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);

        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);

        assertNull(observer.getGroupMembersSupplier().get());

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);
    }

    @Test
    public void testGet_collaborationOnly() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        when(mCollaborationService.getGroupData(COLLABORATION_ID1))
                .thenReturn(SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1));

        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);

        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.COLLABORATION_ONLY, state);

        assertEquals(List.of(GROUP_MEMBER1), observer.getGroupMembersSupplier().get());

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);
    }

    @Test
    public void testGet_hasOtherUsers() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        when(mCollaborationService.getGroupData(COLLABORATION_ID1))
                .thenReturn(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2));

        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);

        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.HAS_OTHER_USERS, state);

        assertEquals(
                List.of(GROUP_MEMBER1, GROUP_MEMBER2), observer.getGroupMembersSupplier().get());

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);
    }

    @Test
    public void testOnShareChanged() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);

        observer.getGroupSharedStateSupplier().addObserver(mOnSharedGroupStateChanged);
        ShadowLooper.runUiThreadTasks();
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.NOT_SHARED);
        assertNull(observer.getGroupMembersSupplier().get());
        Mockito.clearInvocations(mOnSharedGroupStateChanged);

        savedTabGroup.collaborationId = COLLABORATION_ID1;
        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        mSharingObserverCaptor
                .getValue()
                .onGroupAdded(SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1));
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.COLLABORATION_ONLY);
        assertEquals(List.of(GROUP_MEMBER1), observer.getGroupMembersSupplier().get());

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);

        mSharingObserverCaptor
                .getValue()
                .onGroupChanged(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2));
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.HAS_OTHER_USERS);
        assertEquals(
                List.of(GROUP_MEMBER1, GROUP_MEMBER2), observer.getGroupMembersSupplier().get());

        savedTabGroup.collaborationId = COLLABORATION_ID1;
        mSharingObserverCaptor.getValue().onGroupRemoved(COLLABORATION_ID1);
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.NOT_SHARED);
        assertNull(observer.getGroupMembersSupplier().get());

        collaborationId = observer.getCollaborationIdSupplier().get();
        assertNull(collaborationId);
    }

    @Test
    public void testOnSyncChanged() {
        SavedTabGroup syncGroup = new SavedTabGroup();
        syncGroup.localId = new LocalTabGroupId(TAB_GROUP_ID);
        syncGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(syncGroup);

        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);
        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        verify(mTabGroupSyncService).addObserver(mSyncObserverCaptor.capture());
        ObservableSupplier<Integer> sharedStateSupplier = observer.getGroupSharedStateSupplier();
        assertEquals(GroupSharedState.NOT_SHARED, sharedStateSupplier.get().intValue());

        // savedTabGroup.collaborationId is still null, cannot match up the groups yet.
        GroupData shareGroup =
                SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        when(mCollaborationService.getGroupData(COLLABORATION_ID1)).thenReturn(shareGroup);
        mSharingObserverCaptor.getValue().onGroupAdded(shareGroup);
        assertEquals(GroupSharedState.NOT_SHARED, sharedStateSupplier.get().intValue());

        syncGroup.collaborationId = COLLABORATION_ID1;
        mSyncObserverCaptor.getValue().onTabGroupUpdated(syncGroup, TriggerSource.LOCAL);
        assertEquals(GroupSharedState.HAS_OTHER_USERS, sharedStateSupplier.get().intValue());
    }

    @Test
    public void testNoCollaborationOnInitUntilLocalIdEvent() {
        SavedTabGroup syncGroup = new SavedTabGroup();
        syncGroup.localId = new LocalTabGroupId(TAB_GROUP_ID);
        syncGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(syncGroup);

        SharedGroupObserver observer =
                new SharedGroupObserver(
                        TAB_GROUP_ID,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);
        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        verify(mTabGroupSyncService).addObserver(mSyncObserverCaptor.capture());
        ObservableSupplier<Integer> sharedStateSupplier = observer.getGroupSharedStateSupplier();
        assertEquals(GroupSharedState.NOT_SHARED, sharedStateSupplier.get().intValue());

        GroupData shareGroup =
                SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        when(mCollaborationService.getGroupData(COLLABORATION_ID1)).thenReturn(shareGroup);

        syncGroup.collaborationId = COLLABORATION_ID1;
        mSyncObserverCaptor.getValue().onTabGroupLocalIdChanged("sync_id", syncGroup.localId);
        assertEquals(GroupSharedState.HAS_OTHER_USERS, sharedStateSupplier.get().intValue());
    }
}
