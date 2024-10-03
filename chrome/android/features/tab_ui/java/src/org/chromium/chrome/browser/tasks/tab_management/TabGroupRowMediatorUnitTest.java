// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CLUSTER_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DESTROYABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DISPLAY_AS_SHARED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.core.util.Supplier;
import androidx.test.filters.SmallTest;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link TabGroupRowMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID, ChromeFeatureList.DATA_SHARING})
public class TabGroupRowMediatorUnitTest {
    private static final String COLLABORATION_ID1 = "collaborationId1";
    private static final String EMAIL1 = "one@gmail.com";
    private static final String EMAIL2 = "two@gmail.com";
    private static final String GAIA_ID1 = "gaiaId1";
    private static final String GAIA_ID2 = "gaiaId2";
    private static final String SYNC_GROUP_ID1 = "syncGroup1";
    private static final GroupMember GROUP_MEMBER1 =
            newGroupMember(GAIA_ID1, EMAIL1, MemberRole.OWNER);
    private static final GroupMember GROUP_MEMBER2 =
            newGroupMember(GAIA_ID2, EMAIL2, MemberRole.MEMBER);

    private static GroupMember newGroupMember(
            String gaiaId, String email, @MemberRole int memberRole) {
        return new GroupMember(
                gaiaId,
                /* displayName= */ null,
                email,
                memberRole,
                /* avatarUrl= */ null,
                /* givenName= */ null);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private PaneManager mPaneManager;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private CoreAccountInfo mCoreAccountInfo;
    @Mock private Supplier<Integer> mFetchGroupState;

    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;

    private SavedTabGroupTab mTab1;
    private SavedTabGroupTab mTab2;
    private SavedTabGroupTab mTab3;
    private SavedTabGroupTab mTab4;
    private SavedTabGroupTab mTab5;

    @Before
    public void setUp() {
        // Cannot initialize even test GURLs too early.
        mTab1 = newTab(JUnitTestGURLs.URL_1);
        mTab2 = newTab(JUnitTestGURLs.URL_2);
        mTab3 = newTab(JUnitTestGURLs.URL_3);
        mTab4 = newTab(JUnitTestGURLs.BLUE_1);
        mTab5 = newTab(JUnitTestGURLs.BLUE_2);
    }

    private PropertyModel buildTestModel(List<SavedTabGroupTab> savedTabs) {
        return buildTestModel(savedTabs, /* isShared= */ false);
    }

    private PropertyModel buildTestModel(List<SavedTabGroupTab> savedTabs, boolean isShared) {
        SavedTabGroup group = newGroup(savedTabs, isShared ? COLLABORATION_ID1 : null);
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        TabGroupRowMediator mediator =
                new TabGroupRowMediator(
                        context,
                        group,
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mPaneManager,
                        mTabGroupUiActionHandler,
                        mModalDialogManager,
                        mActionConfirmationManager,
                        mFaviconResolver,
                        new LazyOneshotSupplierImpl<>() {
                            @Override
                            public void doSet() {
                                set(mCoreAccountInfo);
                            }
                        },
                        mFetchGroupState);
        return mediator.getModel();
    }

    private SavedTabGroupTab newTab(GURL url) {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.url = url;
        return tab;
    }

    private SavedTabGroup newGroup(List<SavedTabGroupTab> savedTabs, String collaborationId) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = savedTabs;
        group.collaborationId = collaborationId;
        return group;
    }

    private void respondToReadGroup(GroupMember[] members) {
        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        members,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testNoParity() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(new SavedTabGroupTab()));
        // 0 is the default value.
        assertEquals(0, propertyModel.get(COLOR_INDEX));
    }

    @Test
    @SmallTest
    public void testFavicons_zero() {
        PropertyModel propertyModel = buildTestModel(Collections.emptyList());
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(0, clusterData.totalCount);
        assertEquals(0, clusterData.firstUrls.size());
    }

    @Test
    @SmallTest
    public void testFavicons_one() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(1, clusterData.totalCount);
        assertEquals(1, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
    }

    @Test
    @SmallTest
    public void testFavicons_two() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1, mTab2));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(2, clusterData.totalCount);
        assertEquals(2, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
    }

    @Test
    @SmallTest
    public void testFavicons_three() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1, mTab2, mTab3));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(3, clusterData.totalCount);
        assertEquals(3, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
    }

    @Test
    @SmallTest
    public void testFavicons_four() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1, mTab2, mTab3, mTab4));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(4, clusterData.totalCount);
        assertEquals(4, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
        assertEquals(JUnitTestGURLs.BLUE_1, clusterData.firstUrls.get(3));
    }

    @Test
    @SmallTest
    public void testFavicons_five() {
        List<SavedTabGroupTab> tabs = Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5);
        PropertyModel propertyModel = buildTestModel(tabs);
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(5, clusterData.totalCount);
        assertEquals(4, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
        assertEquals(JUnitTestGURLs.BLUE_1, clusterData.firstUrls.get(3));
    }

    @Test
    @SmallTest
    public void testNotShared() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1), /* isShared= */ false);
        assertFalse(propertyModel.get(DISPLAY_AS_SHARED));
        assertNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }

    @Test
    @SmallTest
    public void testCollaborationButOnlyOneUser() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1), /* isShared= */ true);
        respondToReadGroup(new GroupMember[] {GROUP_MEMBER1});

        assertFalse(propertyModel.get(DISPLAY_AS_SHARED));
        assertNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }

    @Test
    @SmallTest
    public void testShared() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1), /* isShared= */ true);
        respondToReadGroup(new GroupMember[] {GROUP_MEMBER1, GROUP_MEMBER2});

        assertTrue(propertyModel.get(DISPLAY_AS_SHARED));
        assertNotNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }

    @Test
    @SmallTest
    public void testDestroyable() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(mTab1), /* isShared= */ true);

        assertFalse(propertyModel.get(DISPLAY_AS_SHARED));
        propertyModel.get(DESTROYABLE).destroy();

        respondToReadGroup(new GroupMember[] {GROUP_MEMBER1, GROUP_MEMBER2});
        assertFalse(propertyModel.get(DISPLAY_AS_SHARED));
        assertNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }
}
