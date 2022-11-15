// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.chromium.base.ContextUtils.getApplicationContext;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A message UI that informs the current sync error and contains a button to take action to resolve
 * it.
 * This class is tied to a window and at most one instance per window can exist at a time.
 * In practice however, because the time limit imposed between 2 displays is global,
 * only one instance in the whole application will exist at a time.
 */
public class SyncErrorMessage implements SyncStateChangedListener, UnownedUserData {
    @VisibleForTesting
    @IntDef({MessageType.NOT_SHOWN, MessageType.AUTH_ERROR, MessageType.PASSPHRASE_REQUIRED,
            MessageType.SYNC_SETUP_INCOMPLETE, MessageType.CLIENT_OUT_OF_DATE,
            MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
            MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS,
            MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING,
            MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageType {
        int NOT_SHOWN = -1;
        int AUTH_ERROR = 0;
        int PASSPHRASE_REQUIRED = 1;
        int SYNC_SETUP_INCOMPLETE = 2;
        int CLIENT_OUT_OF_DATE = 3;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING = 4;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS = 5;
        int TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING = 6;
        int TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS = 7;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({Action.SHOWN, Action.DISMISSED, Action.BUTTON_CLICKED, Action.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Action {
        int SHOWN = 0;
        int DISMISSED = 1;
        int BUTTON_CLICKED = 2;
        int NUM_ENTRIES = 3;
    }

    private final @MessageType int mType;
    private final Activity mActivity;
    private final MessageDispatcher mMessageDispatcher;
    private final PropertyModel mModel;
    private static MessageDispatcher sMessageDispatcherForTesting;

    private static final UnownedUserDataKey<SyncErrorMessage> SYNC_ERROR_MESSAGE_KEY =
            new UnownedUserDataKey<>(SyncErrorMessage.class);
    private static final String TAG = "SyncErrorMessage";

    /**
     * Creates a {@link SyncErrorMessage} in the window of |dispatcher|, or results in a no-op
     * if preconditions are not satisfied. The conditions are:
     * a) there is an ongoing sync error and it belongs to the subset defined by
     *    {@link MessageType}.
     * b) a minimal time interval has passed since the UI was last shown.
     * c) there is no other instance of the UI being shown on this window.
     * d) there is a valid {@link MessageDispatcher} in this window.
     *
     * @param windowAndroid The {@link WindowAndroid} to show and dismiss message UIs.
     */
    public static void maybeShowMessageUi(WindowAndroid windowAndroid) {
        try (TraceEvent t = TraceEvent.scoped("SyncErrorMessage.maybeShowMessageUi")) {
            if (getMessageType(SyncSettingsUtils.getSyncError()) == MessageType.NOT_SHOWN) {
                return;
            }

            if (!SyncErrorMessageImpressionTracker.canShowNow()) {
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
            SYNC_ERROR_MESSAGE_KEY.attachToHost(
                    host, new SyncErrorMessage(dispatcher, windowAndroid.getActivity().get()));
        }
    }

    private SyncErrorMessage(MessageDispatcher dispatcher, Activity activity) {
        @SyncError
        int error = SyncSettingsUtils.getSyncError();
        String errorMessage = error == SyncError.SYNC_SETUP_INCOMPLETE
                ? activity.getString(R.string.sync_settings_not_confirmed_title)
                : SyncSettingsUtils.getSyncErrorHint(activity, error);
        // Use the same title with sync error card of sync settings.
        String title = SyncSettingsUtils.getSyncErrorCardTitle(activity, error);
        String primaryButtonText = getPrimaryButtonText(activity, error);
        Resources resources = activity.getResources();
        mModel = new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                         .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                 MessageIdentifier.SYNC_ERROR)
                         .with(MessageBannerProperties.TITLE, title)
                         .with(MessageBannerProperties.DESCRIPTION, errorMessage)
                         .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, primaryButtonText)
                         .with(MessageBannerProperties.ICON,
                                 ApiCompatibilityUtils.getDrawable(
                                         resources, R.drawable.ic_sync_error_legacy_24dp))
                         .with(MessageBannerProperties.ICON_TINT_COLOR,
                                 activity.getColor(R.color.default_red))
                         .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::onAccepted)
                         .with(MessageBannerProperties.ON_DISMISSED, this::onDismissed)
                         .build();
        mMessageDispatcher =
                sMessageDispatcherForTesting == null ? dispatcher : sMessageDispatcherForTesting;
        mMessageDispatcher.enqueueWindowScopedMessage(mModel, false);
        mType = getMessageType(error);
        mActivity = activity;
        SyncService.get().addSyncStateChangedListener(this);
        SyncErrorMessageImpressionTracker.updateLastShownTime();
        recordHistogram(Action.SHOWN);
    }

    @Override
    public void syncStateChanged() {
        // If the error disappeared or changed type in the meantime, dismiss the UI.
        if (mType != getMessageType(SyncSettingsUtils.getSyncError())) {
            mMessageDispatcher.dismissMessage(mModel, DismissReason.UNKNOWN);
            assert !SYNC_ERROR_MESSAGE_KEY.isAttachedToAnyHost(this)
                : "Message UI should have been dismissed";
        }
    }

    private @PrimaryActionClickBehavior int onAccepted() {
        switch (mType) {
            case MessageType.NOT_SHOWN:
                assert false;
                break;
            case MessageType.AUTH_ERROR:
                if (ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)) {
                    startUpdateCredentialsFlow(mActivity);
                } else {
                    openSyncSettings();
                }
                break;
            case MessageType.PASSPHRASE_REQUIRED:
            case MessageType.SYNC_SETUP_INCOMPLETE:
            case MessageType.CLIENT_OUT_OF_DATE:
                openSyncSettings();
                break;
            case MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                openTrustedVaultKeyRetrievalActivity();
                break;
            case MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                openTrustedVaultRecoverabilityDegradedActivity();
                break;
        }

        recordHistogram(Action.BUTTON_CLICKED);
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void onDismissed(@DismissReason int reason) {
        if (reason != DismissReason.TIMER && reason != DismissReason.GESTURE
                && reason != DismissReason.PRIMARY_ACTION) {
            // If the user didn't explicitly accept/dismiss the message, and the display timeout
            // wasn't reached either, resetLastShownTime() so the message can be shown again. This
            // includes the case where the user changes tabs while the message is showing
            // (TAB_SWITCHED).
            SyncErrorMessageImpressionTracker.resetLastShownTime();
        }
        SyncService.get().removeSyncStateChangedListener(this);
        SYNC_ERROR_MESSAGE_KEY.detachFromAllHosts(this);

        // This metric should be recorded only on explicit dismissal.
        if (reason == DismissReason.GESTURE) {
            recordHistogram(Action.DISMISSED);
        }
    }

    private void recordHistogram(@Action int action) {
        assert mType != MessageType.NOT_SHOWN;
        String name = "Signin.SyncErrorMessage.";
        switch (mType) {
            case MessageType.AUTH_ERROR:
                name += "AuthError";
                break;
            case MessageType.PASSPHRASE_REQUIRED:
                name += "PassphraseRequired";
                break;
            case MessageType.SYNC_SETUP_INCOMPLETE:
                name += "SyncSetupIncomplete";
                break;
            case MessageType.CLIENT_OUT_OF_DATE:
                name += "ClientOutOfDate";
                break;
            case MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                name += "TrustedVaultKeyRequiredForEverything";
                break;
            case MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                name += "TrustedVaultKeyRequiredForPasswords";
                break;
            case MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                name += "TrustedVaultRecoverabilityDegradedForEverything";
                break;
            case MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                name += "TrustedVaultRecoverabilityDegradedForPasswords";
                break;
            default:
                assert false;
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(name, action, Action.NUM_ENTRIES);
    }

    private static String getPrimaryButtonText(Context context, @SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
                return ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)
                        ? context.getString(R.string.password_error_sign_in_button_title)
                        : context.getString(R.string.open_settings_button);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.trusted_vault_error_card_button);
            default:
                return context.getString(R.string.open_settings_button);
        }
    }

    private static void openTrustedVaultKeyRetrievalActivity() {
        CoreAccountInfo primaryAccountInfo = getSyncConsentedAccountInfo();
        if (primaryAccountInfo == null) {
            return;
        }
        TrustedVaultClient.get()
                .createKeyRetrievalIntent(primaryAccountInfo)
                .then(
                        (intent)
                                -> {
                            IntentUtils.safeStartActivity(getApplicationContext(),
                                    SyncTrustedVaultProxyActivity.createKeyRetrievalProxyIntent(
                                            intent,
                                            TrustedVaultUserActionTriggerForUMA
                                                    .NEW_TAB_PAGE_INFOBAR));
                        },
                        (exception)
                                -> Log.w(TAG, "Error creating trusted vault key retrieval intent: ",
                                        exception));
    }

    private static void openTrustedVaultRecoverabilityDegradedActivity() {
        CoreAccountInfo primaryAccountInfo = getSyncConsentedAccountInfo();
        if (primaryAccountInfo == null) {
            return;
        }
        TrustedVaultClient.get()
                .createRecoverabilityDegradedIntent(primaryAccountInfo)
                .then(
                        (intent)
                                -> {
                            IntentUtils.safeStartActivity(getApplicationContext(),
                                    SyncTrustedVaultProxyActivity
                                            .createRecoverabilityDegradedProxyIntent(intent,
                                                    TrustedVaultUserActionTriggerForUMA
                                                            .NEW_TAB_PAGE_INFOBAR));
                        },
                        (exception)
                                -> Log.w(TAG,
                                        "Error creating trusted vault recoverability intent: ",
                                        exception));
    }

    private static void openSyncSettings() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(getApplicationContext(), ManageSyncSettings.class,
                ManageSyncSettings.createArguments(false));
    }

    private static void startUpdateCredentialsFlow(Activity activity) {
        Profile profile = Profile.getLastUsedRegularProfile();
        final CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get().getIdentityManager(profile).getPrimaryAccountInfo(
                        ConsentLevel.SYNC);
        assert primaryAccountInfo != null;
        AccountManagerFacadeProvider.getInstance().updateCredentials(
                CoreAccountInfo.getAndroidAccountFrom(primaryAccountInfo), activity, null);
    }

    private static CoreAccountInfo getSyncConsentedAccountInfo() {
        if (!SyncService.get().hasSyncConsent()) {
            return null;
        }
        return SyncService.get().getAccountInfo();
    }

    @VisibleForTesting
    @MessageType
    public static int getMessageType(@SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
                return MessageType.AUTH_ERROR;
            case SyncError.PASSPHRASE_REQUIRED:
                return MessageType.PASSPHRASE_REQUIRED;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return MessageType.SYNC_SETUP_INCOMPLETE;
            case SyncError.CLIENT_OUT_OF_DATE:
                return MessageType.CLIENT_OUT_OF_DATE;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return MessageType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return MessageType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS;
            default:
                return MessageType.NOT_SHOWN;
        }
    }

    @VisibleForTesting
    public static void setMessageDispatcherForTesting(MessageDispatcher dispatcherForTesting) {
        sMessageDispatcherForTesting = dispatcherForTesting;
    }

    @VisibleForTesting
    public static UnownedUserDataKey<SyncErrorMessage> getKeyForTesting() {
        return SYNC_ERROR_MESSAGE_KEY;
    }
}
