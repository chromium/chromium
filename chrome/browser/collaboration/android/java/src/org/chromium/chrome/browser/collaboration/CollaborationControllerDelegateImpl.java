// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.BulkFaviconUtil;
import org.chromium.chrome.browser.data_sharing.DataSharingMetrics;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
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
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.SharedDataPreview;
import org.chromium.components.data_sharing.SharedTabGroupPreview;
import org.chromium.components.data_sharing.TabPreview;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingPreviewDetailsConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.components.data_sharing.configs.DataSharingStringConfig;
import org.chromium.components.data_sharing.configs.DataSharingUiConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** An interface to manage collaboration flow UI screens. */
@JNINamespace("collaboration")
public class CollaborationControllerDelegateImpl implements CollaborationControllerDelegate {
    // TODO(b/405198763): Reuse kLearnAboutBlockedAccountsDefaultUrl.
    private static final String LEARN_MORE_SHARED_TAB_GROUP_PAGE_URL =
            "https://support.google.com/chrome/?p=chrome_collaboration";
    private static final String LEARN_ABOUT_BLOCKED_ACCOUNTS_URL =
            "https://support.google.com/accounts/answer/6388749";
    private static final String ACTIVITY_LOGS_URL =
            "https://myactivity.google.com/product/chrome_shared_tab_group_activity?utm_source=chrome_collab";
    private static final int BITMAP_SIZE = 72;

    private final @FlowType int mFlowType;
    private final BulkFaviconUtil mBulkFaviconUtil = new BulkFaviconUtil();
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

        String sessionId = showJoinScreenWithPreview(token, previewData, joinCallback);

        mCloseScreenRunnable =
                () -> {
                    mDataSharingTabManager.getUiDelegate().destroyFlow(sessionId);
                };
    }

    private String showJoinScreenWithPreview(
            GroupToken token,
            SharedTabGroupPreview previewTabGroupData,
            DataSharingJoinUiConfig.JoinCallback joinCallback) {
        DataSharingStringConfig stringConfig =
                new DataSharingStringConfig.Builder()
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_TITLE,
                                R.plurals.collaboration_preview_dialog_title_multiple)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_TITLE_SINGLE,
                                R.string.collaboration_preview_dialog_title_single)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_DESCRIPTION,
                                R.string.collaboration_preview_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_DETAILS_TITLE,
                                R.string.collaboration_preview_dialog_details_title)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_DETAILS_HEADER,
                                R.string.collaboration_preview_dialog_details_tabs_in_group)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.TABS_COUNT_TITLE,
                                R.plurals.collaboration_preview_dialog_tabs)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_SHARED_TAB_GROUPS,
                                R.string.collaboration_learn_about_shared_groups)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_GROUP_IS_FULL_ERROR_TITLE,
                                R.string.collaboration_group_is_full_error_dialog_header)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.JOIN_GROUP_IS_FULL_ERROR_BODY,
                                R.string.collaboration_group_is_full_error_dialog_body)
                        .build();

        String tabGroupName = previewTabGroupData.title;
        if (TextUtils.isEmpty(tabGroupName)) {
            tabGroupName =
                    TabGroupTitleUtils.getDefaultTitle(mActivity, previewTabGroupData.tabs.size());
        }

        String sessionId =
                mDataSharingTabManager
                        .getUiDelegate()
                        .showJoinFlow(
                                new DataSharingJoinUiConfig.Builder()
                                        .setCommonConfig(
                                                getCommonConfig(tabGroupName, stringConfig))
                                        .setJoinCallback(joinCallback)
                                        .setGroupToken(token)
                                        .setSharedDataPreview(
                                                new SharedDataPreview(previewTabGroupData))
                                        .build());

        fetchFavicons(sessionId, previewTabGroupData.tabs, previewTabGroupData.tabs.size());
        return sessionId;
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
                showShareDialogInternal(existingGroup.title, existingGroup, createCallback);

        mCloseScreenRunnable =
                () -> {
                    mDataSharingTabManager.getUiDelegate().destroyFlow(sessionId);
                };
    }

    private String showShareDialogInternal(
            String tabGroupDisplayName,
            SavedTabGroup existingGroup,
            DataSharingCreateUiConfig.CreateCallback createCallback) {
        DataSharingUIDelegate uiDelegate = mDataSharingTabManager.getUiDelegate();

        if (TextUtils.isEmpty(tabGroupDisplayName)) {
            tabGroupDisplayName =
                    TabGroupTitleUtils.getDefaultTitle(mActivity, existingGroup.savedTabs.size());
        }

        DataSharingStringConfig stringConfig =
                new DataSharingStringConfig.Builder()
                        .setResourceId(
                                DataSharingStringConfig.StringKey.CREATE_TITLE,
                                R.string.collaboration_share_group_title)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.CREATE_DESCRIPTION,
                                R.string.collaboration_share_group_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_SHARED_TAB_GROUPS,
                                R.string.collaboration_learn_about_shared_groups)
                        .build();

        String sessionId =
                uiDelegate.showCreateFlow(
                        new DataSharingCreateUiConfig.Builder()
                                .setCommonConfig(getCommonConfig(tabGroupDisplayName, stringConfig))
                                .setCreateCallback(createCallback)
                                .build());
        fetchFavicons(
                sessionId,
                convertToTabsPreviewList(existingGroup.savedTabs),
                /* maxFaviconsToFetch= */ 4);

        return sessionId;
    }

    private DataSharingUiConfig getCommonConfig(
            String tabGroupName, DataSharingStringConfig stringConfig) {
        DataSharingUiConfig.DataSharingCallback dataSharingCallback =
                new DataSharingUiConfig.DataSharingCallback() {
                    @Override
                    public void onClickOpenChromeCustomTab(Context context, GURL url) {
                        mDataSharingTabManager
                                .getTabGroupsDelegate()
                                .openUrlInChromeCustomTab(context, url);
                    }
                };
        DataSharingUiConfig.Builder commonConfig =
                new DataSharingUiConfig.Builder()
                        .setActivity(mActivity)
                        .setIsTablet(
                                DeviceFormFactor.isWindowOnTablet(
                                        mDataSharingTabManager.getWindowAndroid()))
                        .setLearnMoreHyperLink(getTabGroupHelpUrl())
                        .setDataSharingStringConfig(stringConfig)
                        .setDataSharingCallback(dataSharingCallback);
        if (!TextUtils.isEmpty(tabGroupName)) {
            commonConfig.setTabGroupName(tabGroupName);
        }
        return commonConfig.build();
    }

    private GURL getTabGroupHelpUrl() {
        return new GURL(LEARN_MORE_SHARED_TAB_GROUP_PAGE_URL);
    }

    private GURL getLearnAboutBlockedAccountsUrl() {
        return new GURL(LEARN_ABOUT_BLOCKED_ACCOUNTS_URL);
    }

    private GURL getActivityLogsUrl() {
        return new GURL(ACTIVITY_LOGS_URL);
    }

    private List<TabPreview> convertToTabsPreviewList(List<SavedTabGroupTab> savedTabs) {
        int tabsCount = savedTabs.size();
        List<TabPreview> preview = new ArrayList<>();
        for (int i = 0; i < tabsCount; ++i) {
            // displayUrl field is not used in the create or manage UI where local tab group is
            // available.
            preview.add(new TabPreview(savedTabs.get(i).url, /* displayUrl= */ ""));
        }
        return preview;
    }

    private void fetchFavicons(String sessionId, List<TabPreview> tabs, int maxFaviconsToFetch) {
        // First fetch favicons for up to 4 tabs, then fetch favicons for the remaining tabs.
        int previewImageSize = 4;
        Runnable fetchAll =
                () -> {
                    fetchFaviconsInternal(
                            sessionId,
                            tabs,
                            /* maxTabs= */ maxFaviconsToFetch,
                            () -> {
                                DataSharingMetrics.recordJoinActionFlowState(
                                        DataSharingMetrics.JoinActionStateAndroid
                                                .ALL_FAVICONS_FETCHED);
                            });
                };

        if (tabs.size() <= previewImageSize) {
            fetchAll.run();
            return;
        }
        Runnable onFetched =
                () -> {
                    DataSharingMetrics.recordJoinActionFlowState(
                            DataSharingMetrics.JoinActionStateAndroid.PREVIEW_FAVICONS_FETCHED);
                    fetchAll.run();
                };
        fetchFaviconsInternal(sessionId, tabs, /* maxTabs= */ previewImageSize, onFetched);
    }

    private void fetchFaviconsInternal(
            String sessionId, List<TabPreview> tabs, int maxTabs, Runnable doneCallback) {
        List<GURL> urls = new ArrayList<>();
        List<String> displayUrls = new ArrayList<>();

        // Fetch URLs for favicons (up to maxTabs).
        for (int i = 0; i < Math.min(maxTabs, tabs.size()); i++) {
            urls.add(tabs.get(i).url);
        }

        // Always collect all display URLs.
        for (TabPreview tab : tabs) {
            displayUrls.add(tab.displayUrl);
        }
        mBulkFaviconUtil.fetchAsBitmap(
                mActivity,
                mDataSharingTabManager.getProfile(),
                urls,
                BITMAP_SIZE,
                (favicons) -> {
                    updateFavicons(sessionId, displayUrls, favicons);
                    doneCallback.run();
                });
    }

    private void updateFavicons(String sessionId, List<String> displayUrls, List<Bitmap> favicons) {
        List<DataSharingPreviewDetailsConfig.TabPreview> tabsPreviewList = new ArrayList<>();
        for (int i = 0; i < displayUrls.size(); i++) {
            tabsPreviewList.add(
                    new DataSharingPreviewDetailsConfig.TabPreview(
                            displayUrls.get(i), i < favicons.size() ? favicons.get(i) : null));
        }
        DataSharingRuntimeDataConfig runtimeConfig =
                new DataSharingRuntimeDataConfig.Builder()
                        .setSessionId(sessionId)
                        .setDataSharingPreviewDetailsConfig(
                                new DataSharingPreviewDetailsConfig.Builder()
                                        .setTabPreviews(tabsPreviewList)
                                        .build())
                        .build();
        mDataSharingTabManager.getUiDelegate().updateRuntimeData(sessionId, runtimeConfig);
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
                showManageSharing(
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

    private String showManageSharing(String collaborationId, @Nullable Runnable finishRunnable) {
        Profile profile = mDataSharingTabManager.getProfile();
        assert profile != null;

        DataSharingUIDelegate uiDelegate = mDataSharingTabManager.getUiDelegate();
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        String tabGroupName =
                DataSharingTabGroupUtils.getTabGroupTitle(
                        mActivity, collaborationId, tabGroupSyncService);

        DataSharingStringConfig stringConfig =
                new DataSharingStringConfig.Builder()
                        .setResourceId(
                                DataSharingStringConfig.StringKey.MANAGE_DESCRIPTION,
                                R.string.collaboration_manage_group_description)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LET_ANYONE_JOIN_DESCRIPTION,
                                R.string.collaboration_manage_share_wisely)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_SHARED_TAB_GROUPS,
                                R.string.collaboration_learn_about_shared_groups)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.BLOCK_MESSAGE,
                                R.string.collaboration_owner_block_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.BLOCK_AND_LEAVE_GROUP_MESSAGE,
                                R.string.collaboration_block_leave_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEARN_ABOUT_BLOCKED_ACCOUNTS,
                                R.string.collaboration_block_leave_learn_more)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.REMOVE_MESSAGE,
                                R.string.collaboration_owner_remove_member_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.LEAVE_GROUP_MESSAGE,
                                R.string.collaboration_leave_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey.STOP_SHARING_MESSAGE,
                                R.string.collaboration_owner_stop_sharing_dialog_body)
                        .setResourceId(
                                DataSharingStringConfig.StringKey
                                        .LET_ANYONE_JOIN_GROUP_WHEN_FULL_DESCRIPTION,
                                R.string.collaboration_group_is_full_description)
                        .build();

        DataSharingManageUiConfig.ManageCallback manageCallback =
                new DataSharingManageUiConfig.ManageCallback() {
                    @Override
                    public void onShareInviteLinkClicked(GroupToken groupToken) {
                        onShareInviteLinkClickedWithWait(groupToken, null);
                    }

                    @Override
                    public void onShareInviteLinkClickedWithWait(
                            GroupToken groupToken, Callback<Boolean> onFinished) {
                        GURL url =
                                mDataSharingTabManager
                                        .getDataSharingService()
                                        .getDataSharingUrl(
                                                new GroupData(
                                                        groupToken.collaborationId,
                                                        tabGroupName,
                                                        /* members= */ null,
                                                        groupToken.accessToken));
                        if (url == null) {
                            Callback.runNullSafe(onFinished, false);
                            DataSharingMetrics.recordShareActionFlowState(
                                    DataSharingMetrics.ShareActionStateAndroid.URL_CREATION_FAILED);
                            return;
                        }
                        mDataSharingTabManager.showShareSheet(
                                mActivity, groupToken.collaborationId, url, onFinished);
                    }

                    @Override
                    public void onStopSharingInitiated(Callback<Boolean> readyToStopSharing) {
                        SavedTabGroup existingGroup =
                                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                                        collaborationId, tabGroupSyncService);
                        tabGroupSyncService.aboutToUnShareTabGroup(
                                existingGroup.localId, readyToStopSharing);
                    }

                    @Override
                    public void onStopSharingCompleted(boolean success) {
                        SavedTabGroup existingGroup =
                                DataSharingTabGroupUtils.getTabGroupForCollabIdFromSync(
                                        collaborationId, tabGroupSyncService);
                        tabGroupSyncService.onTabGroupUnShareComplete(
                                existingGroup.localId, success);
                    }

                    @Override
                    public void onSessionFinished() {
                        if (finishRunnable != null) {
                            finishRunnable.run();
                        }
                    }
                };
        DataSharingManageUiConfig manageConfig =
                new DataSharingManageUiConfig.Builder()
                        .setGroupToken(new GroupToken(collaborationId, null))
                        .setManageCallback(manageCallback)
                        .setLearnAboutBlockedAccounts(getLearnAboutBlockedAccountsUrl())
                        .setActivityLogsUrl(getActivityLogsUrl())
                        .setCommonConfig(getCommonConfig(tabGroupName, stringConfig))
                        .build();
        return uiDelegate.showManageFlow(manageConfig);
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
        mBulkFaviconUtil.destroy();
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

    BulkFaviconUtil getBulkFaviconUtilForTesting() {
        return mBulkFaviconUtil;
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
