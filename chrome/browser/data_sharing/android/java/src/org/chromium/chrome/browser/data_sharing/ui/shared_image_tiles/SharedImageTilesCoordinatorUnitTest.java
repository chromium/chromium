// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
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
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;

/** Unit test for {@link SharedImageTilesCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedImageTilesCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String COLLABORATION_ID = "collaboration_id";
    private static final String EMAIL = "test@test.com";

    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;

    private Activity mActivity;
    private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private SharedImageTilesView mView;
    private TextView mCountTileView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        initialize(SharedImageTilesType.DEFAULT, SharedImageTilesColor.DEFAULT);
    }

    private void initialize(@SharedImageTilesType int type, @SharedImageTilesColor int color) {
        mSharedImageTilesCoordinator =
                new SharedImageTilesCoordinator(mActivity, type, color, mDataSharingService);
        mView = mSharedImageTilesCoordinator.getView();
        mCountTileView = mView.findViewById(R.id.tiles_count);
    }

    private void verifyViews(int countVisibility, int iconViewCount) {
        assertEquals(mCountTileView.getVisibility(), countVisibility);
        assertEquals(mSharedImageTilesCoordinator.getAllIconViews().size(), iconViewCount);
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
        GroupMember memberValid =
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
                                new GroupMember[] {memberValid, memberInvalid1, memberInvalid2},
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
        doReturn(mDataSharingUiDelegate).when(mDataSharingService).getUIDelegate();

        mSharedImageTilesCoordinator.updateCollaborationId(COLLABORATION_ID);

        verify(mDataSharingUiDelegate)
                .showAvatars(any(), any(), eq(Arrays.asList(memberValid.email)), any(), any());
    }
}
