// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;

import android.app.Activity;
import android.content.Intent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupsDelegate;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.loading.LoadingFullscreenCoordinator;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.Outcome;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.SigninStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.TabPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** Unit test for {@link CollaborationControllerDelegateImpl} */
@RunWith(BaseRobolectricTestRunner.class)
public class CollaborationControllerDelegateImplUnitTest {
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 456;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Callback<Boolean> mCloseScreenCallback;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private LoadingFullscreenCoordinator mLoadingFullscreenCoordinator;
    @Mock private SigninManager mSigninManager;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private Callback<Runnable> mSwitchToTabSwitcherCallback;
    @Mock private Callback<Callback<Boolean>> mStartAccountRefreshCallback;
    @Mock private CollaborationService mCollaborationService;
    @Mock private FaviconHelper mFaviconHelper;

    @Mock
    private CollaborationControllerDelegateImpl.Natives
            mCollaborationControllerDelegateImplNativeMock;

    private CollaborationControllerDelegateImpl mCollaborationControllerDelegateImpl;
    private Activity mActivity;
    private SavedTabGroup mSavedTabGroup;
    private SyncedGroupTestHelper mSyncedGroupTestHelper;

    @Before
    public void setUp() {
        CollaborationControllerDelegateImplJni.setInstanceForTesting(
                mCollaborationControllerDelegateImplNativeMock);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        CollaborationServiceFactory.setForTesting(mCollaborationService);

        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        mSavedTabGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        mSavedTabGroup.collaborationId = COLLABORATION_ID1;
        mSavedTabGroup.localId = LOCAL_ID;
        mSavedTabGroup.savedTabs = SyncedGroupTestHelper.tabsFromIds(TAB_ID);
        mSavedTabGroup.savedTabs.get(0).url = new GURL("https://www.example.com/");

        doReturn(
                        new ServiceStatus(
                                /* signinStatus= */ 0,
                                /* syncStatus= */ 0,
                                /* collaborationStatus= */ 0))
                .when(mCollaborationService)
                .getServiceStatus();
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        doReturn(true).when(mSigninManager).isSigninAllowed();
        doReturn((long) 0)
                .when(mCollaborationControllerDelegateImplNativeMock)
                .createNativeObject(any());
        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
        doReturn(mProfile).when(mDataSharingTabManager).getProfile();
        doReturn(mWindowAndroid).when(mDataSharingTabManager).getWindowAndroid();
        doReturn(mDataSharingService).when(mDataSharingTabManager).getDataSharingService();
        doReturn(mDataSharingUiDelegate).when(mDataSharingTabManager).getUiDelegate();
        doReturn(mDataSharingTabGroupsDelegate).when(mDataSharingTabManager).getTabGroupsDelegate();
        doReturn(mModalDialogManager).when(mWindowAndroid).getModalDialogManager();
        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getContext();
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
    }

    private void createDelegate(@FlowType int type) {
        mCollaborationControllerDelegateImpl =
                new CollaborationControllerDelegateImpl(
                        mActivity,
                        type,
                        mDataSharingTabManager,
                        mSigninAndHistorySyncActivityLauncher,
                        mLoadingFullscreenCoordinator,
                        mSwitchToTabSwitcherCallback,
                        mStartAccountRefreshCallback);

        if (type == FlowType.JOIN) {
            verify(mLoadingFullscreenCoordinator).startLoading(any(), any());
        }
    }

    @Test
    public void testWaitForTabSwitcher() {
        mCollaborationControllerDelegateImpl =
                new CollaborationControllerDelegateImpl(
                        mActivity,
                        FlowType.JOIN,
                        mDataSharingTabManager,
                        mSigninAndHistorySyncActivityLauncher,
                        mLoadingFullscreenCoordinator,
                        mSwitchToTabSwitcherCallback,
                        mStartAccountRefreshCallback);
        verify(mLoadingFullscreenCoordinator).startLoading(any(), any());

        long resultCallback = 1;
        long exitCallback = 2;
        ArgumentCaptor<Runnable> onTabSwitcherShownRunnableCaptor =
                ArgumentCaptor.forClass(Runnable.class);
        mCollaborationControllerDelegateImpl.prepareFlowUI(exitCallback, resultCallback);
        verify(mSwitchToTabSwitcherCallback).onResult(onTabSwitcherShownRunnableCaptor.capture());
        verify(mCollaborationControllerDelegateImplNativeMock, never())
                .runResultCallback(anyInt(), eq(resultCallback));

        onTabSwitcherShownRunnableCaptor.getValue().run();
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }

    @Test
    public void testShowAuthenticationUiJoinFlow() {
        createDelegate(FlowType.JOIN);
        Intent intent = new Intent();
        long resultCallback = 1;

        doReturn(intent)
                .when(mSigninAndHistorySyncActivityLauncher)
                .createFullscreenSigninIntentOrShowError(
                        eq(mActivity),
                        eq(mProfile),
                        any(FullscreenSigninAndHistorySyncConfig.class),
                        eq(SigninAccessPoint.COLLABORATION_TAB_GROUP));

        mCollaborationControllerDelegateImpl.showAuthenticationUi(resultCallback);
        ArgumentCaptor<IntentCallback> intentCallbackCaptor =
                ArgumentCaptor.forClass(IntentCallback.class);
        verify(mWindowAndroid)
                .showCancelableIntent(eq(intent), intentCallbackCaptor.capture(), eq(null));
        intentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, null);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }

    @Test
    public void testShowAuthenticationSigninPaused() {
        doReturn(
                        new ServiceStatus(
                                SigninStatus.SIGNED_IN_PAUSED,
                                /* syncStatus= */ 0,
                                /* collaborationStatus= */ 0))
                .when(mCollaborationService)
                .getServiceStatus();
        createDelegate(FlowType.JOIN);
        long resultCallback = 1;
        mCollaborationControllerDelegateImpl.showAuthenticationUi(resultCallback);

        ArgumentCaptor<Callback<Boolean>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mStartAccountRefreshCallback).onResult(successCallback.capture());

        successCallback.getValue().onResult(true);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }

    @Test
    public void testSigninNotAllowed() {
        createDelegate(FlowType.JOIN);
        long resultCallback = 1;

        doReturn(false).when(mSigninManager).isSigninAllowed();

        mCollaborationControllerDelegateImpl.showAuthenticationUi(resultCallback);
        verify(mLoadingFullscreenCoordinator).closeLoadingScreen();

        ArgumentCaptor<PropertyModel> propertyModelCaptor =
                ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(propertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                propertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
        verify(mSettingsNavigation).startSettings(eq(mActivity), anyInt());

        controller.onDismiss(propertyModelCaptor.getValue(), DialogDismissalCause.NAVIGATE);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.CANCEL), eq(resultCallback));
    }

    @Test
    public void testShowAuthenticationUiShareOrManageFlow() {
        createDelegate(FlowType.SHARE_OR_MANAGE);
        Intent intent = new Intent();
        long resultCallback = 1;

        doReturn(intent)
                .when(mSigninAndHistorySyncActivityLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mActivity),
                        eq(mProfile),
                        any(BottomSheetSigninAndHistorySyncConfig.class),
                        eq(SigninAccessPoint.COLLABORATION_TAB_GROUP));

        mCollaborationControllerDelegateImpl.showAuthenticationUi(resultCallback);
        ArgumentCaptor<IntentCallback> intentCallbackCaptor =
                ArgumentCaptor.forClass(IntentCallback.class);
        verify(mWindowAndroid)
                .showCancelableIntent(eq(intent), intentCallbackCaptor.capture(), eq(null));
        intentCallbackCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, null);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }

    @Test
    public void testExitFlowFromService() {
        createDelegate(FlowType.JOIN);
        Intent intent = new Intent();
        long prepareResultCallback = 1;
        long authenticationResultCallback = 2;
        int requestCode = 3;
        long exitCallback = 4;

        doReturn(intent)
                .when(mSigninAndHistorySyncActivityLauncher)
                .createFullscreenSigninIntentOrShowError(
                        eq(mActivity),
                        eq(mProfile),
                        any(FullscreenSigninAndHistorySyncConfig.class),
                        eq(SigninAccessPoint.COLLABORATION_TAB_GROUP));
        doReturn(requestCode)
                .when(mWindowAndroid)
                .showCancelableIntent(eq(intent), any(IntentCallback.class), eq(null));

        mCollaborationControllerDelegateImpl.prepareFlowUI(exitCallback, prepareResultCallback);
        mCollaborationControllerDelegateImpl.showAuthenticationUi(authenticationResultCallback);

        // Simulate exiting flow from CollaborationService.
        mCollaborationControllerDelegateImpl.onFlowFinished();
        mCollaborationControllerDelegateImpl.clearNativePtr();

        // Running flow should be destroyed.
        verify(mWindowAndroid).cancelIntent(eq(requestCode));
        // Delegate should be deleted from TabManager.
        verify(mDataSharingTabManager).onCollaborationDelegateFlowFinished();
        // Exit callback should be deleted.
        verify(mCollaborationControllerDelegateImplNativeMock).deleteExitCallback(eq(exitCallback));
        verify(mLoadingFullscreenCoordinator).closeLoadingScreen();
    }

    @Test
    public void testExitFlowFromActivity() {
        createDelegate(FlowType.JOIN);
        long prepareResultCallback = 1;
        long exitCallback = 2;

        mCollaborationControllerDelegateImpl.prepareFlowUI(exitCallback, prepareResultCallback);

        // Simulate exiting flow activity destroy.
        mCollaborationControllerDelegateImpl.destroy();

        // Exit callback should be ran.
        verify(mCollaborationControllerDelegateImplNativeMock).runExitCallback(eq(exitCallback));
    }

    @Test
    public void testPromoteTabGroup() {
        createDelegate(FlowType.JOIN);
        long resultCallback = 1;
        String collaborationId = "collaboration";

        mCollaborationControllerDelegateImpl.promoteTabGroup(collaborationId, resultCallback);
        verify(mDataSharingTabManager).promoteTabGroup(collaborationId);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), anyLong());
    }

    @Test
    public void testShowJoinDialog() {
        createDelegate(FlowType.JOIN);
        long resultCallback = 1;
        GroupToken token = new GroupToken(COLLABORATION_ID1, /* accessToken= */ "");
        org.chromium.components.sync.protocol.GroupData groupData =
                org.chromium.components.sync.protocol.GroupData.newBuilder()
                        .setGroupId(COLLABORATION_ID1)
                        .build();
        SharedTabGroupPreview previewData =
                new SharedTabGroupPreview(/* title= */ "", new ArrayList<TabPreview>());

        mCollaborationControllerDelegateImpl.showJoinDialog(token, previewData, resultCallback);

        ArgumentCaptor<DataSharingJoinUiConfig> joinConfigCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.class);
        verify(mDataSharingUiDelegate).showJoinFlow(joinConfigCaptor.capture());

        joinConfigCaptor.getValue().getJoinCallback().onGroupJoinedWithWait(groupData, null);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));

        joinConfigCaptor.getValue().getJoinCallback().onSessionFinished();
        verify(mCollaborationControllerDelegateImplNativeMock, never())
                .runResultCallback(eq(Outcome.CANCEL), eq(resultCallback));
    }

    @Test
    public void testShowJoinDialogCancel() {
        createDelegate(FlowType.JOIN);
        long resultCallback = 1;
        GroupToken token = new GroupToken(COLLABORATION_ID1, /* accessToken= */ "");
        SharedTabGroupPreview previewData =
                new SharedTabGroupPreview(/* title= */ "", new ArrayList<TabPreview>());

        mCollaborationControllerDelegateImpl.showJoinDialog(token, previewData, resultCallback);

        ArgumentCaptor<DataSharingJoinUiConfig> joinConfigCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.class);
        verify(mDataSharingUiDelegate).showJoinFlow(joinConfigCaptor.capture());

        joinConfigCaptor.getValue().getJoinCallback().onSessionFinished();
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.CANCEL), eq(resultCallback));
    }

    @Test
    public void testShowManageDialog() {
        createDelegate(FlowType.SHARE_OR_MANAGE);
        long resultCallback = 1;

        doReturn(mSavedTabGroup)
                .when(mDataSharingTabManager)
                .getSavedTabGroupForEitherId(SYNC_GROUP_ID1, null);
        mCollaborationControllerDelegateImpl.showManageDialog(SYNC_GROUP_ID1, null, resultCallback);

        ArgumentCaptor<DataSharingManageUiConfig> manageConfigCaptor =
                ArgumentCaptor.forClass(DataSharingManageUiConfig.class);
        verify(mDataSharingUiDelegate).showManageFlow(manageConfigCaptor.capture());

        manageConfigCaptor.getValue().getManageCallback().onSessionFinished();
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }

    @Test
    public void testShowShareDialog() {
        createDelegate(FlowType.SHARE_OR_MANAGE);
        long resultCallback = 1;
        String accessToken = "accessToken";
        GURL url = new GURL("url");

        mCollaborationControllerDelegateImpl
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);

        doReturn(mSavedTabGroup)
                .when(mDataSharingTabManager)
                .getSavedTabGroupForEitherId(null, LOCAL_ID);
        mCollaborationControllerDelegateImpl.showShareDialog(null, LOCAL_ID, resultCallback);

        ArgumentCaptor<DataSharingCreateUiConfig> createConfigCaptor =
                ArgumentCaptor.forClass(DataSharingCreateUiConfig.class);
        verify(mDataSharingUiDelegate).showCreateFlow(createConfigCaptor.capture());

        org.chromium.components.sync.protocol.GroupData groupData =
                org.chromium.components.sync.protocol.GroupData.newBuilder()
                        .setGroupId(COLLABORATION_ID1)
                        .setAccessToken(accessToken)
                        .build();

        createConfigCaptor
                .getValue()
                .getCreateCallback()
                .onGroupCreatedWithWait(groupData, mCloseScreenCallback);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultWithGroupTokenCallback(
                        eq(Outcome.SUCCESS),
                        eq(COLLABORATION_ID1),
                        eq(accessToken),
                        eq(resultCallback));

        ArgumentCaptor<Callback<Boolean>> onFinishCallback =
                ArgumentCaptor.forClass(Callback.class);
        mCollaborationControllerDelegateImpl.onUrlReadyToShare(
                COLLABORATION_ID1, url, resultCallback);
        verify(mDataSharingTabManager)
                .showShareSheet(
                        eq(mActivity), eq(COLLABORATION_ID1), eq(url), onFinishCallback.capture());

        onFinishCallback.getValue().onResult(true);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
        verify(mCloseScreenCallback).onResult(true);
    }

    @Test
    public void testShowError() {
        createDelegate(FlowType.JOIN);
        long resultCallback = 1;
        String title = "title";
        String message = "message";

        mCollaborationControllerDelegateImpl.showError(title, message, resultCallback);
        verify(mLoadingFullscreenCoordinator).closeLoadingScreen();

        ArgumentCaptor<PropertyModel> propertyModelCaptor =
                ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(propertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                propertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager).dismissDialog(any(), anyInt());

        controller.onDismiss(propertyModelCaptor.getValue(), DialogDismissalCause.NAVIGATE);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }
}
