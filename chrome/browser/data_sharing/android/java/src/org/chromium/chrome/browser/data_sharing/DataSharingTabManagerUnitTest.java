// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseURLStatus;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Unit test for {@link DataSharingTabManager} */
@RunWith(BaseRobolectricTestRunner.class)
public class DataSharingTabManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker jniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private static final String GROUP_ID = "group_id";
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 123;
    private static final String ACCESS_TOKEN = "access_token";
    private static final String TEST_GROUP_DISPLAY_NAME = "Test Group";
    private static final String GAIA_ID = "GAIA_ID";
    private static final String EMAIL = "fake@gmail.com";
    private static final GURL TEST_URL = new GURL("https://www.example.com/");

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUIDelegate;
    @Mock private DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private BottomSheetContent mBottomSheetContent;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock Callback<Boolean> mCreateGroupFinishedCallback;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private DataSharingTabManager mDataSharingTabManager;
    private SavedTabGroup mSavedTabGroup;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);

        DataSharingServiceFactory.setForTesting(mDataSharingService);
        DataSharingServiceFactory.setDataSharingUIDelegateForTesting(mDataSharingUIDelegate);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        ObservableSupplier<Profile> profileSupplier = new ObservableSupplierImpl<Profile>(mProfile);
        Supplier<BottomSheetController> bottomSheetControllerSupplier =
                new ObservableSupplierImpl<BottomSheetController>(mBottomSheetController);
        ObservableSupplier<ShareDelegate> shareDelegateSupplier =
                new ObservableSupplierImpl<ShareDelegate>(mShareDelegate);
        mDataSharingTabManager =
                new DataSharingTabManager(
                        mDataSharingTabSwitcherDelegate,
                        profileSupplier,
                        bottomSheetControllerSupplier,
                        shareDelegateSupplier,
                        mWindowAndroid);

        mSavedTabGroup = new SavedTabGroup();
        mSavedTabGroup.collaborationId = GROUP_ID;
        mSavedTabGroup.localId = LOCAL_ID;
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroupTab.localId = TAB_ID;
        mSavedTabGroup.savedTabs.add(savedTabGroupTab);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
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
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(
                        new DataSharingService.ParseURLResult(
                                new GroupToken(GROUP_ID, "accessToken"), ParseURLStatus.SUCCESS))
                .when(mDataSharingService)
                .parseDataSharingURL(any());

        String[] tabId = new String[] {GROUP_ID};
        doReturn(tabId).when(mTabGroupSyncService).getAllGroupIds();

        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(GROUP_ID);

        mDataSharingTabManager.initiateJoinFlow(null);
        verify(mDataSharingTabSwitcherDelegate).openTabGroupWithTabId(TAB_ID);
    }

    @Test
    public void testInviteFlowWithNewTabGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(
                        new DataSharingService.ParseURLResult(
                                new GroupToken(GROUP_ID, ACCESS_TOKEN), ParseURLStatus.SUCCESS))
                .when(mDataSharingService)
                .parseDataSharingURL(any());

        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();

        mDataSharingTabManager.initiateJoinFlow(null);
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
    public void testCreateFlowWithExistingGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(GROUP_ID, TEST_GROUP_DISPLAY_NAME, groupMemberArray, ACCESS_TOKEN);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doAnswer(
                        invocation -> {
                            // Capture the callback passed to ensureGroupVisibility
                            Callback<DataSharingService.GroupDataOrFailureOutcome> callback =
                                    invocation.getArgument(1);
                            callback.onResult(outcome);
                            return null;
                        })
                .when(mDataSharingService)
                .ensureGroupVisibility(any(), any());
        doReturn(TEST_URL).when(mDataSharingService).getDataSharingURL(eq(groupData));
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));
        mDataSharingTabManager.createGroupFlow(null, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, null);
        // Verifying showShareSheet() method is called.
        verify(mDataSharingService).getDataSharingURL(eq(groupData));
        verify(mShareDelegate).share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    public void testCreateFlowWithNewTabGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(null).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(GROUP_ID, TEST_GROUP_DISPLAY_NAME, groupMemberArray, ACCESS_TOKEN);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doAnswer(
                        invocation -> {
                            // Capture the callback passed to createGroup
                            Callback<DataSharingService.GroupDataOrFailureOutcome> callback =
                                    invocation.getArgument(1);
                            callback.onResult(outcome);
                            return null;
                        })
                .when(mDataSharingService)
                .createGroup(any(), any());

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingURL(eq(groupData));
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mDataSharingTabManager.createGroupFlow(
                mActivity, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, mCreateGroupFinishedCallback);

        ArgumentCaptor<MemberPickerListenerImpl> memberPickerListenerCaptor =
                ArgumentCaptor.forClass(MemberPickerListenerImpl.class);
        verify(mDataSharingUIDelegate)
                .showMemberPicker(any(), any(), memberPickerListenerCaptor.capture(), any());
        Callback<List<String>> capturedPickerCallback =
                memberPickerListenerCaptor.getValue().getCallback();
        List<String> selectedEmails = Arrays.asList(EMAIL);
        capturedPickerCallback.onResult(selectedEmails);

        // Verifying DataSharingService createGroup API is called.
        verify(mDataSharingService).createGroup(eq(TEST_GROUP_DISPLAY_NAME), any(Callback.class));
        verify(mShareDelegate).share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }
}
