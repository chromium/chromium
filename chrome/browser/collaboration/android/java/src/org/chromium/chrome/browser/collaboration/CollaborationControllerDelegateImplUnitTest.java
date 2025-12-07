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
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
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
import org.chromium.components.collaboration.error_info.Type;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
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

/** Unit test for {@link CollaborationControllerDelegateImpl} */
@RunWith(BaseRobolectricTestRunner.class)
public class CollaborationControllerDelegateImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private DataSharingTabManager mDataSharingTabManager;
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
    @Mock private Tracker mMockTracker;

    @Mock
    private CollaborationControllerDelegateImpl.Natives
            mCollaborationControllerDelegateImplNativeMock;

    private CollaborationControllerDelegateImpl mCollaborationControllerDelegateImpl;
    private Activity mActivity;

    @Before
    public void setUp() {
        CollaborationControllerDelegateImplJni.setInstanceForTesting(
                mCollaborationControllerDelegateImplNativeMock);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        TrackerFactory.setTrackerForTests(mMockTracker);

        doReturn(
                        new ServiceStatus(
                                /* signinStatus= */ 0,
                                /* syncStatus= */ 0,
                                /* collaborationStatus= */ 0))
                .when(mCollaborationService)
                .getServiceStatus();
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        doReturn(true).when(mSigninManager).isSigninAllowed();
        doReturn(0L).when(mCollaborationControllerDelegateImplNativeMock).createNativeObject(any());
        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
        doReturn(mProfile).when(mDataSharingTabManager).getProfile();
        doReturn(mWindowAndroid).when(mDataSharingTabManager).getWindowAndroid();
        doReturn(mModalDialogManager).when(mWindowAndroid).getModalDialogManager();
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
            verify(mLoadingFullscreenCoordinator).startLoading(any(), eq(false));
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
        verify(mLoadingFullscreenCoordinator).startLoading(any(), eq(false));

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
                        eq(SigninAccessPoint.COLLABORATION_JOIN_TAB_GROUP));

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

        doReturn(
                        new ServiceStatus(
                                SigninStatus.SIGNIN_DISABLED,
                                /* syncStatus= */ 0,
                                /* collaborationStatus= */ 0))
                .when(mCollaborationService)
                .getServiceStatus();

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
                        eq(SigninAccessPoint.COLLABORATION_SHARE_TAB_GROUP));

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
                        eq(SigninAccessPoint.COLLABORATION_JOIN_TAB_GROUP));
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
        when(mDataSharingTabManager.displayTabGroupAnywhere(
                        collaborationId, /* isFromInviteFlow= */ true))
                .thenReturn(true);

        mCollaborationControllerDelegateImpl.promoteTabGroup(collaborationId, resultCallback);

        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), anyLong());
    }

    @Test
    public void testShowJoinDialog() {
        createDelegate(FlowType.JOIN);
        String collabId = "Collaboration";
        long resultCallback = 1;
        GroupToken token = new GroupToken(collabId, /* accessToken= */ "");
        org.chromium.components.sync.protocol.GroupData groupData =
                org.chromium.components.sync.protocol.GroupData.newBuilder()
                        .setGroupId(collabId)
                        .build();
        SharedTabGroupPreview previewData =
                new SharedTabGroupPreview(/* title= */ "", /* tabs= */ null);

        mCollaborationControllerDelegateImpl.showJoinDialog(token, previewData, resultCallback);

        ArgumentCaptor<DataSharingJoinUiConfig.JoinCallback> joinCallbackCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.JoinCallback.class);
        verify(mDataSharingTabManager)
                .showJoinScreenWithPreview(
                        eq(mActivity),
                        eq(token),
                        eq(previewData),
                        anyLong(),
                        joinCallbackCaptor.capture());

        joinCallbackCaptor.getValue().onGroupJoinedWithWait(groupData, null);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));

        joinCallbackCaptor.getValue().onSessionFinished();
        verify(mCollaborationControllerDelegateImplNativeMock, never())
                .runResultCallback(eq(Outcome.CANCEL), eq(resultCallback));
    }

    @Test
    public void testShowJoinDialogCancel() {
        createDelegate(FlowType.JOIN);
        String collabId = "Collaboration";
        long resultCallback = 1;
        GroupToken token = new GroupToken(collabId, /* accessToken= */ "");
        SharedTabGroupPreview previewData =
                new SharedTabGroupPreview(/* title= */ "", /* tabs= */ null);

        mCollaborationControllerDelegateImpl.showJoinDialog(token, previewData, resultCallback);

        ArgumentCaptor<DataSharingJoinUiConfig.JoinCallback> joinCallbackCaptor =
                ArgumentCaptor.forClass(DataSharingJoinUiConfig.JoinCallback.class);
        verify(mDataSharingTabManager)
                .showJoinScreenWithPreview(
                        eq(mActivity),
                        eq(token),
                        eq(previewData),
                        anyLong(),
                        joinCallbackCaptor.capture());

        joinCallbackCaptor.getValue().onSessionFinished();
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.CANCEL), eq(resultCallback));
    }

    @Test
    public void testShowManageDialog() {
        createDelegate(FlowType.SHARE_OR_MANAGE);
        long resultCallback = 1;
        String syncId = "syncId";
        String collaborationId = "collaborationId";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.collaborationId = collaborationId;

        doReturn(savedGroup).when(mDataSharingTabManager).getSavedTabGroupForEitherId(syncId, null);
        mCollaborationControllerDelegateImpl.showManageDialog(syncId, null, resultCallback);
        ArgumentCaptor<DataSharingManageUiConfig.ManageCallback> manageCallbackCaptor =
                ArgumentCaptor.forClass(DataSharingManageUiConfig.ManageCallback.class);

        verify(mDataSharingTabManager)
                .showManageSharing(
                        eq(mActivity), eq(collaborationId), manageCallbackCaptor.capture());

        manageCallbackCaptor.getValue().onSessionFinished();
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }

    @Test
    public void testShowShareDialog() {
        createDelegate(FlowType.SHARE_OR_MANAGE);
        long resultCallback = 1;
        LocalTabGroupId localId = new LocalTabGroupId(new Token(1L, 2L));
        String collaborationId = "collaborationId";
        String sessionId = "sessionId";
        String accessToken = "accessToken";
        String title = "title";
        GURL url = new GURL("url");

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = localId;
        savedGroup.collaborationId = collaborationId;
        savedGroup.title = title;

        doReturn(savedGroup)
                .when(mDataSharingTabManager)
                .getSavedTabGroupForEitherId(null, localId);
        ArgumentCaptor<DataSharingCreateUiConfig.CreateCallback> createCallbackCaptor =
                ArgumentCaptor.forClass(DataSharingCreateUiConfig.CreateCallback.class);
        doReturn(sessionId)
                .when(mDataSharingTabManager)
                .showShareDialog(
                        eq(mActivity), eq(title), eq(savedGroup), createCallbackCaptor.capture());

        mCollaborationControllerDelegateImpl.showShareDialog(null, localId, resultCallback);
        org.chromium.components.sync.protocol.GroupData groupData =
                org.chromium.components.sync.protocol.GroupData.newBuilder()
                        .setGroupId(collaborationId)
                        .setAccessToken(accessToken)
                        .build();

        createCallbackCaptor.getValue().onGroupCreatedWithWait(groupData, mCloseScreenCallback);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultWithGroupTokenCallback(
                        eq(Outcome.SUCCESS),
                        eq(collaborationId),
                        eq(accessToken),
                        eq(resultCallback));

        ArgumentCaptor<Callback<Boolean>> onFinishCallback =
                ArgumentCaptor.forClass(Callback.class);
        mCollaborationControllerDelegateImpl.onUrlReadyToShare(
                collaborationId, url, resultCallback);
        verify(mDataSharingTabManager)
                .showShareSheet(
                        eq(mActivity),
                        eq(collaborationId),
                        eq(sessionId),
                        eq(url),
                        onFinishCallback.capture());

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

        mCollaborationControllerDelegateImpl.showError(
                Type.UNKNOWN, title, message, resultCallback);
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

    @Test
    public void testLeaveSharedTabGroup() {
        createDelegate(FlowType.LEAVE_OR_DELETE);
        long resultCallback = 1;
        LocalTabGroupId localId = new LocalTabGroupId(new Token(1L, 2L));
        String title = "title";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = localId;
        savedGroup.title = title;
        doReturn(savedGroup)
                .when(mDataSharingTabManager)
                .getSavedTabGroupForEitherId(null, localId);

        mCollaborationControllerDelegateImpl.showLeaveDialog(null, localId, resultCallback);
        ArgumentCaptor<PropertyModel> propertyModelCaptor =
                ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(propertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                propertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModelCaptor.getValue(), ButtonType.NEGATIVE);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.CANCEL), eq(resultCallback));
    }

    @Test
    public void testDeleteSharedTabGroup() {
        createDelegate(FlowType.LEAVE_OR_DELETE);
        long resultCallback = 1;
        LocalTabGroupId localId = new LocalTabGroupId(new Token(1L, 2L));
        String title = "title";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.localId = localId;
        savedGroup.title = title;
        doReturn(savedGroup)
                .when(mDataSharingTabManager)
                .getSavedTabGroupForEitherId(null, localId);

        mCollaborationControllerDelegateImpl.showLeaveDialog(null, localId, resultCallback);
        ArgumentCaptor<PropertyModel> propertyModelCaptor =
                ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(propertyModelCaptor.capture(), anyInt());

        ModalDialogProperties.Controller controller =
                propertyModelCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(propertyModelCaptor.getValue(), ButtonType.POSITIVE);
        verify(mCollaborationControllerDelegateImplNativeMock)
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }
}
