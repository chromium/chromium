// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

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

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.ParseURLResult;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseURLStatus;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit test for {@link DataSharingTabManager} */
@RunWith(BaseRobolectricTestRunner.class)
public class DataSharingTabManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private static final String GROUP_ID = "group_id";
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 123;
    private static final String ACCESS_TOKEN = "access_token";

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUIDelegate;
    @Mock private DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private WindowAndroid mWindowAndroid;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private DataSharingTabManager mDataSharingTabManager;
    private SavedTabGroup mSavedTabGroup;
    private Activity mActivity;

    @Before
    public void setUp() {
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        ObservableSupplier<Profile> profileSupplier = new ObservableSupplierImpl<>(mProfile);
        Supplier<BottomSheetController> bottomSheetControllerSupplier =
                new ObservableSupplierImpl<>(mBottomSheetController);
        mDataSharingTabManager =
                new DataSharingTabManager(
                        mDataSharingTabSwitcherDelegate,
                        profileSupplier,
                        bottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid);

        mSavedTabGroup = new SavedTabGroup();
        mSavedTabGroup.collaborationId = GROUP_ID;
        mSavedTabGroup.localId = LOCAL_ID;
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroupTab.localId = TAB_ID;
        mSavedTabGroup.savedTabs.add(savedTabGroupTab);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);

        doReturn(mDataSharingUIDelegate).when(mDataSharingService).getUIDelegate();
        doReturn(mProfile).when(mProfile).getOriginalProfile();
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
    }

    private void mockSuccessfulParseDataSharingURL() {
        GroupToken groupToken = new GroupToken(GROUP_ID, ACCESS_TOKEN);
        ParseURLResult result =
                new DataSharingService.ParseURLResult(groupToken, ParseURLStatus.SUCCESS);
        when(mDataSharingService.parseDataSharingURL(any())).thenReturn(result);
    }

    @Test
    public void testInvalidURL() {
        doReturn(new DataSharingService.ParseURLResult(null, ParseURLStatus.UNKNOWN))
                .when(mDataSharingService)
                .parseDataSharingURL(any());
        mDataSharingTabManager.initiateJoinFlow(null);
    }

    @Test
    public void testInviteFlowWithExistingTabGroup() {
        mockSuccessfulParseDataSharingURL();

        String[] tabId = new String[] {GROUP_ID};
        doReturn(tabId).when(mTabGroupSyncService).getAllGroupIds();

        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(GROUP_ID);

        mDataSharingTabManager.initiateJoinFlow(/* dataSharingURL= */ null);
        verify(mDataSharingTabSwitcherDelegate).openTabGroupWithTabId(TAB_ID);
    }

    @Test
    public void testInviteFlowWithNewTabGroup() {
        mockSuccessfulParseDataSharingURL();

        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();

        mDataSharingTabManager.initiateJoinFlow(/* dataSharingURL= */ null);
        verify(mTabGroupSyncService).addObserver(any());
        verify(mDataSharingService).addMember(eq(GROUP_ID), eq(ACCESS_TOKEN), any());
    }

    @Test
    public void testManageSharing() {
        mDataSharingTabManager.showManageSharing(mActivity, GROUP_ID);

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        verify(mDataSharingUIDelegate)
                .createGroupMemberListView(
                        eq(mActivity),
                        /* view= */ any(),
                        eq(GROUP_ID),
                        /* tokenSecret= */ any(),
                        /* config= */ any());
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);
    }

    @Test
    public void testDestroy() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockSuccessfulParseDataSharingURL();
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();
        mDataSharingTabManager.initiateJoinFlow(/* dataSharingURL= */ null);
        // Need to get an observer to verify destroy removes it.
        verify(mTabGroupSyncService).addObserver(any());

        mDataSharingTabManager.destroy();
        verify(mTabGroupSyncService).removeObserver(any());
    }
}
