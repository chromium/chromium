// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.chromium.base.ContextUtils.getApplicationContext;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.ErrorUiAction;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.components.trusted_vault.TrustedVaultUserActionTriggerForUMA;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A message UI that informs the current sync error and contains a button to take action to resolve
 * it. This class is tied to a window and at most one instance per window can exist at a time. In
 * practice however, because the time limit imposed between 2 displays is global, only one instance
 * in the whole application will exist at a time.
 */
@NullMarked
public class SyncErrorMessage implements SyncService.SyncStateChangedListener {
    // Note: Not all SyncErrors have a corresponding SyncErrorMessage, see getError().
    private final @UserActionableError int mError;
    private final Activity mActivity;
    private final Profile mProfile;
    private final IdentityManager mIdentityManager;
    private final SyncService mSyncService;
    private final MessageDispatcher mMessageDispatcher;
    private final PropertyModel mModel;
    private static @Nullable MessageDispatcher sMessageDispatcherForTesting;

    private static final UnownedUserDataKey<SyncErrorMessage> SYNC_ERROR_MESSAGE_KEY =
            new UnownedUserDataKey<>();
    private static final String PASSWORDS_SYNC_ERROR_MESSAGE_VERSION_PARAM_NAME = "version";
    private static final String TAG = "SyncErrorMessage";

    /**
     * Creates a {@link SyncErrorMessage} in the window of |dispatcher|, or results in a no-op if
     * preconditions are not satisfied. The conditions are:
     *
     * <p>a) there is an ongoing sync error and it belongs to the subset defined by {@link
     * MessageType}.
     *
     * <p>b) a minimal time interval has passed since the UI was last shown.
     *
     * <p>c) there is no other instance of the UI being shown on this window.
     *
     * <p>d) there is a valid {@link MessageDispatcher} in this window.
     *
     * @param windowAndroid The {@link WindowAndroid} to show and dismiss message UIs.
     * @param profile The {@link Profile}.
     */
    public static void maybeShowMessageUi(WindowAndroid windowAndroid, Profile profile) {
        try (TraceEvent t = TraceEvent.scoped("SyncErrorMessage.maybeShowMessageUi")) {
            if (getError(profile) == UserActionableError.NONE) {
                return;
            }

            if (!SyncErrorMessageImpressionTracker.canShowNow(UserPrefs.get(profile))) {
                return;
            }

            MessageDispatcher dispatcher = MessageDispatcherProvider.from(windowAndroid);
            if (dispatcher == null) {
                // Show message next time when there is a valid dispatcher attached to this
                // window.
                return;
            }

            UnownedUserDataHost host = windowAndroid.getUnownedUserDataHost();
            if (SYNC_ERROR_MESSAGE_KEY.retrieveDataFromHost(host) != null) {
                // Show message next time when the previous message has disappeared.
                return;
            }
            var activity = windowAndroid.getActivity().get();
            assert activity != null : "Activity should be non-null.";
            SYNC_ERROR_MESSAGE_KEY.attachToHost(
                    host, new SyncErrorMessage(dispatcher, activity, profile));
        }
    }

    private SyncErrorMessage(MessageDispatcher dispatcher, Activity activity, Profile profile) {
        mError = getError(profile);
        mActivity = activity;
        mProfile = profile;
        var identityManager = IdentityServicesProvider.get().getIdentityManager(mProfile);
        assert identityManager != null : "IdentityManager should be non-null.";
        mIdentityManager = identityManager;
        mSyncService = assumeNonNull(SyncServiceFactory.getForProfile(mProfile));
        mSyncService.addSyncStateChangedListener(this);

        String errorMessage = getMessage(activity);
        String title = getTitle(activity);
        String primaryButtonText = getPrimaryButtonText(activity);
        Resources resources = activity.getResources();
        mModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.SYNC_ERROR)
                        .with(MessageBannerProperties.TITLE, title)
                        .with(MessageBannerProperties.DESCRIPTION, errorMessage)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, primaryButtonText)
                        .with(
                                MessageBannerProperties.ICON,
                                ApiCompatibilityUtils.getDrawable(
                                        resources, getNotificationIconResourceId()))
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                activity.getColor(R.color.default_red))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::onAccepted)
                        .with(MessageBannerProperties.ON_DISMISSED, this::onDismissed)
                        .build();
        mMessageDispatcher =
                sMessageDispatcherForTesting == null ? dispatcher : sMessageDispatcherForTesting;
        mMessageDispatcher.enqueueWindowScopedMessage(mModel, false);
        SyncErrorMessageImpressionTracker.updateLastShownTime();
        recordHistogram(ErrorUiAction.SHOWN);
    }

    private @DrawableRes int getNotificationIconResourceId() {
        if (mError == UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE)) {
            return R.drawable.ic_password_manager_key_off;
        }
        return R.drawable.ic_sync_error_legacy_24dp;
    }

    @Override
    public void syncStateChanged() {
        // If the error disappeared or changed type in the meantime, dismiss the UI.
        if (mError != getError(mProfile)) {
            mMessageDispatcher.dismissMessage(mModel, DismissReason.UNKNOWN);
            assert !SYNC_ERROR_MESSAGE_KEY.isAttachedToAnyHost(this)
                    : "Message UI should have been dismissed";
        }
    }

    private @PrimaryActionClickBehavior int onAccepted() {
        switch (mError) {
            case UserActionableError.NONE:
                assert false;
                break;
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                startUpdateCredentialsFlow(mActivity);
                break;
            case UserActionableError.NEEDS_PASSPHRASE:
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                openSettings();
                break;
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                openTrustedVaultKeyRetrievalActivity();
                break;
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                openTrustedVaultRecoverabilityDegradedActivity();
                break;
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                openBookmarkLimitHelpPage();
                break;
        }

        recordHistogram(ErrorUiAction.BUTTON_CLICKED);
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void onDismissed(@DismissReason int reason) {
        if (reason != DismissReason.TIMER
                && reason != DismissReason.GESTURE
                && reason != DismissReason.PRIMARY_ACTION) {
            // If the user didn't explicitly accept/dismiss the message, and the display timeout
            // wasn't reached either, resetLastShownTime() so the message can be shown again. This
            // includes the case where the user changes tabs while the message is showing
            // (TAB_SWITCHED).
            SyncErrorMessageImpressionTracker.resetLastShownTime();
        }
        mSyncService.removeSyncStateChangedListener(this);
        SYNC_ERROR_MESSAGE_KEY.detachFromAllHosts(this);

        // This metric should be recorded only on explicit dismissal.
        if (reason == DismissReason.GESTURE) {
            recordHistogram(ErrorUiAction.DISMISSED);
        }
    }

    private void recordHistogram(@ErrorUiAction int action) {
        assert mError != UserActionableError.NONE;
        String name =
                (mSyncService.hasSyncConsent()
                                ? "Signin.SyncErrorMessage"
                                : "Sync.IdentityErrorMessage")
                        + SyncSettingsUtils.getHistogramSuffixForError(mError);
        RecordHistogram.recordEnumeratedHistogram(name, action, ErrorUiAction.NUM_ENTRIES);
    }

    private String getPrimaryButtonText(Context context) {
        assert mError != UserActionableError.NONE;
        // Check if this is for a sync error.
        if (mSyncService.hasSyncConsent()) {
            switch (mError) {
                case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                    return context.getString(R.string.password_error_sign_in_button_title);
                case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                    return context.getString(R.string.trusted_vault_error_card_button);
                case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                    return context.getString(R.string.learn_more);
                default:
                    return context.getString(R.string.open_settings_button);
            }
        }

        // Strings for identity error.
        switch (mError) {
            case UserActionableError.NEEDS_PASSPHRASE:
                return context.getString(
                        R.string.identity_error_message_button_passphrase_required);
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                // Reuse the same string as that for the identity error card button.
                return context.getString(R.string.identity_error_card_button_client_out_of_date);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return context.getString(getButtonTextForTrustedVaultErrorInfobarStudy());
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.identity_error_message_button_verify);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.learn_more);
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
            case UserActionableError.UNRECOVERABLE_ERROR:
            default:
                assert false;
                return "";
        }
    }

    private @StringRes int getButtonTextForTrustedVaultErrorInfobarStudy() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE)) {
            switch (getTrustedVaultErrorMessageVersion()) {
                case 1:
                    return R.string.identity_error_message_button_verify;
                case 2:
                    return R.string.identity_error_card_button_okay;
                case 3:
                    return R.string.identity_error_card_button_get;
                default:
                    // This should never happen, as there are only two versions.
                    assert false
                            : "Invalid version for SyncEnablePasswordsSyncErrorMessageAlternative: "
                                    + getTrustedVaultErrorMessageVersion();
                    break;
            }
        }
        return R.string.identity_error_message_button_verify;
    }

    private @Nullable String getTitle(Context context) {
        assert mError != UserActionableError.NONE;
        // Check if this is for a sync error.
        if (mSyncService.hasSyncConsent()) {
            // Use the same title with sync error card of sync settings.
            return SyncSettingsUtils.getSyncErrorCardTitle(context, mError);
        }

        // Strings for identity error.
        switch (mError) {
            case UserActionableError.NEEDS_PASSPHRASE:
                return context.getString(R.string.identity_error_message_title_passphrase_required);
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                return context.getString(R.string.identity_error_message_title_client_out_of_date);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return context.getString(getTitleForTrustedVaultErrorMessageStudy());
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                // Reuse the same string as that for the identity error card button.
                return context.getString(R.string.identity_error_card_button_verify);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.bookmark_sync_limit_error_title);
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
            case UserActionableError.UNRECOVERABLE_ERROR:
            default:
                assert false;
                return "";
        }
    }

    private @StringRes int getTitleForTrustedVaultErrorMessageStudy() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE)) {
            return R.string.password_sync_trusted_vault_error_title;
        }
        return R.string.identity_error_card_button_verify;
    }

    private @Nullable String getMessage(Context context) {
        assert mError != UserActionableError.NONE;
        // Check if this is for a sync error.
        if (mSyncService.hasSyncConsent()) {
            return mError == UserActionableError.NEEDS_SETTINGS_CONFIRMATION
                    ? context.getString(R.string.sync_settings_not_confirmed_title)
                    : SyncSettingsUtils.getSyncErrorHint(context, mError);
        }

        // Strings for identity error.
        switch (mError) {
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return context.getString(getContentForTrustedVaultErrorMessageStudy());
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return context.getString(
                        R.string
                                .identity_error_message_body_sync_recoverability_degraded_for_everything);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(
                        R.string
                                .identity_error_message_body_sync_recoverability_degraded_for_passwords);
            case UserActionableError.NEEDS_PASSPHRASE:
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                return context.getString(R.string.identity_error_message_body);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.bookmark_sync_limit_error_description);
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
            case UserActionableError.UNRECOVERABLE_ERROR:
            default:
                assert false;
                return "";
        }
    }

    private @StringRes int getContentForTrustedVaultErrorMessageStudy() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE)) {
            return R.string.password_sync_trusted_vault_error_hint;
        }
        return R.string.identity_error_message_body_sync_retrieve_keys_for_passwords;
    }

    private int getTrustedVaultErrorMessageVersion() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE,
                PASSWORDS_SYNC_ERROR_MESSAGE_VERSION_PARAM_NAME,
                /* defaultValue= */ 0);
    }

    private void openBookmarkLimitHelpPage() {
        SyncSettingsUtils.openBookmarkLimitHelpPage(mActivity, mSyncService);
    }

    private void openTrustedVaultKeyRetrievalActivity() {
        CoreAccountInfo primaryAccountInfo =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (primaryAccountInfo == null) {
            return;
        }
        TrustedVaultClient.get()
                .createKeyRetrievalIntent(primaryAccountInfo)
                .then(
                        (intent) -> {
                            IntentUtils.safeStartActivity(
                                    getApplicationContext(),
                                    SyncTrustedVaultProxyActivity.createKeyRetrievalProxyIntent(
                                            intent,
                                            TrustedVaultUserActionTriggerForUMA
                                                    .NEW_TAB_PAGE_INFOBAR));
                        },
                        (exception) -> {
                            var error = exception == null ? "unknown error." : exception;
                            Log.w(
                                    TAG,
                                    "Error creating trusted vault key retrieval intent: ",
                                    error);
                        });
    }

    private void openTrustedVaultRecoverabilityDegradedActivity() {
        CoreAccountInfo primaryAccountInfo =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (primaryAccountInfo == null) {
            return;
        }
        TrustedVaultClient.get()
                .createRecoverabilityDegradedIntent(primaryAccountInfo)
                .then(
                        (intent) -> {
                            var action = TrustedVaultUserActionTriggerForUMA.NEW_TAB_PAGE_INFOBAR;
                            var proxyIntent =
                                    SyncTrustedVaultProxyActivity
                                            .createRecoverabilityDegradedProxyIntent(
                                                    intent, action);
                            IntentUtils.safeStartActivity(getApplicationContext(), proxyIntent);
                        },
                        (exception) -> {
                            var error = exception == null ? "unknown error." : exception;
                            Log.w(
                                    TAG,
                                    "Error creating trusted vault recoverability intent: ",
                                    error);
                        });
    }

    private void openSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(
                getApplicationContext(),
                ManageSyncSettings.class,
                ManageSyncSettings.createArguments(false));
    }

    private void startUpdateCredentialsFlow(Activity activity) {
        final CoreAccountInfo primaryAccountInfo =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        assert primaryAccountInfo != null;
        AccountManagerFacadeProvider.getInstance()
                .updateCredentials(
                        CoreAccountInfo.getAndroidAccountFrom(primaryAccountInfo), activity, null);
    }

    private static @UserActionableError int getError(Profile profile) {
        @UserActionableError int error = SyncSettingsUtils.getSyncError(profile);
        // Do not show sync error message for UPM_BACKEND_OUTDATED or OTHER_ERRORS.
        if (error == UserActionableError.NEEDS_UPM_BACKEND_UPGRADE
                || error == UserActionableError.UNRECOVERABLE_ERROR) {
            return UserActionableError.NONE;
        }
        return error;
    }

    public static void setMessageDispatcherForTesting(MessageDispatcher dispatcherForTesting) {
        sMessageDispatcherForTesting = dispatcherForTesting;
        ResettersForTesting.register(() -> sMessageDispatcherForTesting = null);
    }

    public static UnownedUserDataKey<SyncErrorMessage> getKeyForTesting() {
        return SYNC_ERROR_MESSAGE_KEY;
    }
}
