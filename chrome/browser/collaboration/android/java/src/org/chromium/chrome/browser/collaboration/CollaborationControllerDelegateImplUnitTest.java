// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.Outcome;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

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

    @Mock
    private CollaborationControllerDelegateImpl.Natives
            mCollaborationControllerDelegateImplNativeMock;

    private CollaborationControllerDelegateImpl mCollaborationControllerDelegateImpl;
    private Activity mActivity;

    @Before
    public void setUp() {
        CollaborationControllerDelegateImplJni.setInstanceForTesting(
                mCollaborationControllerDelegateImplNativeMock);

        doReturn((long) 0)
                .when(mCollaborationControllerDelegateImplNativeMock)
                .createNativeObject(any());
        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
        doReturn(mProfile).when(mDataSharingTabManager).getProfile();
        doReturn(mWindowAndroid).when(mDataSharingTabManager).getWindowAndroid();
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
                        mSigninAndHistorySyncActivityLauncher);
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
                .runResultCallback(eq(Outcome.SUCCESS), eq(resultCallback));
    }
}
