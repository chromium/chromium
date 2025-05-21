// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.DataSharingMetrics;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
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
import org.chromium.components.feature_engagement.Tracker;
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
@NullMarked
@JNINamespace("collaboration")
public class CollaborationControllerDelegateImpl implements CollaborationControllerDelegate {
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();
    private final @FlowType int mFlowType;
    private Activity mActivity;
    private DataSharingTabManager mDataSharingTabManager;
    private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private long mExitCallback;
    private long mNativePtr;
    private @Nullable LoadingFullscreenCoordinator mLoadingFullscreenCoordinator;

    // Will become null once used in the prepareFlowUI().
    private @Nullable Callback<Runnable> mSwitchToTabSwitcherCallback;

    private final Callback<Callback<Boolean>> mStartAccountRefreshCallback;

    // Stores the runnable to close the current showing UI. Is null when there's no UI showing.
    private @Nullable Runnable mCloseScreenRunnable;

    /** Used to suppress IPH UIs while a collaboration flow UI is on the screen. */
    private Tracker.@Nullable DisplayLockHandle mFeatureEngagementLock;

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
        mThreadChecker.assertOnValidThread();

        mActivity = activity;
        mFlowType = type;
        mDataSharingTabManager = tabManager;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        mLoadingFullscreenCoordinator = loadingFullscreenCoordinator;
        mSwitchToTabSwitcherCallback = switchToTabSwitcherCallback;
        mStartAccountRefreshCallback = startAccountRefreshCallback;

        if (mFlowType == FlowType.JOIN) {
            // The screen should not animate in order to hide all ongoing transitions immediately
            // after this call.
            loadingFullscreenCoordinator.startLoading(this::destroy, /* animate= */ false);
        }
    }

    @Override
    public long getNativePtr() {
        mThreadChecker.assertOnValidThread();
        return mNativePtr;
    }

    /**
     * Prepare and wait for the UI to be ready to be shown.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void prepareFlowUI(long exitCallback, long resultCallback) {
        mThreadChecker.assertOnValidThread();
        mExitCallback = exitCallback;

        // Acquire lock to prevent IPH from being shown in a collaboration flow.
        Tracker tracker = TrackerFactory.getTrackerForProfile(mDataSharingTabManager.getProfile());
        mFeatureEngagementLock = tracker.acquireDisplayLock();

        Runnable onTabSwitcherShownRunnable =
                () -> {
                    mThreadChecker.assertOnValidThread();
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
        mThreadChecker.assertOnValidThread();
        @Nullable ModalDialogManager modalDialogManager =
                mDataSharingTabManager.getWindowAndroid().getModalDialogManager();
        assert modalDialogManager != null;

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, @ButtonType int buttonType) {
                        mThreadChecker.assertOnValidThread();
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        mThreadChecker.assertOnValidThread();
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
                    mThreadChecker.assertOnValidThread();
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
        mThreadChecker.assertOnValidThread();
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
        mThreadChecker.assertOnValidThread();
        Profile profile = mDataSharingTabManager.getProfile();
        assert profile != null;

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);

        ServiceStatus serviceStatus =
                CollaborationServiceFactory.getForProfile(profile).getServiceStatus();

        if (serviceStatus.signinStatus == SigninStatus.SIGNIN_DISABLED) {
            // The signin option is disabled manually by the user in settings.
            openSigninSettingsModel(resultCallback);
            return;
        }

        if (serviceStatus.signinStatus == SigninStatus.SIGNED_IN_PAUSED) {
            // Need to redirect to verify account activity.
            Callback<Boolean> successCallback =
                    (success) -> {
                        mThreadChecker.assertOnValidThread();
                        @Outcome int outcome = success ? Outcome.SUCCESS : Outcome.CANCEL;

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
            case FlowType.LEAVE_OR_DELETE:
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
        if (mFlowType == FlowType.JOIN) {
            // Animate in the sign in screen.
            mActivity.overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
        }

        mCloseScreenRunnable =
                () -> {
                    mThreadChecker.assertOnValidThread();
                    mDataSharingTabManager.getWindowAndroid().cancelIntent(requestCode);
                };
    }

    private void openSigninSettingsModel(long resultCallback) {
        mThreadChecker.assertOnValidThread();
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
                        mThreadChecker.assertOnValidThread();
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
                        mThreadChecker.assertOnValidThread();
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
                    mThreadChecker.assertOnValidThread();
                    modalDialogManager.dismissDialog(model, DialogDismissalCause.NAVIGATE);
                };
    }

    private @Nullable Intent createBottomSheetSigninIntent() {
        mThreadChecker.assertOnValidThread();
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
        @SigninAccessPoint int accessPoint;
        if (mFlowType == FlowType.SHARE_OR_MANAGE) {
            accessPoint = SigninAccessPoint.COLLABORATION_SHARE_TAB_GROUP;
        } else {
            accessPoint = SigninAccessPoint.COLLABORATION_LEAVE_OR_DELETE_TAB_GROUP;
        }
        return mSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                mActivity,
                assumeNonNull(mDataSharingTabManager.getProfile()),
                bottomSheetConfig,
                accessPoint);
    }

    private @Nullable Intent createFullscreenSigninIntent() {
        mThreadChecker.assertOnValidThread();
        FullscreenSigninAndHistorySyncConfig fullscreenConfig =
                new FullscreenSigninAndHistorySyncConfig.Builder()
                        .historyOptInMode(HistorySyncConfig.OptInMode.REQUIRED)
                        .signinTitleId(R.string.collaboration_signin_title)
                        .signinSubtitleId(R.string.collaboration_signin_description)
                        .signinDismissTextId(R.string.collaboration_signin_sync_dismiss)
                        .signinLogoId(R.drawable.signin_logo)
                        .historySyncTitleId(R.string.collaboration_sync_title)
                        .historySyncSubtitleId(R.string.collaboration_sync_description)
                        .build();

        return mSigninAndHistorySyncActivityLauncher.createFullscreenSigninIntentOrShowError(
                mActivity,
                assumeNonNull(mDataSharingTabManager.getProfile()),
                fullscreenConfig,
                SigninAccessPoint.COLLABORATION_JOIN_TAB_GROUP);
    }

    private void onSigninResult(int resultCode, long resultCallback) {
        mThreadChecker.assertOnValidThread();
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
        mThreadChecker.assertOnValidThread();
        if (previewData == null) {
            CollaborationControllerDelegateImplJni.get()
                    .runResultCallback(Outcome.FAILURE, resultCallback);
            return;
        }

        DataSharingJoinUiConfig.JoinCallback joinCallback =
                new DataSharingJoinUiConfig.JoinCallback() {
                    // mThreadChecker is an instance variable of the outer class.
                    private long mResultCallback;

                    {
                        mResultCallback = resultCallback;
                    }

                    @Override
                    public void onGroupJoinedWithWait(
                            org.chromium.components.sync.protocol.GroupData groupData,
                            Callback<Boolean> onJoinFinished) {
                        mThreadChecker.assertOnValidThread();
                        DataSharingMetrics.recordJoinActionFlowState(
                                DataSharingMetrics.JoinActionStateAndroid.ADD_MEMBER_SUCCESS);
                        assert groupData.getGroupId().equals(token.collaborationId);
                        mCloseScreenRunnable =
                                () -> {
                                    onJoinFinished.onResult(true);
                                    mThreadChecker.assertOnValidThread();
                                };
                        long callback = mResultCallback;
                        mResultCallback = 0;
                        CollaborationControllerDelegateImplJni.get()
                                .runResultCallback(Outcome.SUCCESS, callback);
                    }

                    @Override
                    public void onSessionFinished() {
                        mThreadChecker.assertOnValidThread();
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
        assumeNonNull(sessionId);

        mCloseScreenRunnable =
                () -> {
                    mThreadChecker.assertOnValidThread();
                    assumeNonNull(mDataSharingTabManager.getUiDelegate()).destroyFlow(sessionId);
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
        mThreadChecker.assertOnValidThread();
        DataSharingCreateUiConfig.CreateCallback createCallback =
                new DataSharingCreateUiConfig.CreateCallback() {
                    // mThreadChecker is an instance variable of the outer class.
                    private long mResultCallback;

                    {
                        mResultCallback = resultWithGroupTokenCallback;
                    }

                    @Override
                    public void onGroupCreatedWithWait(
                            org.chromium.components.sync.protocol.GroupData result,
                            Callback<Boolean> onCreateFinished) {
                        mThreadChecker.assertOnValidThread();
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.GROUP_CREATE_SUCCESS);
                        mCloseScreenRunnable =
                                () -> {
                                    mThreadChecker.assertOnValidThread();
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
                        mThreadChecker.assertOnValidThread();
                        DataSharingMetrics.recordShareActionFlowState(
                                DataSharingMetrics.ShareActionStateAndroid.BOTTOM_SHEET_DISMISSED);
                    }

                    @Override
                    public void onSessionFinished() {
                        mThreadChecker.assertOnValidThread();
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
        assumeNonNull(sessionId);

        mCloseScreenRunnable =
                () -> {
                    mThreadChecker.assertOnValidThread();
                    assumeNonNull(mDataSharingTabManager.getUiDelegate()).destroyFlow(sessionId);
                };
    }

    /**
     * Show the system share sheet.
     *
     * @param groupToken The associated group token.
     */
    @CalledByNative
    void onUrlReadyToShare(String groupId, GURL url, long resultCallback) {
        mThreadChecker.assertOnValidThread();
        if (mCloseScreenRunnable == null) return;
        Callback<Boolean> onFinishCallback =
                (result) -> {
                    // Close the share dialog that is waiting to finish.
                    mThreadChecker.assertOnValidThread();
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
        mThreadChecker.assertOnValidThread();
        SavedTabGroup existingGroup =
                mDataSharingTabManager.getSavedTabGroupForEitherId(syncId, localId);

        String sessionId =
                mDataSharingTabManager.showManageSharing(
                        mActivity,
                        assumeNonNull(existingGroup.collaborationId),
                        (outcome) -> {
                            mThreadChecker.assertOnValidThread();
                            CollaborationControllerDelegateImplJni.get()
                                    .runResultCallback(outcome, resultCallback);
                        });
        assumeNonNull(sessionId);

        mCloseScreenRunnable =
                () -> {
                    mThreadChecker.assertOnValidThread();
                    assumeNonNull(mDataSharingTabManager.getUiDelegate()).destroyFlow(sessionId);
                };
    }

    private Callback<MaybeBlockingResult> getActionConfirmationCallback(long resultCallback) {
        mThreadChecker.assertOnValidThread();
        return (MaybeBlockingResult maybeBlockingResult) -> {
            mThreadChecker.assertOnValidThread();
            boolean accept =
                    maybeBlockingResult.result != ActionConfirmationResult.CONFIRMATION_NEGATIVE;

            if (maybeBlockingResult.finishBlocking != null) {
                mCloseScreenRunnable = maybeBlockingResult.finishBlocking;
            }

            if (accept) {
                CollaborationControllerDelegateImplJni.get()
                        .runResultCallback(Outcome.SUCCESS, resultCallback);
            } else {
                CollaborationControllerDelegateImplJni.get()
                        .runResultCallback(Outcome.CANCEL, resultCallback);
            }
        };
    }

    private ActionConfirmationManager getActionConfirmationManager() {
        mThreadChecker.assertOnValidThread();
        return new ActionConfirmationManager(
                assumeNonNull(mDataSharingTabManager.getProfile()),
                mActivity,
                assumeNonNull(mDataSharingTabManager.getWindowAndroid().getModalDialogManager()));
    }

    /**
     * Show the leave dialog screen.
     *
     * @param syncId The sync id of the tab group
     * @param localId The local id of the tab group.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showLeaveDialog(String syncId, LocalTabGroupId localId, long resultCallback) {
        mThreadChecker.assertOnValidThread();
        SavedTabGroup existingGroup =
                mDataSharingTabManager.getSavedTabGroupForEitherId(syncId, localId);

        getActionConfirmationManager()
                .processLeaveGroupAttempt(
                        getSavedTabGroupTitle(existingGroup),
                        getActionConfirmationCallback(resultCallback));
    }

    /**
     * Show the delete dialog screen.
     *
     * @param syncId The sync id of the tab group
     * @param localId The local id of the tab group.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showDeleteDialog(String syncId, LocalTabGroupId localId, long resultCallback) {
        mThreadChecker.assertOnValidThread();
        SavedTabGroup existingGroup =
                mDataSharingTabManager.getSavedTabGroupForEitherId(syncId, localId);

        getActionConfirmationManager()
                .processDeleteSharedGroupAttempt(
                        getSavedTabGroupTitle(existingGroup),
                        getActionConfirmationCallback(resultCallback));
    }

    private String getSavedTabGroupTitle(SavedTabGroup tabGroup) {
        mThreadChecker.assertOnValidThread();
        return TextUtils.isEmpty(tabGroup.title)
                ? TabGroupTitleUtils.getDefaultTitle(mActivity, tabGroup.savedTabs.size())
                : tabGroup.title;
    }

    /**
     * Open and show the local tab group.
     *
     * @param collaborationId The collaboration id of the tab group to promote.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void promoteTabGroup(String collaborationId, long resultCallback) {
        mThreadChecker.assertOnValidThread();
        closeScreenIfNeeded();
        boolean success =
                mDataSharingTabManager.displayTabGroupAnywhere(
                        collaborationId, /* isFromInviteFlow= */ true);
        // TODO(https://crbug.com/415370145): Track outcomes in metrics.
        @Outcome int outcome = success ? Outcome.SUCCESS : Outcome.FAILURE;
        CollaborationControllerDelegateImplJni.get().runResultCallback(outcome, resultCallback);
    }

    /** Focus and show the current flow screen. */
    @CalledByNative
    void promoteCurrentScreen() {}

    /** Called when the flow is finished. */
    @CalledByNative
    void onFlowFinished() {
        mThreadChecker.assertOnValidThread();
        // Destroy currently showing UI if any.
        closeLoadingIfNeeded();
        closeScreenIfNeeded();
        mDataSharingTabManager.onCollaborationDelegateFlowFinished();
        cleanUpPointers();

        if (mFeatureEngagementLock != null) {
            mFeatureEngagementLock.release();
        }
        if (mExitCallback != 0) {
            CollaborationControllerDelegateImplJni.get().deleteExitCallback(mExitCallback);
        }
    }

    /** It is guaranteed that onFlowFinished() is called before this function. */
    @CalledByNative
    void clearNativePtr() {
        mThreadChecker.assertOnValidThread();
        mNativePtr = 0;
        assert mActivity == null;
    }

    /** Cleans up any outstanding resources. */
    @Override
    public void destroy() {
        mThreadChecker.assertOnValidThread();
        long tempCallback = mExitCallback;
        mExitCallback = 0;
        CollaborationControllerDelegateImplJni.get().runExitCallback(tempCallback);
    }

    @SuppressWarnings("NullAway")
    private void cleanUpPointers() {
        mThreadChecker.assertOnValidThread();
        mActivity = null;
        mDataSharingTabManager = null;
        mSigninAndHistorySyncActivityLauncher = null;
        mLoadingFullscreenCoordinator = null;
    }

    private void closeScreenIfNeeded() {
        mThreadChecker.assertOnValidThread();
        if (mCloseScreenRunnable != null) {
            mCloseScreenRunnable.run();
            mCloseScreenRunnable = null;
        }
    }

    private void closeLoadingIfNeeded() {
        mThreadChecker.assertOnValidThread();
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
                @Nullable String groupId,
                @Nullable String accessToken,
                long resultWithGroupTokenCallback);

        long createNativeObject(CollaborationControllerDelegateImpl jdelegate);
    }
}
