// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import androidx.annotation.Nullable;

import org.junit.Before;
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
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Unit tests for {@link SharedGroupObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedGroupObserverUnitTest {
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String COLLABORATION_ID1 = "collaborationId1";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DataSharingService mDataSharingService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Callback<Integer> mOnSharedGroupStateChanged;

    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;

    private SharedGroupTestHelper mSharedGroupTestHelper;

    @Before
    public void setUp() {
        mSharedGroupTestHelper =
                new SharedGroupTestHelper(mDataSharingService, mReadGroupCallbackCaptor);
    }

    @Test
    public void testDestroy() {
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);
        verify(mDataSharingService).addObserver(any());

        observer.destroy();
        verify(mDataSharingService).removeObserver(any());
    }

    @Test
    public void testGet_nullGroup() {
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertNull(collaborationId);
    }

    @Test
    public void testGet_noCollaborationId() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertNull(collaborationId);
    }

    @Test
    public void testGet_failedReadGroup() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(
                        /* groupData= */ null, PeopleGroupActionFailure.TRANSIENT_FAILURE);
        verify(mDataSharingService).readGroup(any(), mReadGroupCallbackCaptor.capture());
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.NOT_SHARED, state);

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);
    }

    @Test
    public void testGet_collaborationOnly() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        mSharedGroupTestHelper.respondToReadGroup(COLLABORATION_ID1, GROUP_MEMBER1);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.COLLABORATION_ONLY, state);

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);
    }

    @Test
    public void testGet_hasOtherUsers() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        mSharedGroupTestHelper.respondToReadGroup(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        @GroupSharedState int state = observer.getGroupSharedStateSupplier().get();
        assertEquals(GroupSharedState.HAS_OTHER_USERS, state);

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);
    }

    @Test
    public void testOnChanged() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        SharedGroupObserver observer =
                new SharedGroupObserver(TAB_GROUP_ID, mTabGroupSyncService, mDataSharingService);

        observer.getGroupSharedStateSupplier().addObserver(mOnSharedGroupStateChanged);
        ShadowLooper.runUiThreadTasks();
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.NOT_SHARED);
        Mockito.clearInvocations(mOnSharedGroupStateChanged);

        savedTabGroup.collaborationId = COLLABORATION_ID1;
        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        mSharingObserverCaptor
                .getValue()
                .onGroupAdded(SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1));
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.COLLABORATION_ONLY);

        @Nullable String collaborationId = observer.getCollaborationIdSupplier().get();
        assertEquals(COLLABORATION_ID1, collaborationId);

        mSharingObserverCaptor
                .getValue()
                .onGroupChanged(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2));
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.HAS_OTHER_USERS);

        savedTabGroup.collaborationId = COLLABORATION_ID1;
        mSharingObserverCaptor.getValue().onGroupRemoved(COLLABORATION_ID1);
        verify(mOnSharedGroupStateChanged).onResult(GroupSharedState.NOT_SHARED);

        collaborationId = observer.getCollaborationIdSupplier().get();
        assertNull(collaborationId);
    }
}
