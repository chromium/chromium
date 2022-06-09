// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.common.base.Optional;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A helper class for showing PasswordSettings. */
public class PasswordManagerHelper {
    // Key for the argument with which PasswordsSettings will be launched. The value for
    // this argument should be part of the ManagePasswordsReferrer enum, which contains
    // all points of entry to the passwords settings.
    public static final String MANAGE_PASSWORDS_REFERRER = "manage-passwords-referrer";

    // Indicates the operation that was requested from the {@link PasswordCheckupClientHelper}.
    @IntDef({PasswordCheckOperation.RUN_PASSWORD_CHECKUP,
            PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT,
            PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordCheckOperation {
        /** Run password checkup. */
        int RUN_PASSWORD_CHECKUP = 0;
        /** Obtain the number of breached credentials. */
        int GET_BREACHED_CREDENTIALS_COUNT = 1;
        /** Obtain pending intent for launching password checkup UI */
        int GET_PASSWORD_CHECKUP_INTENT = 2;
    }

    private static final String UPM_VARIATION_FEATURE_PARAM = "stage";

    // Loading dialog is dismissed with this delay after sending an intent to prevent
    // the old activity from showing up before the new one is shown.
    private static final long LOADING_DIALOG_DISMISS_DELAY_MS = 300L;

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

    private static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

    private static final String LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM =
            "PasswordManager.ModalLoadingDialog.CredentialManager.Outcome";
    private static final String LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM =
            "PasswordManager.ModalLoadingDialog.PasswordCheckup.Outcome";

    private static PasswordCheckupClientMetricsRecorder sPasswordCheckupMetricsRecorderForTesting;

    /**
     *  The identifier of the loading dialog outcome.
     *
     *  These values are persisted to logs. Entries should not be renumbered and
     *  numeric values should never be reused.
     *  Please, keep in sync with tools/metrics/histograms/enums.xml.
     */
    @VisibleForTesting
    @IntDef({LoadingDialogOutcome.NOT_SHOWN_LOADED, LoadingDialogOutcome.SHOWN_LOADED,
            LoadingDialogOutcome.SHOWN_CANCELLED, LoadingDialogOutcome.SHOWN_TIMED_OUT,
            LoadingDialogOutcome.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface LoadingDialogOutcome {
        /** The loading dialog was requested but loading finished before it got shown. */
        int NOT_SHOWN_LOADED = 0;
        /** The loading dialog was shown, loading process finished.  */
        int SHOWN_LOADED = 1;
        /** The loading dialog was shown and cancelled by user before loading finished. */
        int SHOWN_CANCELLED = 2;
        /** The loading dialog was shown and timed out before loading finished. */
        int SHOWN_TIMED_OUT = 3;
        int NUM_ENTRIES = 4;
    }

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

        if (credentialManagerLauncher != null && canUseUpmCheckup()) {
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

    // TODO(crbug.com/1327294): Make sure we rely on the same util in all places that need
    // to check whether UPM can be used (for password check as well as for all other cases that
    // share the same preconditions, e.g. launching the credential manager).
    public static boolean canUseUpmCheckup() {
        SyncService syncService = SyncService.get();
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return PasswordManagerHelper.usesUnifiedPasswordManagerUI() && syncService != null
                && hasChosenToSyncPasswords(syncService)
                && !prefService.getBoolean(
                        Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS);
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
     * Asynchronously runs Password Checkup in GMS Core and stores the result in
     * PasswordSpecifics then saves it to the ChromeSync module.
     *
     * @param referrer the place that requested to start a check.
     * @param checkupClient the {@link PasswordCheckupClientHelper} instance to launch the checkup
     *         with.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used
     * @param successCallback callback called when password check finishes successfully
     * @param failureCallback callback called if password check encountered an error
     */
    public static void runPasswordCheckupInBackground(@PasswordCheckReferrer int referrer,
            PasswordCheckupClientHelper checkupClient, Optional<String> accountName,
            Callback<Void> successCallback, Callback<Exception> failureCallback) {
        PasswordCheckupClientMetricsRecorder passwordCheckupMetricsRecorder =
                new PasswordCheckupClientMetricsRecorder(
                        PasswordCheckOperation.RUN_PASSWORD_CHECKUP);
        checkupClient.runPasswordCheckupInBackground(referrer, accountName,
                result
                -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.absent());
                    successCallback.onResult(result);
                },
                error -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    failureCallback.onResult(error);
                });
    }

    /**
     * Asynchronously returns the number of breached credentials for the provided account.
     *
     * @param referrer the place that requested number of breached credentials.
     * @param checkupClient the {@link PasswordCheckupClientHelper} instance to request the count
     *         with.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with the number of breached passwords.
     * @param failureCallback callback called if encountered an error.
     */
    public static void getBreachedCredentialsCount(@PasswordCheckReferrer int referrer,
            PasswordCheckupClientHelper checkupClient, Optional<String> accountName,
            Callback<Integer> successCallback, Callback<Exception> failureCallback) {
        PasswordCheckupClientMetricsRecorder passwordCheckupMetricsRecorder =
                new PasswordCheckupClientMetricsRecorder(
                        PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT);
        checkupClient.getBreachedCredentialsCount(referrer, accountName,
                result
                -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.absent());
                    successCallback.onResult(result);
                },
                error -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    failureCallback.onResult(error);
                });
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
                    recordLoadingDialogMetrics(LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                            loadingDialogCoordinator.getState());
                    loadingDialogCoordinator.dismiss();
                });
    }

    @VisibleForTesting
    static void launchPasswordCheckup(@PasswordCheckReferrer int referrer,
            PasswordCheckupClientHelper checkupClient, Optional<String> account,
            LoadingModalDialogCoordinator loadingDialogCoordinator) {
        assert checkupClient != null;

        loadingDialogCoordinator.show();
        PasswordCheckupClientMetricsRecorder passwordCheckupMetricsRecorder =
                new PasswordCheckupClientMetricsRecorder(
                        (PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT));
        checkupClient.getPasswordCheckupIntent(referrer, account,
                (intent)
                        -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.absent());
                    maybeLaunchIntentWithLoadingDialog(loadingDialogCoordinator, intent,
                            PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                            LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM);
                },
                (error) -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    recordLoadingDialogMetrics(LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                            loadingDialogCoordinator.getState());
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

    private static void launchIntentAndRecordSuccess(
            PendingIntent intent, String intentLaunchSuccessHistogram) {
        boolean launchIntentSuccessfully = true;
        try {
            intent.send();
        } catch (CanceledException e) {
            launchIntentSuccessfully = false;
        }
        RecordHistogram.recordBooleanHistogram(
                intentLaunchSuccessHistogram, launchIntentSuccessfully);
    }

    private static void launchCredentialManagerIntent(PendingIntent intent, long startTimeMs,
            boolean forAccount, LoadingModalDialogCoordinator loadingDialogCoordinator) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        recordSuccessMetrics(SystemClock.elapsedRealtime() - startTimeMs, forAccount);

        maybeLaunchIntentWithLoadingDialog(loadingDialogCoordinator, intent,
                forAccount ? ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM
                           : LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM);
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

    /**
     * Launches the pending intent and reports metrics if the loading dialog was not cancelled or
     * timed out. Intent launch metric is not recorded if the loading was cancelled or timed out.
     *
     * @param loadingDialogCoordinator {@link LoadingModalDialogCoordinator}.
     * @param intent {@link PendingIntent} to be launched.
     * @param intentLaunchSuccessHistogram Name of the intent launch success histogram.
     */
    private static void maybeLaunchIntentWithLoadingDialog(
            LoadingModalDialogCoordinator loadingDialogCoordinator, PendingIntent intent,
            String intentLaunchSuccessHistogram, String loadingDialogOutcomeHistogram) {
        @LoadingModalDialogCoordinator.State
        int loadingDialogState = loadingDialogCoordinator.getState();
        if (loadingDialogState == LoadingModalDialogCoordinator.State.CANCELLED
                || loadingDialogState == LoadingModalDialogCoordinator.State.TIMED_OUT) {
            // Dialog was dismissed or timeout occurred before the loading finished, do not launch
            // the intent.
            recordLoadingDialogMetrics(loadingDialogOutcomeHistogram, loadingDialogState);
            return;
        }

        if (loadingDialogState == LoadingModalDialogCoordinator.State.PENDING) {
            // Dialog is not yet visible, dismiss immediately.
            recordLoadingDialogMetrics(loadingDialogOutcomeHistogram, loadingDialogState);
            loadingDialogCoordinator.dismiss();
            launchIntentAndRecordSuccess(intent, intentLaunchSuccessHistogram);
            return;
        }

        if (loadingDialogCoordinator.isImmediatelyDismissable()) {
            // Dialog is visible and dismissable. Dismiss with a small delay to cover the intent
            // launch delay.
            recordLoadingDialogMetrics(loadingDialogOutcomeHistogram, loadingDialogState);
            launchIntentAndRecordSuccess(intent, intentLaunchSuccessHistogram);
            new Handler(Looper.getMainLooper())
                    .postDelayed(
                            loadingDialogCoordinator::dismiss, LOADING_DIALOG_DISMISS_DELAY_MS);
            return;
        }

        // Dialog could not be dismissed right now, wait for it to become immediately
        // dismissable.
        loadingDialogCoordinator.addObserver(new LoadingModalDialogCoordinator.Observer() {
            @Override
            public void onDismissable() {
                // Record the known state - if the dialog was cancelled or timed out,
                // {@link #onCancelledOrTimedOut()} would be called.
                recordLoadingDialogMetrics(loadingDialogOutcomeHistogram, loadingDialogState);
                launchIntentAndRecordSuccess(intent, intentLaunchSuccessHistogram);
                new Handler(Looper.getMainLooper())
                        .postDelayed(
                                loadingDialogCoordinator::dismiss, LOADING_DIALOG_DISMISS_DELAY_MS);
            }

            @Override
            public void onDismissedWithState(@LoadingModalDialogCoordinator.State int finalState) {
                recordLoadingDialogMetrics(loadingDialogOutcomeHistogram, finalState);
            }
        });
    }

    /**
     * Reports metric for the GMS Core UI loading dialog.
     * Should be called right before launching the loaded intent or before dismissing the dialog if
     * the intent will not be launched.
     *
     * @param histogramName Name of the histogram to report metric via.
     * @param loadingDialogState State of the loading dialog before launching the intent.
     */
    private static void recordLoadingDialogMetrics(
            String histogramName, @LoadingModalDialogCoordinator.State int loadingDialogState) {
        switch (loadingDialogState) {
            case LoadingModalDialogCoordinator.State.PENDING:
                RecordHistogram.recordEnumeratedHistogram(histogramName,
                        LoadingDialogOutcome.NOT_SHOWN_LOADED, LoadingDialogOutcome.NUM_ENTRIES);
                break;
            case LoadingModalDialogCoordinator.State.SHOWN:
                RecordHistogram.recordEnumeratedHistogram(histogramName,
                        LoadingDialogOutcome.SHOWN_LOADED, LoadingDialogOutcome.NUM_ENTRIES);
                break;
            case LoadingModalDialogCoordinator.State.CANCELLED:
                RecordHistogram.recordEnumeratedHistogram(histogramName,
                        LoadingDialogOutcome.SHOWN_CANCELLED, LoadingDialogOutcome.NUM_ENTRIES);
                break;
            case LoadingModalDialogCoordinator.State.TIMED_OUT:
                RecordHistogram.recordEnumeratedHistogram(histogramName,
                        LoadingDialogOutcome.SHOWN_TIMED_OUT, LoadingDialogOutcome.NUM_ENTRIES);
                break;
            case LoadingModalDialogCoordinator.State.READY:
            case LoadingModalDialogCoordinator.State.FINISHED:
                throw new AssertionError(
                        "Unexpected state for metrics recording: " + loadingDialogState);
        }
    }
}
