// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabGroupColorViewProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupColorViewProviderUnitTest {
    private static final Token REGULAR_TAB_GROUP_ID = new Token(3L, 4L);
    private static final Token INCOGNITO_TAB_GROUP_ID = new Token(5L, 6L);
    private static final Token OTHER_TAB_GROUP_ID = new Token(3L, 89L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;

    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;

    private Context mContext;
    private TabGroupColorViewProvider mRegularColorViewProvider;
    private TabGroupColorViewProvider mIncognitoColorViewProvider;

    @Before
    public void setUp() {
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mContext = activity;

        mRegularColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        EitherGroupId.createLocalId(new LocalTabGroupId(REGULAR_TAB_GROUP_ID)),
                        /* isIncognito= */ false,
                        TabGroupColorId.RED,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService);

        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());

        mIncognitoColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        EitherGroupId.createLocalId(new LocalTabGroupId(INCOGNITO_TAB_GROUP_ID)),
                        /* isIncognito= */ true,
                        TabGroupColorId.BLUE,
                        /* tabGroupSyncService= */ null,
                        /* dataSharingService= */ null,
                        /* collaborationService= */ null);
    }

    @Test
    public void testDestroy() {
        mRegularColorViewProvider.destroy();
        verify(mDataSharingService).removeObserver(any());

        // Shouldn't crash even though it is an effective no-op.
        mIncognitoColorViewProvider.destroy();
    }

    @Test
    public void testSetAndGetTabGroupId() {
        assertEquals(
                REGULAR_TAB_GROUP_ID,
                mRegularColorViewProvider.getTabGroupIdForTesting().getLocalId().tabGroupId);
        assertEquals(
                INCOGNITO_TAB_GROUP_ID,
                mIncognitoColorViewProvider.getTabGroupIdForTesting().getLocalId().tabGroupId);

        mRegularColorViewProvider.setTabGroupId(
                EitherGroupId.createLocalId(new LocalTabGroupId(OTHER_TAB_GROUP_ID)));
        assertEquals(
                OTHER_TAB_GROUP_ID,
                mRegularColorViewProvider.getTabGroupIdForTesting().getLocalId().tabGroupId);
    }

    @Test
    public void testHasCollaborationId() {
        assertFalse(mRegularColorViewProvider.hasCollaborationId());

        createCollaboration();
        assertTrue(mRegularColorViewProvider.hasCollaborationId());

        // Shouldn't crash even though it is an effective no-op.
        assertFalse(mIncognitoColorViewProvider.hasCollaborationId());
    }

    @Test
    public void testColorView_NotShared() {
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.RED,
                TabGroupColorId.CYAN);
        verifyColorView(
                mIncognitoColorViewProvider,
                /* isIncognito= */ true,
                TabGroupColorId.BLUE,
                TabGroupColorId.PURPLE);
    }

    @Test
    public void testColorView_Shared_AlreadyCreated() {
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.RED,
                TabGroupColorId.CYAN);

        createCollaboration();

        verifyColorViewCollaboration(TabGroupColorId.CYAN);

        mSharingObserverCaptor.getValue().onGroupRemoved(COLLABORATION_ID1);
        assertFalse(mRegularColorViewProvider.hasCollaborationId());

        // Verify the view is back to the unshared state.
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.CYAN,
                TabGroupColorId.GREY);
    }

    @Test
    public void testColorView_SharedToNotSharedIdChange() {
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.RED,
                TabGroupColorId.CYAN);

        createCollaboration();

        verifyColorViewCollaboration(TabGroupColorId.CYAN);

        mRegularColorViewProvider.setTabGroupId(
                EitherGroupId.createLocalId(new LocalTabGroupId(OTHER_TAB_GROUP_ID)));
        assertFalse(mRegularColorViewProvider.hasCollaborationId());

        // Verify the view is back to the unshared state.
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.CYAN,
                TabGroupColorId.GREY);
    }

    @Test
    public void testColorView_NotSharedToSharedIdChange() {
        mRegularColorViewProvider.setTabGroupId(
                EitherGroupId.createLocalId(new LocalTabGroupId(OTHER_TAB_GROUP_ID)));

        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.RED,
                TabGroupColorId.CYAN);

        createCollaboration();
        assertFalse(mRegularColorViewProvider.hasCollaborationId());

        mRegularColorViewProvider.setTabGroupId(
                EitherGroupId.createLocalId(new LocalTabGroupId(REGULAR_TAB_GROUP_ID)));
        assertTrue(mRegularColorViewProvider.hasCollaborationId());

        verifyColorViewCollaboration(TabGroupColorId.CYAN);
    }

    @Test
    public void testColorView_Shared_NotCreatedYet() {
        createCollaboration();

        verifyColorViewCollaboration(TabGroupColorId.RED);

        mRegularColorViewProvider.destroy();

        // Verify the view is back to the unshared state in the event it remains in the view
        // hierarchy transiently.
        verifyColorView(
                mRegularColorViewProvider,
                /* isIncognito= */ false,
                TabGroupColorId.RED,
                TabGroupColorId.PINK);
    }

    private void createCollaboration() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(REGULAR_TAB_GROUP_ID)))
                .thenReturn(savedTabGroup);
        var groupData =
                SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        mSharingObserverCaptor.getValue().onGroupAdded(groupData);
        when(mCollaborationService.getGroupData(COLLABORATION_ID1)).thenReturn(groupData);
    }

    private void verifyColorView(
            TabGroupColorViewProvider viewProvider,
            boolean isIncognito,
            @TabGroupColorId int initialColorId,
            @TabGroupColorId int finalColorId) {
        FrameLayout colorView = (FrameLayout) viewProvider.getLazyView();
        assertEquals(0, colorView.getChildCount());

        Resources res = mContext.getResources();
        @Px int size = res.getDimensionPixelSize(R.dimen.tab_group_color_icon_item_size);
        assertEquals(size, colorView.getMinimumWidth());
        assertEquals(size, colorView.getMinimumHeight());

        GradientDrawable drawable = (GradientDrawable) colorView.getBackground();
        assertNotNull(drawable);

        assertEquals(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, initialColorId, isIncognito),
                drawable.getColor().getDefaultColor());

        viewProvider.setTabGroupColorId(finalColorId);
        assertEquals(colorView, viewProvider.getLazyView());

        assertEquals(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, finalColorId, isIncognito),
                drawable.getColor().getDefaultColor());
        float radii = res.getDimension(R.dimen.tab_group_color_icon_item_radius);
        assertAllCornerRadiiAre(radii, drawable);
    }

    private void verifyColorViewCollaboration(@TabGroupColorId int currentColorId) {
        FrameLayout colorView = (FrameLayout) mRegularColorViewProvider.getLazyView();
        assertEquals(1, colorView.getChildCount());

        Resources res = mContext.getResources();
        final @Px int stroke = res.getDimensionPixelSize(R.dimen.tab_group_color_icon_stroke);
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) colorView.getChildAt(0).getLayoutParams();
        assertEquals(stroke, params.getMarginStart());
        assertEquals(stroke, params.topMargin);

        SharedImageTilesConfig config =
                SharedImageTilesConfig.Builder.createForTabGroupColorContext(
                                mContext, currentColorId)
                        .build();
        final @Px int size = config.getBorderAndTotalIconSizes(res).second + 2 * stroke;
        assertEquals(size, colorView.getMinimumWidth());
        assertEquals(size, colorView.getMinimumHeight());

        GradientDrawable drawable = (GradientDrawable) colorView.getBackground();
        assertNotNull(drawable);

        assertEquals(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, currentColorId, false),
                drawable.getColor().getDefaultColor());
        int radius = (size + 1) / 2;
        assertAllCornerRadiiAre((float) radius, drawable);
    }

    void assertAllCornerRadiiAre(float radius, GradientDrawable drawable) {
        float[] radii = drawable.getCornerRadii();
        assertNotNull(radii);
        assertEquals(8, radii.length);
        for (int i = 0; i < radii.length; i++) {
            assertEquals("Radii not equal at index " + i, radius, radii[i], MathUtils.EPSILON);
        }
    }
}
