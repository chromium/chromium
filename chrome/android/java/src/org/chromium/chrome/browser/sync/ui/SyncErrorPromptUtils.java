// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.chromium.base.ContextUtils.getApplicationContext;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Shared code between the old {@link org.chromium.chrome.browser.infobar.SyncErrorInfobar}
 * and the new UI based on {@link SyncErrorMessage}.
 *
 * TODO(crbug.com/1246073): make private as methods of message ui controller once the migration to
 *                          the new UI is completed.
 */
public class SyncErrorPromptUtils {
    @VisibleForTesting
    public static final long MINIMAL_DURATION_BETWEEN_UI_MS =
            TimeUnit.MILLISECONDS.convert(24, TimeUnit.HOURS);

    @VisibleForTesting
    public static final long MINIMAL_DURATION_TO_PWM_ERROR_UI_MS =
            TimeUnit.MILLISECONDS.convert(30, TimeUnit.MINUTES);

    @IntDef({SyncErrorPromptType.NOT_SHOWN, SyncErrorPromptType.AUTH_ERROR,
            SyncErrorPromptType.PASSPHRASE_REQUIRED, SyncErrorPromptType.SYNC_SETUP_INCOMPLETE,
            SyncErrorPromptType.CLIENT_OUT_OF_DATE,
            SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
            SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS,
            SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING,
            SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncErrorPromptType {
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
    @IntDef({SyncErrorPromptAction.SHOWN, SyncErrorPromptAction.DISMISSED,
            SyncErrorPromptAction.BUTTON_CLICKED, SyncErrorPromptAction.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncErrorPromptAction {
        int SHOWN = 0;
        int DISMISSED = 1;
        int BUTTON_CLICKED = 2;
        int NUM_ENTRIES = 3;
    }

    private static final String TAG = "SyncErrorPromptUtils";

    public static String getTitle(Context context, @SyncError int error) {
        // Use the same title with sync error card of sync settings.
        return SyncSettingsUtils.getSyncErrorCardTitle(context, error);
    }

    public static String getPrimaryButtonText(Context context, @SyncError int error) {
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

    public static String getErrorMessage(Context context, @SyncError int error) {
        return (error == SyncError.SYNC_SETUP_INCOMPLETE)
                ? context.getString(R.string.sync_settings_not_confirmed_title)
                : SyncSettingsUtils.getSyncErrorHint(context, error);
    }

    @SyncErrorPromptType
    public static int getSyncErrorUiType(@SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
                return SyncErrorPromptType.AUTH_ERROR;
            case SyncError.PASSPHRASE_REQUIRED:
                return SyncErrorPromptType.PASSPHRASE_REQUIRED;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return SyncErrorPromptType.SYNC_SETUP_INCOMPLETE;
            case SyncError.CLIENT_OUT_OF_DATE:
                return SyncErrorPromptType.CLIENT_OUT_OF_DATE;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS;
            default:
                return SyncErrorPromptType.NOT_SHOWN;
        }
    }

    public static void onUserAccepted(@SyncErrorPromptType int type, Activity activity) {
        switch (type) {
            case SyncErrorPromptType.NOT_SHOWN:
                assert false;
                return;
            case SyncErrorPromptType.AUTH_ERROR:
                if (ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)) {
                    startUpdateCredentialsFlow(activity);
                } else {
                    openSyncSettings();
                }
                return;
            case SyncErrorPromptType.PASSPHRASE_REQUIRED:
            case SyncErrorPromptType.SYNC_SETUP_INCOMPLETE:
            case SyncErrorPromptType.CLIENT_OUT_OF_DATE:
                openSyncSettings();
                return;

            case SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                openTrustedVaultKeyRetrievalActivity();
                return;

            case SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                openTrustedVaultRecoverabilityDegradedActivity();
                return;
        }
    }

    public static boolean shouldShowPrompt(@SyncErrorPromptType int type) {
        if (type == SyncErrorPromptType.NOT_SHOWN) {
            return false;
        }
        long lastShownTime =
                SharedPreferencesManager.getInstance().readLong(SYNC_ERROR_PROMPT_SHOWN_AT_TIME, 0);

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)) {
            // Since the password manager error and the sync error can be related,
            // the sync error should be shown only if at least MINIMAL_DURATION_TO_PWM_ERROR_UI_MS
            // have passed since the last password manager error. This condition is mirrored
            // for the password manager error.
            long currentTime = TimeUtils.currentTimeMillis();
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            long upmErrorShownTime =
                    Long.valueOf(prefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP));
            return currentTime - lastShownTime > MINIMAL_DURATION_BETWEEN_UI_MS
                    && currentTime - upmErrorShownTime > MINIMAL_DURATION_TO_PWM_ERROR_UI_MS;
        }
        return TimeUtils.currentTimeMillis() - lastShownTime > MINIMAL_DURATION_BETWEEN_UI_MS;
    }

    public static void updateLastShownTime() {
        SharedPreferencesManager.getInstance().writeLong(
                SYNC_ERROR_PROMPT_SHOWN_AT_TIME, TimeUtils.currentTimeMillis());
    }

    public static void resetLastShownTime() {
        SharedPreferencesManager.getInstance().removeKey(SYNC_ERROR_PROMPT_SHOWN_AT_TIME);
    }

    public static String getSyncErrorPromptUiHistogramSuffix(@SyncErrorPromptType int type) {
        assert type != SyncErrorPromptType.NOT_SHOWN;
        switch (type) {
            case SyncErrorPromptType.AUTH_ERROR:
                return "AuthError";
            case SyncErrorPromptType.PASSPHRASE_REQUIRED:
                return "PassphraseRequired";
            case SyncErrorPromptType.SYNC_SETUP_INCOMPLETE:
                return "SyncSetupIncomplete";
            case SyncErrorPromptType.CLIENT_OUT_OF_DATE:
                return "ClientOutOfDate";
            case SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return "TrustedVaultKeyRequiredForEverything";
            case SyncErrorPromptType.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return "TrustedVaultKeyRequiredForPasswords";
            case SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return "TrustedVaultRecoverabilityDegradedForEverything";
            case SyncErrorPromptType.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return "TrustedVaultRecoverabilityDegradedForPasswords";
            default:
                assert false;
                return "";
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
}
