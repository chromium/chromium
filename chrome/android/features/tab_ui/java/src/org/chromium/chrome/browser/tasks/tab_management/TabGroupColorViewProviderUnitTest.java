// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.widget.FrameLayout;

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

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.ServiceStatus;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabGroupColorViewProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupColorViewProviderUnitTest {
    private static final String COLLABORATION_ID1 = "collaborationId1";
    private static final Token REGULAR_TAB_GROUP_ID = new Token(3L, 4L);
    private static final Token INCOGNITO_TAB_GROUP_ID = new Token(5L, 6L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private ServiceStatus mServiceStatus;

    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;
    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;

    private SharedGroupTestHelper mSharedGroupTestHelper;
    private Context mContext;
    private TabGroupColorViewProvider mRegularColorViewProvider;
    private TabGroupColorViewProvider mIncognitoColorViewProvider;

    @Before
    public void setUp() {
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mDataSharingService.getServiceStatus()).thenReturn(mServiceStatus);

        mSharedGroupTestHelper =
                new SharedGroupTestHelper(mDataSharingService, mReadGroupCallbackCaptor);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mContext = activity;

        mRegularColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        REGULAR_TAB_GROUP_ID,
                        /* isIncognito= */ false,
                        TabGroupColorId.RED,
                        mTabGroupSyncService,
                        mDataSharingService);

        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());

        mIncognitoColorViewProvider =
                new TabGroupColorViewProvider(
                        activity,
                        INCOGNITO_TAB_GROUP_ID,
                        /* isIncognito= */ true,
                        TabGroupColorId.BLUE,
                        /* tabGroupSyncService= */ null,
                        /* dataSharingService= */ null);
    }

    @Test
    public void testDestroy() {
        mRegularColorViewProvider.destroy();
        verify(mDataSharingService).removeObserver(any());

        // Shouldn't crash even though it is an effective no-op.
        mIncognitoColorViewProvider.destroy();
    }

    @Test
    public void testGetTabGroupId() {
        assertEquals(REGULAR_TAB_GROUP_ID, mRegularColorViewProvider.getTabGroupId());
        assertEquals(INCOGNITO_TAB_GROUP_ID, mIncognitoColorViewProvider.getTabGroupId());
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
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        mSharingObserverCaptor
                .getValue()
                .onGroupAdded(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2));
    }

    private void verifyColorView(
            TabGroupColorViewProvider viewProvider,
            boolean isIncognito,
            @TabGroupColorId int initialColorId,
            @TabGroupColorId int finalColorId) {
        FrameLayout colorView = (FrameLayout) viewProvider.getLazyView();
        assertEquals(0, colorView.getChildCount());

        GradientDrawable drawable = (GradientDrawable) colorView.getBackground();
        assertNotNull(drawable);

        assertEquals(
                ColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, initialColorId, isIncognito),
                drawable.getColor().getDefaultColor());
        assertNotEquals(0, viewProvider.getStrokeWidthForTesting());

        viewProvider.setTabGroupColorId(finalColorId);
        assertEquals(colorView, viewProvider.getLazyView());

        assertEquals(
                ColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, finalColorId, isIncognito),
                drawable.getColor().getDefaultColor());
        assertNotEquals(0, viewProvider.getStrokeWidthForTesting());
    }

    private void verifyColorViewCollaboration(@TabGroupColorId int currentColorId) {
        FrameLayout colorView = (FrameLayout) mRegularColorViewProvider.getLazyView();
        assertEquals(1, colorView.getChildCount());

        GradientDrawable drawable = (GradientDrawable) colorView.getBackground();
        assertNotNull(drawable);

        assertEquals(
                ColorPickerUtils.getTabGroupColorPickerItemColor(mContext, currentColorId, false),
                drawable.getColor().getDefaultColor());
        assertEquals(0, mRegularColorViewProvider.getStrokeWidthForTesting());
    }
}
