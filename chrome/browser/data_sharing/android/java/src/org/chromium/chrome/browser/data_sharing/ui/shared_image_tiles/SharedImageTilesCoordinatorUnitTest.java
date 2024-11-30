// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;
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
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.ui.base.TestActivity;

/** Unit test for {@link SharedImageTilesCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedImageTilesCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String COLLABORATION_ID = "collaboration_id";
    private static final String EMAIL = "test@test.com";

    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private Bitmap mAvatarBitmap;

    private Activity mActivity;
    private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private SharedImageTilesView mView;
    private TextView mCountTileView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        initialize(
                SharedImageTilesType.DEFAULT,
                new SharedImageTilesColor(SharedImageTilesColor.Style.DEFAULT));
    }

    private void initialize(@SharedImageTilesType int type, SharedImageTilesColor color) {
        mSharedImageTilesCoordinator =
                new SharedImageTilesCoordinator(mActivity, type, color, mDataSharingService);
        mView = mSharedImageTilesCoordinator.getView();
        mCountTileView = mView.findViewById(R.id.tiles_count);
    }

    private void verifyViews(int countVisibility, int iconViewCount) {
        assertEquals(mCountTileView.getVisibility(), countVisibility);
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
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(
                        new GroupData(
                                /* groupId= */ null,
                                /* displayName= */ null,
                                new GroupMember[] {
                                    memberValid1, memberValid2, memberInvalid1, memberInvalid2
                                },
                                /* accessToken= */ null),
                        PeopleGroupActionFailure.UNKNOWN);

        doAnswer(
                        invocation -> {
                            Callback<GroupDataOrFailureOutcome> callback =
                                    invocation.getArgument(1);
                            callback.onResult(outcome);
                            return null;
                        })
                .when(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID), any(Callback.class));
        doReturn(mDataSharingUiDelegate).when(mDataSharingService).getUiDelegate();
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
        verifyViews(View.GONE, /* iconViewCount= */ 0);

        mSharedImageTilesCoordinator.updateMembersCount(1);
        verifyViews(View.GONE, /* iconViewCount= */ 1);

        mSharedImageTilesCoordinator.updateMembersCount(2);
        verifyViews(View.GONE, /* iconViewCount= */ 2);

        mSharedImageTilesCoordinator.updateMembersCount(3);
        verifyViews(View.GONE, /* iconViewCount= */ 3);

        mSharedImageTilesCoordinator.updateMembersCount(4);
        verifyViews(View.VISIBLE, /* iconViewCount= */ 2);
    }

    @Test
    public void testFetchPeopleIcon() {
        simulateReadGroupWith2ValidMembers();
        Callback<Boolean> mockFinishedCallback = mock(Callback.class);
        mSharedImageTilesCoordinator.updateCollaborationId(COLLABORATION_ID, mockFinishedCallback);

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
        mSharedImageTilesCoordinator.updateCollaborationId(COLLABORATION_ID, mockFinishedCallback);

        verify(mockFinishedCallback, never()).onResult(anyBoolean());

        // A new update would fail the previous ongoing update.
        Callback<Boolean> mockFinishedCallback2 = mock(Callback.class);
        mSharedImageTilesCoordinator.updateCollaborationId(COLLABORATION_ID, mockFinishedCallback2);

        verify(mockFinishedCallback).onResult(false);
    }
}
