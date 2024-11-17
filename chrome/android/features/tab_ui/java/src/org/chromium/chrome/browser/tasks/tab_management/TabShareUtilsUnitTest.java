// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
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

/** Unit tests for {@link TabShareUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabShareUtilsUnitTest {
    private static final int TAB_ID = 34789;
    private static final Token TAB_GROUP_ID = new Token(87493L, 3489L);
    private static final String GAIA_ID = "asdf";
    private static final String GROUP_ID = "group";
    private static final String DISPLAY_NAME = "display_name";
    private static final String ACCESS_TOKEN = "token";
    private static final String EMAIL = "foo@bar.com";
    private static final String COLLABORATION_ID = "my-collaboration";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Tab mTab;
    @Mock TabModel mTabModel;
    @Mock TabGroupSyncService mTabGroupSyncService;
    @Mock IdentityManager mIdentityManager;
    @Mock CoreAccountInfo mCoreAccountInfo;

    private LocalTabGroupId mLocalTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
    private SavedTabGroup mSavedTabGroup;
    private GroupDataOrFailureOutcome mGroupDataOutcome;

    @Before
    public void setUp() {
        GroupMember member =
                new GroupMember(
                        GAIA_ID,
                        DISPLAY_NAME,
                        EMAIL,
                        MemberRole.MEMBER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupData groupData =
                new GroupData(GROUP_ID, DISPLAY_NAME, new GroupMember[] {member}, ACCESS_TOKEN);
        mGroupDataOutcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);

        mSavedTabGroup = new SavedTabGroup();
        mSavedTabGroup.collaborationId = COLLABORATION_ID;

        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        when(mTabGroupSyncService.getGroup(mLocalTabGroupId)).thenReturn(mSavedTabGroup);

        when(mCoreAccountInfo.getGaiaId()).thenReturn(GAIA_ID);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);
    }

    @Test
    public void testGetCollaborationIdOrNull_Failures() {
        assertNull(
                TabShareUtils.getCollaborationIdOrNull(
                        TAB_ID, /* tabModel= */ null, mTabGroupSyncService));
        assertNull(
                TabShareUtils.getCollaborationIdOrNull(
                        TAB_ID, mTabModel, /* tabGroupSyncService= */ null));
        assertNull(
                TabShareUtils.getCollaborationIdOrNull(
                        Tab.INVALID_TAB_ID, mTabModel, mTabGroupSyncService));

        when(mTab.getTabGroupId()).thenReturn(null);
        assertNull(TabShareUtils.getCollaborationIdOrNull(TAB_ID, mTabModel, mTabGroupSyncService));
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        when(mTabGroupSyncService.getGroup(mLocalTabGroupId)).thenReturn(null);
        assertNull(TabShareUtils.getCollaborationIdOrNull(TAB_ID, mTabModel, mTabGroupSyncService));
        when(mTabGroupSyncService.getGroup(mLocalTabGroupId)).thenReturn(mSavedTabGroup);
    }

    @Test
    public void testGetCollaborationIdOrNull_Success() {
        String collaborationId =
                TabShareUtils.getCollaborationIdOrNull(TAB_ID, mTabModel, mTabGroupSyncService);
        assertTrue(TabShareUtils.isCollaborationIdValid(collaborationId));
    }

    @Test
    public void testIsCollaborationIdValid() {
        assertFalse(TabShareUtils.isCollaborationIdValid(null));
        assertFalse(TabShareUtils.isCollaborationIdValid(""));
        assertTrue(TabShareUtils.isCollaborationIdValid("valid-id"));
    }

    @Test
    public void testGetSelfMemberRole_Unknown() {
        assertEquals(
                MemberRole.UNKNOWN,
                TabShareUtils.getSelfMemberRole(/* outcome= */ null, mIdentityManager));
        assertEquals(
                MemberRole.UNKNOWN,
                TabShareUtils.getSelfMemberRole(
                        mGroupDataOutcome, /* identityManager= */ (IdentityManager) null));

        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        assertEquals(
                MemberRole.UNKNOWN,
                TabShareUtils.getSelfMemberRole(mGroupDataOutcome, mIdentityManager));
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);

        GroupDataOrFailureOutcome datalessGroupDataOutcome =
                new GroupDataOrFailureOutcome(
                        /* groupData= */ null, PeopleGroupActionFailure.UNKNOWN);
        assertEquals(
                MemberRole.UNKNOWN,
                TabShareUtils.getSelfMemberRole(datalessGroupDataOutcome, mIdentityManager));

        GroupData memberlessGroupData =
                new GroupData(GROUP_ID, DISPLAY_NAME, /* members= */ null, ACCESS_TOKEN);
        GroupDataOrFailureOutcome memberlessGroupDataOutcome =
                new GroupDataOrFailureOutcome(
                        memberlessGroupData, PeopleGroupActionFailure.UNKNOWN);
        assertEquals(
                MemberRole.UNKNOWN,
                TabShareUtils.getSelfMemberRole(memberlessGroupDataOutcome, mIdentityManager));

        GroupData emptyMemberGroupData =
                new GroupData(GROUP_ID, DISPLAY_NAME, new GroupMember[] {}, ACCESS_TOKEN);
        GroupDataOrFailureOutcome emptyMemberGroupDataOutcome =
                new GroupDataOrFailureOutcome(
                        emptyMemberGroupData, PeopleGroupActionFailure.UNKNOWN);
        assertEquals(
                MemberRole.UNKNOWN,
                TabShareUtils.getSelfMemberRole(emptyMemberGroupDataOutcome, mIdentityManager));
    }

    @Test
    public void testGetSelfMemberRole() {
        assertEquals(
                MemberRole.MEMBER,
                TabShareUtils.getSelfMemberRole(mGroupDataOutcome, mIdentityManager));
    }
}
