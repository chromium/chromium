// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.DataSharingService.ParseUrlResult;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseUrlStatus;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.PeopleGroupActionOutcome;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Unit test for {@link DataSharingTabManager} */
@RunWith(BaseRobolectricTestRunner.class)
public class DataSharingTabManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker jniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private static final String GROUP_ID = "group_id";
    private static final String SYNC_ID = "sync_id";
    private static final Integer ROOT_ID = 123;
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 456;
    private static final String ACCESS_TOKEN = "access_token";
    private static final String TEST_GROUP_DISPLAY_NAME = "Test Group";
    private static final String GAIA_ID = "GAIA_ID";
    private static final String EMAIL = "fake@gmail.com";
    private static final GURL TEST_URL = new GURL("https://www.example.com/");

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private DataSharingTabSwitcherDelegate mDataSharingTabSwitcherDelegate;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private Callback<Boolean> mCreateGroupFinishedCallback;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private TabModel mTabModel;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mOutcomeCallbackCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private DataSharingTabManager mDataSharingTabManager;
    private SavedTabGroup mSavedTabGroup;
    private Activity mActivity;
    private OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplier<Profile> mProfileSupplier;
    private Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;

    @Before
    public void setUp() {
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);

        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        mProfileSupplier = new ObservableSupplierImpl<Profile>(mProfile);
        mBottomSheetControllerSupplier =
                new ObservableSupplierImpl<BottomSheetController>(mBottomSheetController);
        mShareDelegateSupplier = new ObservableSupplierImpl<ShareDelegate>(mShareDelegate);
        mTabGroupUiActionHandlerSupplier.set(mTabGroupUiActionHandler);

        mDataSharingTabManager =
                new DataSharingTabManager(
                        mDataSharingTabSwitcherDelegate,
                        mProfileSupplier,
                        mBottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        ApplicationProvider.getApplicationContext().getResources(),
                        mTabGroupUiActionHandlerSupplier);

        mSavedTabGroup = new SavedTabGroup();
        mSavedTabGroup.collaborationId = GROUP_ID;
        mSavedTabGroup.syncId = SYNC_ID;
        mSavedTabGroup.localId = LOCAL_ID;
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroupTab.localId = TAB_ID;
        mSavedTabGroup.savedTabs.add(savedTabGroupTab);

        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
    }

    private void mockSuccessfulParseDataSharingUrl() {
        GroupToken groupToken = new GroupToken(GROUP_ID, ACCESS_TOKEN);
        ParseUrlResult result =
                new DataSharingService.ParseUrlResult(groupToken, ParseUrlStatus.SUCCESS);
        when(mDataSharingService.parseDataSharingUrl(any())).thenReturn(result);
    }

    private void mockUnsuccessfulParseDataSharingUrl(@ParseUrlStatus int status) {
        assert status != ParseUrlStatus.SUCCESS;
        ParseUrlResult result =
                new DataSharingService.ParseUrlResult(/* groupToken= */ null, status);
        when(mDataSharingService.parseDataSharingUrl(any())).thenReturn(result);
    }

    @Test
    public void testNoProfile() {
        mProfileSupplier = new ObservableSupplierImpl<Profile>();
        mDataSharingTabManager =
                new DataSharingTabManager(
                        mDataSharingTabSwitcherDelegate,
                        mProfileSupplier,
                        mBottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        ApplicationProvider.getApplicationContext().getResources(),
                        mTabGroupUiActionHandlerSupplier);
        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        // Verify we never parse the URL without a profile.
        verify(mDataSharingService, never()).parseDataSharingUrl(TEST_URL);
    }

    @Test
    public void testInvalidUrl() {
        doReturn(new DataSharingService.ParseUrlResult(null, ParseUrlStatus.UNKNOWN))
                .when(mDataSharingService)
                .parseDataSharingUrl(TEST_URL);

        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        // Verify sync is never checked when parsing error occurs.
        verify(mTabGroupSyncService, never()).getAllGroupIds();
    }

    @Test
    public void testJoinFlowWithExistingTabGroup() {
        mockSuccessfulParseDataSharingUrl();

        // Mock exist in sync.
        String[] tabId = new String[] {GROUP_ID};
        doReturn(tabId).when(mTabGroupSyncService).getAllGroupIds();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(GROUP_ID);

        // Mock exist in local tab model.
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        Tab tab = new MockTab(TAB_ID, mProfile);
        doReturn(tab).when(mTabModel).getTabById(TAB_ID);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab);

        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);
        verify(mDataSharingTabSwitcherDelegate).openTabGroupWithTabId(TAB_ID);
    }

    @Test
    public void testJoinFlowWithExistingTabGroupSyncOnly() {
        doReturn(
                        new DataSharingService.ParseUrlResult(
                                new GroupToken(GROUP_ID, ACCESS_TOKEN), ParseUrlStatus.SUCCESS))
                .when(mDataSharingService)
                .parseDataSharingUrl(any());

        // Mock exist in sync.
        doReturn(new String[] {GROUP_ID}).when(mTabGroupSyncService).getAllGroupIds();

        // Mock does not exist in local tab model.
        SavedTabGroup savedTabGroupWithoutLocalId = new SavedTabGroup();
        savedTabGroupWithoutLocalId.collaborationId = GROUP_ID;
        savedTabGroupWithoutLocalId.syncId = SYNC_ID;
        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        savedTabGroupWithoutLocalId.savedTabs.add(savedTabGroupTab);

        when(mTabGroupSyncService.getGroup(GROUP_ID)).thenReturn(savedTabGroupWithoutLocalId);
        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(mSavedTabGroup);

        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        verify(mTabGroupUiActionHandler).openTabGroup(SYNC_ID);
        verify(mDataSharingTabSwitcherDelegate).openTabGroupWithTabId(TAB_ID);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testOldUiJoinFlowWithNewTabGroup() {
        mockSuccessfulParseDataSharingUrl();

        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();

        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        // The same group should not be observed twice.
        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        verify(mTabGroupSyncService).addObserver(any());
        verify(mDataSharingService, times(2)).addMember(eq(GROUP_ID), eq(ACCESS_TOKEN), any());
    }

    private static final class JoinTestHelper {
        Boolean mCallbackResult;
        Callback<Boolean> mCallback =
                new Callback<Boolean>() {
                    @Override
                    public void onResult(Boolean result) {
                        Assert.assertTrue(result);
                        mCallbackResult = result;
                    }
                };

        JoinTestHelper() {}

        Callback<Boolean> getCallback() {
            return mCallback;
        }

        void waitForCallback() {
            CriteriaHelper.pollUiThreadForJUnit(() -> mCallbackResult);
        }
    }

    private org.chromium.components.sync.protocol.GroupData getSyncGroupData() {
        return org.chromium.components.sync.protocol.GroupData.newBuilder()
                .setGroupId(GROUP_ID)
                .setDisplayName(TEST_GROUP_DISPLAY_NAME)
                .build();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testJoinFlowWithNewTabGroup() {
        mockSuccessfulParseDataSharingUrl();

        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();

        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        ArgumentCaptor<DataSharingJoinUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.class);
        verify(mDataSharingUiDelegate).showJoinFlow(uiConfigCaptor.capture());

        ArgumentCaptor<TabGroupSyncService.Observer> syncObserverCaptor =
                ArgumentCaptor.forClass(TabGroupSyncService.Observer.class);
        verify(mTabGroupSyncService).addObserver(syncObserverCaptor.capture());

        DataSharingJoinUiConfig uiConfig = uiConfigCaptor.getValue();
        Assert.assertEquals(GROUP_ID, uiConfig.getGroupToken().groupId);
        Assert.assertEquals(ACCESS_TOKEN, uiConfig.getGroupToken().accessToken);

        Assert.assertNotNull(uiConfig.getJoinCallback());
        JoinTestHelper helper = new JoinTestHelper();

        uiConfig.getJoinCallback().onGroupJoinedWithWait(getSyncGroupData(), helper.getCallback());
        syncObserverCaptor.getValue().onTabGroupAdded(mSavedTabGroup, TriggerSource.REMOTE);

        helper.waitForCallback();
        verify(mDataSharingUiDelegate).destroyFlow(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testJoinFlowWithNewTabGroupOpenedBeforeJoinCallback() {
        mockSuccessfulParseDataSharingUrl();

        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();

        mDataSharingTabManager.initiateJoinFlow(null, TEST_URL);

        ArgumentCaptor<DataSharingJoinUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.class);
        verify(mDataSharingUiDelegate).showJoinFlow(uiConfigCaptor.capture());

        ArgumentCaptor<TabGroupSyncService.Observer> syncObserverCaptor =
                ArgumentCaptor.forClass(TabGroupSyncService.Observer.class);
        verify(mTabGroupSyncService).addObserver(syncObserverCaptor.capture());

        DataSharingJoinUiConfig uiConfig = uiConfigCaptor.getValue();
        Assert.assertEquals(GROUP_ID, uiConfig.getGroupToken().groupId);
        Assert.assertEquals(ACCESS_TOKEN, uiConfig.getGroupToken().accessToken);

        Assert.assertNotNull(uiConfig.getJoinCallback());
        JoinTestHelper helper = new JoinTestHelper();

        syncObserverCaptor.getValue().onTabGroupAdded(mSavedTabGroup, TriggerSource.REMOTE);
        uiConfig.getJoinCallback().onGroupJoinedWithWait(getSyncGroupData(), helper.getCallback());

        helper.waitForCallback();
        verify(mDataSharingUiDelegate).destroyFlow(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testManageSharing() {
        mDataSharingTabManager.showManageSharing(mActivity, GROUP_ID);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testDestroy() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockSuccessfulParseDataSharingUrl();
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        doReturn(new String[0]).when(mTabGroupSyncService).getAllGroupIds();
        mDataSharingTabManager.initiateJoinFlow(null, /* dataSharingURL= */ null);
        // Need to get an observer to verify destroy removes it.
        verify(mTabGroupSyncService).addObserver(any());

        mDataSharingTabManager.destroy();
        verify(mTabGroupSyncService).removeObserver(any());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testCreateFlowOldUiWithExistingGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(GROUP_ID, TEST_GROUP_DISPLAY_NAME, groupMemberArray, ACCESS_TOKEN);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doCallback(1, (Callback<GroupDataOrFailureOutcome> callback) -> callback.onResult(outcome))
                .when(mDataSharingService)
                .ensureGroupVisibility(any(), any());
        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(eq(groupData));
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));
        mDataSharingTabManager.createGroupFlow(null, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, null);
        // Verifying showShareSheet() method is called.
        verify(mDataSharingService).getDataSharingUrl(eq(groupData));
        verify(mShareDelegate).share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testCreateFlowWithExistingGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(any());
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mDataSharingTabManager.createGroupFlow(null, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, null);

        ArgumentCaptor<DataSharingManageUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingManageUiConfig.class);
        verify(mDataSharingUiDelegate).showManageFlow(uiConfigCaptor.capture());

        DataSharingManageUiConfig uiConfig = uiConfigCaptor.getValue();
        Assert.assertEquals(GROUP_ID, uiConfig.getGroupToken().groupId);
        // Manage should not pass access token.
        Assert.assertEquals(null, uiConfig.getGroupToken().accessToken);

        Assert.assertNotNull(uiConfig.getManageCallback());
        uiConfig.getManageCallback()
                .onShareInviteLinkClicked(new GroupToken(GROUP_ID, ACCESS_TOKEN));
        verify(mShareDelegate).share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testCreateFlowOldUiWithNewTabGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(null).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(GROUP_ID, TEST_GROUP_DISPLAY_NAME, groupMemberArray, ACCESS_TOKEN);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doCallback(1, (Callback<GroupDataOrFailureOutcome> callback) -> callback.onResult(outcome))
                .when(mDataSharingService)
                .createGroup(any(), any());

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(eq(groupData));
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createGroupFlow(
                mActivity, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, mCreateGroupFinishedCallback);

        // Verifying DataSharingService createGroup API is called.
        verify(mDataSharingService).createGroup(eq(TEST_GROUP_DISPLAY_NAME), any());
        verify(mShareDelegate).share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testCreateFlowWithNewTabGroup() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(null).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(GROUP_ID, TEST_GROUP_DISPLAY_NAME, groupMemberArray, ACCESS_TOKEN);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doCallback(1, (Callback<GroupDataOrFailureOutcome> callback) -> callback.onResult(outcome))
                .when(mDataSharingService)
                .createGroup(any(), any());

        var groupDataProto = getSyncGroupData();

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(any());
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createGroupFlow(
                mActivity, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, mCreateGroupFinishedCallback);

        ArgumentCaptor<DataSharingCreateUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingCreateUiConfig.class);
        verify(mDataSharingUiDelegate).showCreateFlow(uiConfigCaptor.capture());

        DataSharingCreateUiConfig uiConfig = uiConfigCaptor.getValue();

        Assert.assertNotNull(uiConfig.getCreateCallback());
        uiConfig.getCreateCallback().onGroupCreated(groupDataProto);

        // Verifying DataSharingService createGroup API is called.
        verify(mTabGroupSyncService).makeTabGroupShared(LOCAL_ID, GROUP_ID);
        verify(mShareDelegate).share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testCreateFlowCancelled() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(null).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(GROUP_ID, TEST_GROUP_DISPLAY_NAME, groupMemberArray, ACCESS_TOKEN);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        doCallback(1, (Callback<GroupDataOrFailureOutcome> callback) -> callback.onResult(outcome))
                .when(mDataSharingService)
                .createGroup(any(), any());

        doReturn(TEST_URL).when(mDataSharingService).getDataSharingUrl(any());
        doReturn(TEST_URL)
                .when(mDistillerUrlUtilsJniMock)
                .getOriginalUrlFromDistillerUrl(any(String.class));

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createGroupFlow(
                mActivity, TEST_GROUP_DISPLAY_NAME, LOCAL_ID, mCreateGroupFinishedCallback);

        ArgumentCaptor<DataSharingCreateUiConfig> uiConfigCaptor =
                ArgumentCaptor.forClass(DataSharingCreateUiConfig.class);
        verify(mDataSharingUiDelegate).showCreateFlow(uiConfigCaptor.capture());

        DataSharingCreateUiConfig uiConfig = uiConfigCaptor.getValue();

        Assert.assertNotNull(uiConfig.getCreateCallback());
        uiConfig.getCreateCallback().onCancelClicked();

        // Verifying DataSharingService createGroup API is called.
        verify(mTabGroupSyncService, never()).makeTabGroupShared(any(), any());
        verify(mShareDelegate, never())
                .share(any(), any(), eq(ShareDelegate.ShareOrigin.TAB_GROUP));
    }

    @Test
    public void testParseDataSharingUrlFailure() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockUnsuccessfulParseDataSharingUrl(ParseUrlStatus.HOST_OR_PATH_MISMATCH_FAILURE);

        mDataSharingTabManager.initiateJoinFlow(null, /* dataSharingURL= */ null);
        verify(mModalDialogManager).showDialog(mPropertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                mPropertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID_V2})
    public void testAddMemberFailure() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockSuccessfulParseDataSharingUrl();

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        mDataSharingTabManager.initiateJoinFlow(null, /* dataSharingURL= */ null);
        verify(mDataSharingService).addMember(any(), any(), mOutcomeCallbackCaptor.capture());

        mOutcomeCallbackCaptor.getValue().onResult(PeopleGroupActionOutcome.PERSISTENT_FAILURE);
        verify(mModalDialogManager).showDialog(mPropertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                mPropertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
    }
}
