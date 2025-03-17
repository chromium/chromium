// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.DataSharingMetrics;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.loading.LoadingFullscreenCoordinator;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.FlowType;
import org.chromium.components.collaboration.Outcome;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.SigninStatus;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** An interface to manage collaboration flow UI screens. */
@JNINamespace("collaboration")
public class CollaborationControllerDelegateImpl implements CollaborationControllerDelegate {
    private final @FlowType int mFlowType;
    private Activity mActivity;
    private DataSharingTabManager mDataSharingTabManager;
    private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private long mExitCallback;
    private long mNativePtr;
    private @Nullable LoadingFullscreenCoordinator mLoadingFullscreenCoordinator;

    // Will become null once used in the prepareFlowUI().
    private @Nullable Callback<Runnable> mSwitchToTabSwitcherCallback;

    private Callback<Callback<Boolean>> mStartAccountRefreshCallback;

    // Stores the runnable to close the current showing UI. Is null when there's no UI showing.
    private Runnable mCloseScreenRunnable;

    /**
     * Constructor for a new {@link CollaborationControllerDelegateImpl} object.
     *
     * @param activity The current tabbed activity.
     * @param type The flow type of the delegate.
     * @param tabManager Handle communication with ShareKit UI.
     * @param signinAndHistorySyncActivityLauncher The launcher of signin UI.
     * @param loadingFullscreenCoordinator Used to start a loading screen.
     * @param switchToTabSwitcherCallback Callback used to show the tab switcher view.
     */
    public CollaborationControllerDelegateImpl(
            Activity activity,
            @FlowType int type,
            DataSharingTabManager tabManager,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            LoadingFullscreenCoordinator loadingFullscreenCoordinator,
            @Nullable Callback<Runnable> switchToTabSwitcherCallback,
            Callback<Callback<Boolean>> startAccountRefreshCallback) {
        mNativePtr = CollaborationControllerDelegateImplJni.get().createNativeObject(this);

        mActivity = activity;
        mFlowType = type;
        mDataSharingTabManager = tabManager;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        mLoadingFullscreenCoordinator = loadingFullscreenCoordinator;
        mSwitchToTabSwitcherCallback = switchToTabSwitcherCallback;
        mStartAccountRefreshCallback = startAccountRefreshCallback;

        if (mFlowType == FlowType.JOIN) {
            loadingFullscreenCoordinator.startLoading(
                    mActivity.getString(R.string.collaboration_loading_text),
                    () -> {
                        destroy();
                    });
        }
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
        Runnable onTabSwitcherShownRunnable =
                () -> {
                    CollaborationControllerDelegateImplJni.get()
                            .runResultCallback(Outcome.SUCCESS, resultCallback);
                };
        if (mFlowType == FlowType.JOIN) {
            // Wait for tab switcher to be shown before launching the join flow. This is to ensure
            // that all necessary tab UI are ready.
            assert mSwitchToTabSwitcherCallback != null;
            mSwitchToTabSwitcherCallback.onResult(onTabSwitcherShownRunnable);
            mSwitchToTabSwitcherCallback = null;
        } else {
            onTabSwitcherShownRunnable.run();
        }
    }

    /**
     * Show an error dialog.
     *
     * @param titleText The title text of the error dialog.
     * @param messageParagraphText the body text of the error dialog.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showError(String titleText, String messageParagraphText, long resultCallback) {
        @Nullable
        ModalDialogManager modalDialogManager =
                mDataSharingTabManager.getWindowAndroid().getModalDialogManager();
        assert modalDialogManager != null;

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, @ButtonType int buttonType) {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        CollaborationControllerDelegateImplJni.get()
                                .runResultCallback(Outcome.SUCCESS, resultCallback);
                    }
                };
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, titleText)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, messageParagraphText)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mActivity.getString(
                                        R.string.data_sharing_invitation_failure_button))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        closeLoadingIfNeeded();
        closeScreenIfNeeded();
        modalDialogManager.showDialog(model, ModalDialogType.APP);

        mCloseScreenRunnable =
                () -> {
                    modalDialogManager.dismissDialog(model, DialogDismissalCause.NAVIGATE);
                };
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
        Profile profile = mDataSharingTabManager.getProfile();
        assert profile != null;

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        ServiceStatus serviceStatus =
                CollaborationServiceFactory.getForProfile(profile).getServiceStatus();

        if (serviceStatus.signinStatus == SigninStatus.NOT_SIGNED_IN
                && !signinManager.isSigninAllowed()) {
            // The signin option is disabled manually by the user in settings.
            openSigninSettingsModel(resultCallback);
            return;
        }

        if (serviceStatus.signinStatus == SigninStatus.SIGNED_IN_PAUSED) {
            // Need to redirect to verify account activity.
            Callback<Boolean> successCallback =
                    (success) -> {
                        @Outcome int outcome = success ? Outcome.SUCCESS : Outcome.FAILURE;

                        CollaborationControllerDelegateImplJni.get()
                                .runResultCallback(outcome, resultCallback);
                    };
            mStartAccountRefreshCallback.onResult(successCallback);
            return;
        }

        Intent intent = null;
        switch (mFlowType) {
            case FlowType.JOIN:
                intent = createFullscreenSigninIntent();
                break;
            case FlowType.SHARE_OR_MANAGE:
                intent = createBottomSheetSigninIntent();
                break;
            default:
                assert false;
        }

        if (intent == null) {
            CollaborationControllerDelegateImplJni.get()
                    .runResultCallback(Outcome.CANCEL, resultCallback);
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

    private void openSigninSettingsModel(long resultCallback) {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();

        @Nullable
        ModalDialogManager modalDialogManager =
                mDataSharingTabManager.getWindowAndroid().getModalDialogManager();
        assert modalDialogManager != null;

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, @ButtonType int buttonType) {
                        switch (buttonType) {
                            case ModalDialogProperties.ButtonType.POSITIVE:
                                settingsNavigation.startSettings(
                                        mActivity,
                                        SettingsNavigation.SettingsFragment.GOOGLE_SERVICES);
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                                break;
                            default:
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.UNKNOWN);
                        }
                    }

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        CollaborationControllerDelegateImplJni.get()
                                .runResultCallback(Outcome.CANCEL, resultCallback);
                    }
                };
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                mActivity.getString(R.string.collaboration_signed_out_header))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mActivity.getString(R.string.collaboration_signed_out_body))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mActivity.getString(
                                        R.string.collaboration_signed_out_positive_button))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mActivity.getString(R.string.collaboration_signin_sync_dismiss))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        closeLoadingIfNeeded();
        modalDialogManager.showDialog(model, ModalDialogType.APP);

        mCloseScreenRunnable =
                () -> {
                    modalDialogManager.dismissDialog(model, DialogDismissalCause.NAVIGATE);
                };
    }

    private Intent createBottomSheetSigninIntent() {
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.collaboration_signin_bottom_sheet_title)
                        .setSubtitleStringId(R.string.collaboration_signin_bottom_sheet_description)
                        .build();

        BottomSheetSigninAndHistorySyncConfig bottomSheetConfig =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                strings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.REQUIRED)
                        .historySyncTitleId(R.string.collaboration_sync_title)
                        .historySyncSubtitleId(R.string.collaboration_sync_description)
                        .build();
        return mSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                mActivity,
                mDataSharingTabManager.getProfile(),
                bottomSheetConfig,
                SigninAccessPoint.COLLABORATION_TAB_GROUP);
    }

    private Intent createFullscreenSigninIntent() {
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

        return mSigninAndHistorySyncActivityLauncher.createFullscreenSigninIntentOrShowError(
                mActivity,
                mDataSharingTabManager.getProfile(),
                fullscreenConfig,
                SigninAccessPoint.COLLABORATION_TAB_GROUP);
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
     * @param syncId The sync id of the tab group
     * @param localId The local id of the tab group.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showShareDialog(
            String syncId, LocalTabGroupId localId, long resultWithGroupTokenCallback) {
        DataSharingCreateUiConfig.CreateCallback createCallback =
                new DataSharingCreateUiConfig.CreateCallback() {
                    private long mResultCallback;

                    {
                        mResultCallback = resultWithGroupTokenCallback;
                    }

                    @Override
                    public void onGroupCreatedWithWait(
                            org.chromium.components.sync.protocol.GroupData result,
                            Callback<Boolean> onCreateFinished) {
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.GROUP_CREATE_SUCCESS);
                        mCloseScreenRunnable =
                                () -> {
                                    onCreateFinished.onResult(true);
                                };

                        CollaborationControllerDelegateImplJni.get()
                                .runResultWithGroupTokenCallback(
                                        Outcome.SUCCESS,
                                        result.getGroupId(),
                                        result.getAccessToken(),
                                        mResultCallback);
                        mResultCallback = 0;
                    }

                    @Override
                    public void onCancelClicked() {
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.BOTTOM_SHEET_DISMISSED);
                    }

                    @Override
                    public void onSessionFinished() {
                        mCloseScreenRunnable = null;
                        if (mResultCallback != 0) {
                            CollaborationControllerDelegateImplJni.get()
                                    .runResultWithGroupTokenCallback(
                                            Outcome.CANCEL, null, null, mResultCallback);
                        }
                    }
                };

        SavedTabGroup existingGroup =
                mDataSharingTabManager.getSavedTabGroupForEitherId(syncId, localId);

        String sessionId =
                mDataSharingTabManager.showShareDialog(
                        mActivity, existingGroup.title, existingGroup, createCallback);

        mCloseScreenRunnable =
                () -> {
                    mDataSharingTabManager.getUiDelegate().destroyFlow(sessionId);
                };
    }

    /**
     * Show the system share sheet.
     *
     * @param groupToken The associated group token.
     */
    @CalledByNative
    void onUrlReadyToShare(String groupId, GURL url, long resultCallback) {
        if (mCloseScreenRunnable == null) return;
        Callback<Boolean> onFinishCallback =
                (result) -> {
                    // Close the share dialog that is waiting to finish.
                    closeScreenIfNeeded();
                    if (!result) {
                        CollaborationControllerDelegateImplJni.get()
                                .runResultCallback(Outcome.FAILURE, resultCallback);
                    }
                    CollaborationControllerDelegateImplJni.get()
                            .runResultCallback(Outcome.SUCCESS, resultCallback);
                };
        mDataSharingTabManager.showShareSheet(mActivity, groupId, url, onFinishCallback);
    }

    /**
     * Show the manage dialog screen.
     *
     * @param syncId The sync id of the tab group
     * @param localId The local id of the tab group.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showManageDialog(String syncId, LocalTabGroupId localId, long resultCallback) {
        SavedTabGroup existingGroup =
                mDataSharingTabManager.getSavedTabGroupForEitherId(syncId, localId);

        String sessionId =
                mDataSharingTabManager.showManageSharing(
                        mActivity,
                        existingGroup.collaborationId,
                        () -> {
                            CollaborationControllerDelegateImplJni.get()
                                    .runResultCallback(Outcome.SUCCESS, resultCallback);
                        });

        mCloseScreenRunnable =
                () -> {
                    mDataSharingTabManager.getUiDelegate().destroyFlow(sessionId);
                };
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
        closeLoadingIfNeeded();
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
        mLoadingFullscreenCoordinator = null;
    }

    private void closeScreenIfNeeded() {
        if (mCloseScreenRunnable != null) {
            mCloseScreenRunnable.run();
            mCloseScreenRunnable = null;
        }
    }

    private void closeLoadingIfNeeded() {
        if (mLoadingFullscreenCoordinator != null) {
            mLoadingFullscreenCoordinator.closeLoadingScreen();
        }
    }

    @NativeMethods
    interface Natives {
        void runResultCallback(int joutcome, long resultCallback);

        void runExitCallback(long exitCallback);

        void deleteExitCallback(long exitCallback);

        void runResultWithGroupTokenCallback(
                int joutcome,
                String groupId,
                String accessToken,
                long resultWithGroupTokenCallback);

        long createNativeObject(CollaborationControllerDelegateImpl jdelegate);
    }
}
