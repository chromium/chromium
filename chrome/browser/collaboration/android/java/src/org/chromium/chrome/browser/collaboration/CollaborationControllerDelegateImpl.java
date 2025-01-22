// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import android.app.Activity;
import android.content.Intent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.DataSharingMetrics;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.Outcome;
import org.chromium.components.collaboration.Type;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** An interface to manage collaboration flow UI screens. */
@JNINamespace("collaboration")
public class CollaborationControllerDelegateImpl implements CollaborationControllerDelegate {
    private final @FlowType int mFlowType;
    private Activity mActivity;
    private DataSharingTabManager mDataSharingTabManager;
    private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private long mExitCallback;
    private long mNativePtr;

    // Stores the runnable to close the current showing UI. Is null when there's no UI showing.
    private Runnable mCloseScreenRunnable;

    public CollaborationControllerDelegateImpl(
            Activity activity,
            @FlowType int type,
            DataSharingTabManager tabManager,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher) {
        mNativePtr = CollaborationControllerDelegateImplJni.get().createNativeObject(this);

        mActivity = activity;
        mFlowType = type;
        mDataSharingTabManager = tabManager;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
    }

    @Override
    public long getNativePtr() {
        return mNativePtr;
    }

    /**
     * Prepare and wait for the UI to be ready to be shown.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void prepareFlowUI(long exitCallback, long resultCallback) {
        mExitCallback = exitCallback;
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.SUCCESS, resultCallback);
    }

    /**
     * Show an error dialog.
     *
     * @param errorType The type of error.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showError(@Type int errorType, long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Cancel and hide the UI.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void cancel(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Show the sign-in and sync authentication screens.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showAuthenticationUi(long resultCallback) {
        assert mDataSharingTabManager.getProfile() != null;

        Intent intent = null;
        switch (mFlowType) {
            case FlowType.JOIN:
                // TODO(haileywang): Add the correct logo: .signinLogoId(R.drawable.signin_logo).
                FullscreenSigninAndHistorySyncConfig fullscreenConfig =
                        new FullscreenSigninAndHistorySyncConfig.Builder()
                                .historyOptInMode(HistorySyncConfig.OptInMode.REQUIRED)
                                .signinTitleId(R.string.collaboration_signin_title)
                                .signinSubtitleId(R.string.collaboration_signin_description)
                                .signinDismissTextId(R.string.collaboration_signin_sync_dismiss)
                                .historySyncTitleId(R.string.collaboration_sync_title)
                                .historySyncSubtitleId(R.string.collaboration_sync_description)
                                .build();

                intent =
                        mSigninAndHistorySyncActivityLauncher
                                .createFullscreenSigninIntentOrShowError(
                                        mActivity,
                                        mDataSharingTabManager.getProfile(),
                                        fullscreenConfig,
                                        SigninAccessPoint.COLLABORATION_TAB_GROUP);
                break;
            case FlowType.SHARE_OR_MANAGE:
                AccountPickerBottomSheetStrings strings =
                        new AccountPickerBottomSheetStrings.Builder(
                                        R.string.collaboration_signin_bottom_sheet_title)
                                .setSubtitleStringId(
                                        R.string.collaboration_signin_bottom_sheet_description)
                                .build();

                BottomSheetSigninAndHistorySyncConfig bottomSheetConfig =
                        new BottomSheetSigninAndHistorySyncConfig.Builder(
                                        strings,
                                        NoAccountSigninMode.BOTTOM_SHEET,
                                        WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                        HistorySyncConfig.OptInMode.REQUIRED)
                                .build();
                intent =
                        mSigninAndHistorySyncActivityLauncher
                                .createBottomSheetSigninIntentOrShowError(
                                        mActivity,
                                        mDataSharingTabManager.getProfile(),
                                        bottomSheetConfig,
                                        SigninAccessPoint.COLLABORATION_TAB_GROUP);
                break;
            default:
                assert false;
        }

        if (intent == null) {
            CollaborationControllerDelegateImplJni.get()
                    .runResultCallback(Outcome.FAILURE, resultCallback);
            return;
        }
        int requestCode =
                mDataSharingTabManager
                        .getWindowAndroid()
                        .showCancelableIntent(
                                intent,
                                (resultCode, data) -> onSigninResult(resultCode, resultCallback),
                                /* errorId= */ null);

        mCloseScreenRunnable =
                () -> {
                    mDataSharingTabManager.getWindowAndroid().cancelIntent(requestCode);
                };
    }

    private void onSigninResult(int resultCode, long resultCallback) {
        mCloseScreenRunnable = null;
        if (resultCode == Activity.RESULT_OK) {
            CollaborationControllerDelegateImplJni.get()
                    .runResultCallback(Outcome.SUCCESS, resultCallback);
            return;
        }
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.CANCEL, resultCallback);
    }

    /** Notify that the primary account has been modified. */
    @CalledByNative
    void notifySignInAndSyncStatusChange() {}

    /**
     * Show the join dialog screen.
     *
     * @param token Group id and token secret of the current join request.
     * @param previewTabData Preview of shared tab group data.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showJoinDialog(GroupToken token, SharedTabGroupPreview previewData, long resultCallback) {
        if (previewData == null) {
            CollaborationControllerDelegateImplJni.get()
                    .runResultCallback(Outcome.FAILURE, resultCallback);
            return;
        }

        DataSharingJoinUiConfig.JoinCallback joinCallback =
                new DataSharingJoinUiConfig.JoinCallback() {
                    private long mResultCallback;

                    {
                        mResultCallback = resultCallback;
                    }

                    @Override
                    public void onGroupJoinedWithWait(
                            org.chromium.components.sync.protocol.GroupData groupData,
                            Callback<Boolean> onJoinFinished) {
                        DataSharingMetrics.recordJoinActionFlowState(
                                DataSharingMetrics.JoinActionStateAndroid.ADD_MEMBER_SUCCESS);
                        assert groupData.getGroupId().equals(token.collaborationId);
                        mCloseScreenRunnable =
                                () -> {
                                    onJoinFinished.onResult(true);
                                };
                        long callback = mResultCallback;
                        mResultCallback = 0;
                        CollaborationControllerDelegateImplJni.get()
                                .runResultCallback(Outcome.SUCCESS, callback);
                    }

                    @Override
                    public void onSessionFinished() {
                        mCloseScreenRunnable = null;
                        if (mResultCallback != 0) {
                            CollaborationControllerDelegateImplJni.get()
                                    .runResultCallback(Outcome.CANCEL, mResultCallback);
                        }
                    }
                };

        String sessionId =
                mDataSharingTabManager.showJoinScreenWithPreview(
                        mActivity, token, previewData, joinCallback);

        mCloseScreenRunnable =
                () -> {
                    mDataSharingTabManager.getUiDelegate().destroyFlow(sessionId);
                };
    }

    /**
     * Show the share dialog screen.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showShareDialog(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Show the manage dialog screen.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showManageDialog(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Open and show the local tab group.
     *
     * @param collaborationId The collaboration id of the tab group to promote.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void promoteTabGroup(String collaborationId, long resultCallback) {
        closeScreenIfNeeded();
        mDataSharingTabManager.promoteTabGroup(collaborationId);
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.SUCCESS, resultCallback);
    }

    /** Focus and show the current flow screen. */
    @CalledByNative
    void promoteCurrentScreen() {}

    /** Called when the flow is finished. */
    @CalledByNative
    void onFlowFinished() {
        // Destroy currently showing UI if any.
        closeScreenIfNeeded();
        mDataSharingTabManager.onCollaborationDelegateFlowFinished();
        cleanUpPointers();

        if (mExitCallback != 0) {
            CollaborationControllerDelegateImplJni.get().deleteExitCallback(mExitCallback);
        }
    }

    /** It is guaranteed that onFlowFinished() is called before this function. */
    @CalledByNative
    void clearNativePtr() {
        mNativePtr = 0;
        assert mActivity == null;
    }

    /** Cleans up any outstanding resources. */
    @Override
    public void destroy() {
        long tempCallback = mExitCallback;
        mExitCallback = 0;
        CollaborationControllerDelegateImplJni.get().runExitCallback(tempCallback);
    }

    private void cleanUpPointers() {
        mActivity = null;
        mDataSharingTabManager = null;
        mSigninAndHistorySyncActivityLauncher = null;
    }

    private void closeScreenIfNeeded() {
        if (mCloseScreenRunnable != null) {
            mCloseScreenRunnable.run();
            mCloseScreenRunnable = null;
        }
    }

    @NativeMethods
    interface Natives {
        void runResultCallback(int joutcome, long resultCallback);

        void runExitCallback(long exitCallback);

        void deleteExitCallback(long exitCallback);

        long createNativeObject(CollaborationControllerDelegateImpl jdelegate);
    }
}
