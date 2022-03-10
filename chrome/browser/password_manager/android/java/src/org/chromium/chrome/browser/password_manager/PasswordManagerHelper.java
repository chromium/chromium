// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;

/** A helper class for showing PasswordSettings. */
public class PasswordManagerHelper {
    // Key for the argument with which PasswordsSettings will be launched. The value for
    // this argument should be part of the ManagePasswordsReferrer enum, which contains
    // all points of entry to the passwords settings.
    public static final String MANAGE_PASSWORDS_REFERRER = "manage-passwords-referrer";

    private static final String UPM_VARIATION_FEATURE_PARAM = "stage";

    // |PasswordSettings| full class name to open the fragment. Will be changed to
    // |PasswordSettings.class.getName()| once it's modularized.
    private static final String PASSWORD_SETTINGS_CLASS =
            "org.chromium.chrome.browser.password_manager.settings.PasswordSettings";
    private static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    private static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    private static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    private static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    private static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    private static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    private static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    private static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    /**
     * Launches the password settings or, if available, the credential manager from Google Play
     * Services.
     *
     * @param context used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(Context context, @ManagePasswordsReferrer int referrer,
            SettingsLauncher settingsLauncher, CredentialManagerLauncher credentialManagerLauncher,
            SyncService syncService) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);

        // The credential manager is NonNull if the Unified password manager is active or there is
        // a dry run measuring the latency/success of fetching the launch intent.
        if (credentialManagerLauncher != null) {
            // This method always request the launch intent but only actually launches it when the
            // UnifiedPasswordManager feature allows it.
            launchTheCredentialManager(referrer, credentialManagerLauncher, syncService);

            if (usesUnifiedPasswordManagerUI()) {
                // While waiting for the new UI, exit early to prevent launching the old settings.
                return;
            }
        }

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(MANAGE_PASSWORDS_REFERRER, referrer);
        context.startActivity(settingsLauncher.createSettingsActivityIntent(
                context, PASSWORD_SETTINGS_CLASS, fragmentArgs));
    }

    /**
     *  Checks whether the sync feature is enabled and the user has chosen to sync passwords.
     *  Note that this doesn't mean that passwords are actively syncing.
     *
     * @param syncService the service to query about the sync status.
     * @return true if syncing passwords is enabled
     */
    public static boolean hasChosenToSyncPasswords(SyncService syncService) {
        return syncService != null && syncService.isSyncFeatureEnabled()
                && syncService.getChosenDataTypes().contains(ModelType.PASSWORDS);
    }

    /**
     *  Checks whether the sync feature is enabled, the user has chosen to sync passwords and
     *  they haven't set up a custom passphrase.
     *  The caller should make sure that the sync engine is initialized before calling this method.
     *
     *  Note that this doesn't mean that passwords are actively syncing.
     *
     * @param syncService the service to query about the sync status.
     * @return true if syncing passwords is enabled without custom passphrase.
     */
    public static boolean hasChosenToSyncPasswordsWithNoCustomPassphrase(SyncService syncService) {
        assert syncService.isEngineInitialized();
        return PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                && !syncService.isUsingExplicitPassphrase();
    }

    /**
     * Checks whether the user is actively syncing passwords without a custom passphrase.
     * The caller should make sure that the sync engine is initialized before calling this method.
     *
     * @param syncService the service to query about the sync status.
     * @return true if actively syncing passwords and no custom passphrase was set.
     */
    public static boolean isSyncingPasswordsWithNoCustomPassphrase(SyncService syncService) {
        assert syncService.isEngineInitialized();
        if (syncService == null || !syncService.hasSyncConsent()) return false;
        if (!syncService.getActiveDataTypes().contains(ModelType.PASSWORDS)) return false;
        if (syncService.isUsingExplicitPassphrase()) return false;
        return true;
    }

    public static boolean usesUnifiedPasswordManagerUI() {
        return ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ANDROID)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                           UNIFIED_PASSWORD_MANAGER_ANDROID, UPM_VARIATION_FEATURE_PARAM,
                           UpmExperimentVariation.ENABLE_FOR_SYNCING_USERS)
                != UpmExperimentVariation.SHADOW_SYNCING_USERS;
    }

    private static void launchTheCredentialManager(@ManagePasswordsReferrer int referrer,
            CredentialManagerLauncher credentialManagerLauncher, SyncService syncService) {
        if (hasChosenToSyncPasswords(syncService)) {
            long startTimeMs = SystemClock.elapsedRealtime();
            credentialManagerLauncher.getCredentialManagerIntentForAccount(referrer,

                    CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()),
                    (intent)
                            -> PasswordManagerHelper.launchCredentialManager(
                                    intent, startTimeMs, true),
                    (error) -> PasswordManagerHelper.recordFailureMetrics(error, true));
            return;
        }

        long startTimeMs = SystemClock.elapsedRealtime();
        credentialManagerLauncher.getCredentialManagerIntentForLocal(referrer,
                (intent)
                        -> PasswordManagerHelper.launchCredentialManager(
                                intent, startTimeMs, false),
                (error) -> PasswordManagerHelper.recordFailureMetrics(error, false));
    }

    private static void recordFailureMetrics(
            @CredentialManagerError int error, boolean forAccount) {
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;
        final String kGetIntentErrorHistogram =
                forAccount ? ACCOUNT_GET_INTENT_ERROR_HISTOGRAM : LOCAL_GET_INTENT_ERROR_HISTOGRAM;
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, false);
        RecordHistogram.recordEnumeratedHistogram(
                kGetIntentErrorHistogram, error, CredentialManagerError.COUNT);
    }

    private static void launchCredentialManager(
            PendingIntent intent, long startTimeMs, boolean forAccount) {
        recordSuccessMetrics(SystemClock.elapsedRealtime() - startTimeMs, forAccount);

        if (!usesUnifiedPasswordManagerUI()) {
            return; // The built-in settings screen has already been started at this point.
        }

        boolean launchIntentSuccessfully = true;
        try {
            intent.send();
        } catch (CanceledException e) {
            launchIntentSuccessfully = false;
        }
        RecordHistogram.recordBooleanHistogram(forAccount
                        ? ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM
                        : LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                launchIntentSuccessfully);
    }

    private static void recordSuccessMetrics(long elapsedTimeMs, boolean forAccount) {
        final String kGetIntentLatencyHistogram = forAccount ? ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM
                                                             : LOCAL_GET_INTENT_LATENCY_HISTOGRAM;
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;

        RecordHistogram.recordTimesHistogram(kGetIntentLatencyHistogram, elapsedTimeMs);
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, true);
    }
}
