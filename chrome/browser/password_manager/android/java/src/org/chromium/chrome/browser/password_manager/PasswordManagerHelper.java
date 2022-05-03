// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import com.google.common.base.Optional;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;
import org.chromium.ui.modaldialog.ModalDialogManager;

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

    private static final String PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Latency";
    private static final String PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Success";
    private static final String PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Error";
    private static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

    /**
     * Launches the password settings or, if available, the credential manager from Google Play
     * Services.
     *
     * @param context used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(Context context, @ManagePasswordsReferrer int referrer,
            SettingsLauncher settingsLauncher, CredentialManagerLauncher credentialManagerLauncher,
            SyncService syncService,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);

        if (credentialManagerLauncher != null && hasChosenToSyncPasswords(syncService)) {
            LoadingModalDialogCoordinator loadingDialogCoordinator =
                    LoadingModalDialogCoordinator.create(modalDialogManagerSupplier, context);
            launchTheCredentialManager(
                    referrer, credentialManagerLauncher, syncService, loadingDialogCoordinator);
            return;
        }

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(MANAGE_PASSWORDS_REFERRER, referrer);
        context.startActivity(settingsLauncher.createSettingsActivityIntent(
                context, PASSWORD_SETTINGS_CLASS, fragmentArgs));
    }

    public static void showPasswordCheckup(Context context, @PasswordCheckReferrer int referrer,
            PasswordCheckupClientHelper checkupClient, SyncService syncService,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        if (!usesUnifiedPasswordManagerUI()) return;

        Optional<String> account = hasChosenToSyncPasswords(syncService)
                ? Optional.of(CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()))
                : Optional.absent();

        LoadingModalDialogCoordinator loadingDialogCoordinator =
                LoadingModalDialogCoordinator.create(modalDialogManagerSupplier, context);

        launchPasswordCheckup(referrer, checkupClient, account, loadingDialogCoordinator);
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
        if (!ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ANDROID)) return false;
        @UpmExperimentVariation
        int variation = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                UNIFIED_PASSWORD_MANAGER_ANDROID, UPM_VARIATION_FEATURE_PARAM,
                UpmExperimentVariation.ENABLE_FOR_SYNCING_USERS);
        switch (variation) {
            case UpmExperimentVariation.ENABLE_FOR_SYNCING_USERS:
            case UpmExperimentVariation.ENABLE_FOR_ALL_USERS:
                return true;
            case UpmExperimentVariation.SHADOW_SYNCING_USERS:
            case UpmExperimentVariation.ENABLE_ONLY_BACKEND_FOR_SYNCING_USERS:
                return false;
        }
        assert false : "Whether to use UI is undefined for variation: " + variation;
        return false;
    }

    @VisibleForTesting
    static void launchTheCredentialManager(@ManagePasswordsReferrer int referrer,
            CredentialManagerLauncher credentialManagerLauncher, SyncService syncService,
            LoadingModalDialogCoordinator loadingDialogCoordinator) {
        if (!hasChosenToSyncPasswords(syncService)) return;

        loadingDialogCoordinator.show();

        long startTimeMs = SystemClock.elapsedRealtime();
        credentialManagerLauncher.getCredentialManagerIntentForAccount(referrer,
                CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()),
                (intent)
                        -> PasswordManagerHelper.launchCredentialManagerIntent(
                                intent, startTimeMs, true, loadingDialogCoordinator),
                (error) -> {
                    PasswordManagerHelper.recordFailureMetrics(error, true);
                    loadingDialogCoordinator.dismiss();
                });
    }

    @VisibleForTesting
    static void launchPasswordCheckup(@PasswordCheckReferrer int referrer,
            PasswordCheckupClientHelper checkupClient, Optional<String> account,
            LoadingModalDialogCoordinator loadingDialogCoordinator) {
        assert checkupClient != null;

        loadingDialogCoordinator.show();

        long startTimeMs = SystemClock.elapsedRealtime();
        checkupClient.getPasswordCheckupPendingIntent(referrer, account,
                (intent)
                        -> PasswordManagerHelper.launchPasswordCheckupIntent(
                                intent, startTimeMs, loadingDialogCoordinator),
                (error) -> {
                    RecordHistogram.recordBooleanHistogram(
                            PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM, false);
                    RecordHistogram.recordEnumeratedHistogram(
                            PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM, error,
                            CredentialManagerError.COUNT);
                    loadingDialogCoordinator.dismiss();
                });
    }

    private static void recordFailureMetrics(
            @CredentialManagerError int error, boolean forAccount) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;
        final String kGetIntentErrorHistogram =
                forAccount ? ACCOUNT_GET_INTENT_ERROR_HISTOGRAM : LOCAL_GET_INTENT_ERROR_HISTOGRAM;
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, false);
        RecordHistogram.recordEnumeratedHistogram(
                kGetIntentErrorHistogram, error, CredentialManagerError.COUNT);
    }

    private static boolean launchIntent(PendingIntent intent, Runnable onLaunchFinishedCallback) {
        boolean launchIntentSuccessfully = true;
        try {
            PendingIntent.OnFinished onFinished = new PendingIntent.OnFinished() {
                @Override
                public void onSendFinished(PendingIntent pendingIntent, Intent intent,
                        int resultCode, String resultData, Bundle resultExtras) {
                    onLaunchFinishedCallback.run();
                }
            };
            intent.send(0, onFinished, new Handler(Looper.getMainLooper()));
        } catch (CanceledException e) {
            launchIntentSuccessfully = false;
            onLaunchFinishedCallback.run();
        }
        return launchIntentSuccessfully;
    }

    private static void launchCredentialManagerIntent(PendingIntent intent, long startTimeMs,
            boolean forAccount, LoadingModalDialogCoordinator loadingDialogCoordinator) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        recordSuccessMetrics(SystemClock.elapsedRealtime() - startTimeMs, forAccount);

        if (loadingDialogCoordinator.getState() == LoadingModalDialogCoordinator.State.CANCELLED) {
            // Dialog was dismissed before the loading finished, do not launch the intent.
            return;
        }

        boolean launchIntentSuccessfully = launchIntent(intent, loadingDialogCoordinator::dismiss);
        RecordHistogram.recordBooleanHistogram(forAccount
                        ? ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM
                        : LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                launchIntentSuccessfully);
    }

    private static void launchPasswordCheckupIntent(PendingIntent intent, long startTimeMs,
            LoadingModalDialogCoordinator loadingDialogCoordinator) {
        RecordHistogram.recordTimesHistogram(PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM,
                SystemClock.elapsedRealtime() - startTimeMs);
        RecordHistogram.recordBooleanHistogram(PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM, true);

        if (loadingDialogCoordinator.getState() == LoadingModalDialogCoordinator.State.CANCELLED) {
            // Dialog was dismissed before the loading finished, do not launch the intent.
            return;
        }

        boolean launchIntentSuccessfully = launchIntent(intent, loadingDialogCoordinator::dismiss);
        RecordHistogram.recordBooleanHistogram(
                PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                launchIntentSuccessfully);
    }

    private static void recordSuccessMetrics(long elapsedTimeMs, boolean forAccount) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        final String kGetIntentLatencyHistogram = forAccount ? ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM
                                                             : LOCAL_GET_INTENT_LATENCY_HISTOGRAM;
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;

        RecordHistogram.recordTimesHistogram(kGetIntentLatencyHistogram, elapsedTimeMs);
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, true);
    }
}
