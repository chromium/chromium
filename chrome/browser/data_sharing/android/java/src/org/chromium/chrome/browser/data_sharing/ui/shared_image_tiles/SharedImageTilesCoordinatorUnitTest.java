// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit test for {@link SharedImageTilesCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedImageTilesCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String COLLABORATION_ID = "collaboration_id";
    private static final String EMAIL = "test@test.com";

    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private Bitmap mAvatarBitmap;

    private Activity mActivity;
    private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private SharedImageTilesView mView;
    private TextView mCountTileView;
    private ImageView mManageIcon;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        SharedImageTilesConfig config = new SharedImageTilesConfig.Builder(mActivity).build();
        initialize(config);
    }

    private void initialize(SharedImageTilesConfig config) {
        mSharedImageTilesCoordinator =
                new SharedImageTilesCoordinator(
                        mActivity, config, mDataSharingService, mCollaborationService);
        mView = mSharedImageTilesCoordinator.getView();
        mCountTileView = mView.findViewById(R.id.tiles_count);
        mManageIcon = mView.findViewById(R.id.shared_image_tiles_manage);
        doReturn(mDataSharingUiDelegate).when(mDataSharingService).getUiDelegate();
    }

    private void verifyViews(int countVisibility, int iconViewCount, int manageVisibility) {
        assertEquals(mCountTileView.getVisibility(), countVisibility);
        assertEquals(mManageIcon.getVisibility(), manageVisibility);
        assertEquals(mSharedImageTilesCoordinator.getAllIconViews().size(), iconViewCount);
    }

    private void simulateReadGroupWith2ValidMembers() {
        GroupMember memberValid1 =
                new GroupMember(
                        /* gaiaId= */ null,
                        /* displayName= */ null,
                        EMAIL,
                        /* role= */ 0,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember memberValid2 =
                new GroupMember(
                        /* gaiaId= */ null,
                        /* displayName= */ null,
                        EMAIL,
                        /* role= */ 0,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember memberInvalid1 =
                new GroupMember(
                        /* gaiaId= */ null,
                        /* displayName= */ null,
                        /* email= */ null,
                        /* role= */ 0,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember memberInvalid2 =
                new GroupMember(
                        /* gaiaId= */ null,
                        /* displayName= */ null,
                        /* email= */ "",
                        /* role= */ 0,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupData groupData =
                new GroupData(
                        /* groupId= */ null,
                        /* displayName= */ null,
                        new GroupMember[] {
                            memberValid1, memberValid2, memberInvalid1, memberInvalid2
                        },
                        /* accessToken= */ null);

        doReturn(groupData).when(mCollaborationService).getGroupData(eq(COLLABORATION_ID));
    }

    @Test
    public void testInitialize() {
        assertNotNull(mSharedImageTilesCoordinator.getView());
    }

    @Test
    public void testDefaultTheme() {
        // Default theme should have the following view logic:
        // 0 tile count: None
        // 1 tile count: Tile
        // 2 tile count: Tile Tile
        // 3 tile count: Tile Tile Tile
        // 4 tile count: Tile Tile +2
        // etc
        verifyViews(View.GONE, /* iconViewCount= */ 0, View.GONE);

        mSharedImageTilesCoordinator.updateMembersCount(1);
        verifyViews(View.GONE, /* iconViewCount= */ 1, View.VISIBLE);

        mSharedImageTilesCoordinator.updateMembersCount(2);
        verifyViews(View.GONE, /* iconViewCount= */ 2, View.GONE);

        mSharedImageTilesCoordinator.updateMembersCount(3);
        verifyViews(View.GONE, /* iconViewCount= */ 3, View.GONE);

        mSharedImageTilesCoordinator.updateMembersCount(4);
        verifyViews(View.VISIBLE, /* iconViewCount= */ 2, View.GONE);
    }

    @Test
    public void testFetchPeopleIcon() {
        simulateReadGroupWith2ValidMembers();
        Callback<Boolean> mockFinishedCallback = mock(Callback.class);
        mSharedImageTilesCoordinator.fetchImagesForCollaborationId(
                COLLABORATION_ID, mockFinishedCallback);

        ArgumentCaptor<DataSharingAvatarBitmapConfig> configCaptor =
                ArgumentCaptor.forClass(DataSharingAvatarBitmapConfig.class);

        verify(mDataSharingUiDelegate, times(2)).getAvatarBitmap(configCaptor.capture());

        // Finished callback is not triggered if we are waiting for more bitmaps.
        configCaptor
                .getAllValues()
                .get(0)
                .getDataSharingAvatarCallback()
                .onAvatarLoaded(mAvatarBitmap);

        verify(mockFinishedCallback, never()).onResult(anyBoolean());

        // Finished callback should only be called when all bitmaps returns.
        configCaptor
                .getAllValues()
                .get(1)
                .getDataSharingAvatarCallback()
                .onAvatarLoaded(mAvatarBitmap);

        verify(mockFinishedCallback).onResult(true);
    }

    @Test
    public void testFetchPeopleIconFailure() {
        simulateReadGroupWith2ValidMembers();

        Callback<Boolean> mockFinishedCallback = mock(Callback.class);
        mSharedImageTilesCoordinator.fetchImagesForCollaborationId(
                COLLABORATION_ID, mockFinishedCallback);

        verify(mockFinishedCallback, never()).onResult(anyBoolean());

        // A new update would fail the previous ongoing update.
        Callback<Boolean> mockFinishedCallback2 = mock(Callback.class);
        mSharedImageTilesCoordinator.fetchImagesForCollaborationId(
                COLLABORATION_ID, mockFinishedCallback2);

        verify(mockFinishedCallback).onResult(false);
    }

    @Test
    public void testUpdateGroupMembers() {
        GroupMember memberValid1 =
                new GroupMember(
                        /* gaiaId= */ null,
                        /* displayName= */ null,
                        EMAIL,
                        /* role= */ 0,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember memberValid2 =
                new GroupMember(
                        /* gaiaId= */ null,
                        /* displayName= */ null,
                        EMAIL,
                        /* role= */ 0,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        ArgumentCaptor<DataSharingAvatarBitmapConfig> configCaptor =
                ArgumentCaptor.forClass(DataSharingAvatarBitmapConfig.class);

        // Two members.
        int count = 2;
        mSharedImageTilesCoordinator.onGroupMembersChanged(
                COLLABORATION_ID, List.of(memberValid1, memberValid2));
        verify(mDataSharingUiDelegate, times(count)).getAvatarBitmap(configCaptor.capture());
        verifyViews(View.GONE, /* iconViewCount= */ 2, View.GONE);

        // No members.
        mSharedImageTilesCoordinator.onGroupMembersChanged(COLLABORATION_ID, /* members= */ null);
        verify(mDataSharingUiDelegate, times(count)).getAvatarBitmap(configCaptor.capture());
        verifyViews(View.GONE, /* iconViewCount= */ 0, View.GONE);

        // Two members.
        count += 2;
        mSharedImageTilesCoordinator.onGroupMembersChanged(
                COLLABORATION_ID, List.of(memberValid1, memberValid2));
        verify(mDataSharingUiDelegate, times(count)).getAvatarBitmap(configCaptor.capture());
        verifyViews(View.GONE, /* iconViewCount= */ 2, View.GONE);

        // No group.
        mSharedImageTilesCoordinator.onGroupMembersChanged(
                /* collaborationId= */ null, /* members= */ null);
        verify(mDataSharingUiDelegate, times(count)).getAvatarBitmap(configCaptor.capture());
        verifyViews(View.GONE, /* iconViewCount= */ 0, View.GONE);

        // 1 member + manage icon.
        count += 1;
        mSharedImageTilesCoordinator.onGroupMembersChanged(COLLABORATION_ID, List.of(memberValid1));
        verify(mDataSharingUiDelegate, times(count)).getAvatarBitmap(configCaptor.capture());
        verifyViews(View.GONE, /* iconViewCount= */ 1, View.VISIBLE);
    }
}
