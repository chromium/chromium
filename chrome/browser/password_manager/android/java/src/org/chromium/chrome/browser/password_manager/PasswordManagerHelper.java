// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.PASSKEY_MANAGEMENT_USING_ACCOUNT_SETTINGS_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.common.api.ApiException;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Optional;

/**
 * A helper class for showing PasswordSettings.
 * TODO(crbug.com/1345232): Split up this class
 **/
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

    // Referrer string for the Google Play Store when installing GMS Core package
    private static final String STORE_REFERER = "chrome_upm";

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
    private static final String ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError";
    private static final String ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError.ConnectionResultCode";
    private static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    private static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    private static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    private static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    private static final String LOCAL_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.APIError";
    private static final String LOCAL_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.APIError"
            + ".ConnectionResultCode";
    private static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    private static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

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
     * @param managePasskeys indicates whether passkey management is needed, which when true will
     *      attempt to launch the credential manager even without syncing enabled.
     */
    public static void showPasswordSettings(Context context, @ManagePasswordsReferrer int referrer,
            SettingsLauncher settingsLauncher, SyncService syncService,
            Supplier<ModalDialogManager> modalDialogManagerSupplier, boolean managePasskeys) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);

        if (canUseUpm()) {
            LoadingModalDialogCoordinator loadingDialogCoordinator =
                    LoadingModalDialogCoordinator.create(modalDialogManagerSupplier, context);
            launchTheCredentialManager(referrer, syncService, loadingDialogCoordinator,
                    modalDialogManagerSupplier, context);
            return;
        }

        if (managePasskeys && canUseAccountSettings()) {
            // Passkey management has been selected but UPM is not available, possibly because
            // password sync is not turned on. Attempt to use an AccountSettings intent to show
            // an account chooser and open the native password manager, where passkeys are managed.
            CredentialManagerLauncher credentialManagerLauncher;
            try {
                credentialManagerLauncher = getCredentialManagerLauncher();
            } catch (CredentialManagerBackendException exception) {
                if (exception.errorCode != CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED) {
                    return;
                }

                showGmsUpdateDialog(modalDialogManagerSupplier, context);
                return;
            }

            String accountName = (syncService != null)
                    ? CoreAccountInfo.getEmailFrom(syncService.getAccountInfo())
                    : "";
            credentialManagerLauncher.getAccountSettingsIntent(accountName, context::startActivity);
            return;
        }

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(MANAGE_PASSWORDS_REFERRER, referrer);
        context.startActivity(settingsLauncher.createSettingsActivityIntent(
                context, PASSWORD_SETTINGS_CLASS, fragmentArgs));
    }

    /**
     * Checks the availability and status of the UPM feature.
     * All clients should check this before trying to use UPM methods.
     * Checks for the UPM to be anabled and downstream backend to be available.
     *
     * TODO(crbug.com/1327294): Make sure we rely on the same util in all places that need
     * to check whether UPM can be used (for password check as well as for all other cases that
     * share the same preconditions, e.g. launching the credential manager).
     *
     * TODO(crbug.com/1345232): pass syncService and prefService instances as parameters
     *
     * @return True if Unified Password Manager can be used, false otherwise.
     */
    public static boolean canUseUpm() {
        Profile profile = Profile.getLastUsedRegularProfile();
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        PrefService prefService = UserPrefs.get(profile);
        return PasswordManagerHelper.usesUnifiedPasswordManagerUI() && syncService != null
                && hasChosenToSyncPasswords(syncService)
                && !prefService.getBoolean(
                        Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS)
                && PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    /**
     * Checks the ability to use an AccountSettings intent to launch the password manager.
     * This provides a fallback for users who attempt to manage passkeys when UPM is not
     * available. Passkeys cannot be managed from the Chrome password settings page.
     *
     * Since there is not necessarily a signed in Chrome user, the intent might show an
     * account chooser before showing the password manager.
     *
     * @return True if the AccountSettings intent is available for use, false otherwise.
     */
    public static boolean canUseAccountSettings() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return PasswordManagerHelper.usesUnifiedPasswordManagerUI()
                && ChromeFeatureList.isEnabled(PASSKEY_MANAGEMENT_USING_ACCOUNT_SETTINGS_ANDROID)
                && PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    /**
     * Launches the Password Checkup UI from Google Play Services.
     *
     * @param context used to show the loading dialog.
     * @param referrer the place that requested to show the UI.
     * @param syncService the service to query about the sync status.
     * @param modalDialogManagerSupplier The supplier of the ModalDialogManager to be used by
     *         loading dialog.
     */
    public static void showPasswordCheckup(Context context, @PasswordCheckReferrer int referrer,
            SyncService syncService, Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        assert canUseUpm();

        Optional<String> account = hasChosenToSyncPasswords(syncService)
                ? Optional.of(CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()))
                : Optional.empty();

        LoadingModalDialogCoordinator loadingDialogCoordinator =
                LoadingModalDialogCoordinator.create(modalDialogManagerSupplier, context);

        launchPasswordCheckup(
                referrer, account, loadingDialogCoordinator, modalDialogManagerSupplier, context);
    }

    /**
     * Asynchronously runs Password Checkup in GMS Core and stores the result in
     * PasswordSpecifics then saves it to the ChromeSync module.
     *
     * @param referrer the place that requested to start a check.
     * @param accountName the account name that is syncing passwords. If no value was provided, the
     *         local account will be used
     * @param successCallback callback called when password check finishes successfully
     * @param failureCallback callback called if password check encountered an error
     */
    public static void runPasswordCheckupInBackground(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        assert canUseUpm();

        PasswordCheckupClientMetricsRecorder passwordCheckupMetricsRecorder =
                new PasswordCheckupClientMetricsRecorder(
                        PasswordCheckOperation.RUN_PASSWORD_CHECKUP);

        PasswordCheckupClientHelper checkupClient;
        try {
            checkupClient = getPasswordCheckupClientHelper();
        } catch (Exception exception) {
            failureCallback.onResult(exception);
            return;
        }

        checkupClient.runPasswordCheckupInBackground(referrer, accountName,
                result
                -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.empty());
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
     * @param accountName the account name that is syncing passwords. If no value was provided, the
     *         local account will be used.
     * @param successCallback callback called with the number of breached passwords.
     * @param failureCallback callback called if encountered an error.
     */
    public static void getBreachedCredentialsCount(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        assert canUseUpm();

        PasswordCheckupClientMetricsRecorder passwordCheckupMetricsRecorder =
                new PasswordCheckupClientMetricsRecorder(
                        PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT);

        PasswordCheckupClientHelper checkupClient;
        try {
            checkupClient = getPasswordCheckupClientHelper();
        } catch (Exception exception) {
            failureCallback.onResult(exception);
            return;
        }

        checkupClient.getBreachedCredentialsCount(referrer, accountName,
                result
                -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.empty());
                    successCallback.onResult(result);
                },
                error -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    failureCallback.onResult(error);
                });
    }

    /**
     * Checks whether the sync feature is enabled and the user has chosen to sync passwords.
     * Note that this doesn't mean that passwords are actively syncing.
     *
     * @param syncService the service to query about the sync status.
     * @return true if syncing passwords is enabled
     */
    public static boolean hasChosenToSyncPasswords(SyncService syncService) {
        return syncService != null && syncService.isSyncFeatureEnabled()
                && syncService.getSelectedTypes().contains(UserSelectableType.PASSWORDS);
    }

    /**
     * Checks whether the sync feature is enabled, the user has chosen to sync passwords and
     * they haven't set up a custom passphrase.
     * The caller should make sure that the sync engine is initialized before calling this
     * method.
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
     * The caller should make sure that the sync engine is initialized before calling this
     * method.
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

    public static boolean usesUnifiedPasswordManagerBranding() {
        return usesUnifiedPasswordManagerUI()
                || ChromeFeatureList.isEnabled(UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING);
    }

    // TODO(http://crbug.com/1371422): Remove method and manage eviction from native code
    // as this is covered by chrome://password-manager-internals page.
    public static void resetUpmUnenrollment() {
        // Exit early if Chrome doesn't need UPM UI. Assumes the unenroll pref isn't included in
        // the usesUnifiedPasswordManagementUI check.
        if (!PasswordManagerHelper.usesUnifiedPasswordManagerUI()) return;
        PrefService prefs = UserPrefs.get(Profile.getLastUsedRegularProfile());

        // Exit early if the user is not unenrolled.
        if (!prefs.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS)) return;

        // Re-enroll the user by resetting the enroll pref. Other state reset happens on
        // unenroll.
        prefs.setBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS, false);
    }

    public static void launchGmsUpdate(Context context) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        String deepLinkUrl = "market://details?id="
                + GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE + "&referrer=" + STORE_REFERER;

        intent.setPackage("com.android.vending");
        intent.setData(Uri.parse(deepLinkUrl));
        intent.putExtra("callerId", context.getPackageName());

        // Request for overlay flow, Play Store will fallback to the default
        // behaviour if overlay is not available.
        // TODO(crbug.com/1348506): Use AlleyOop v3 overlay UI after fixing Chrome restart
        // during the GMS Core installation.
        // intent.putExtra("overlay", true);

        context.startActivity(intent);
    }

    @VisibleForTesting
    static void launchTheCredentialManager(@ManagePasswordsReferrer int referrer,
            SyncService syncService, LoadingModalDialogCoordinator loadingDialogCoordinator,
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        assert canUseUpm();
        assert syncService != null;

        CredentialManagerLauncher credentialManagerLauncher;
        try {
            credentialManagerLauncher = getCredentialManagerLauncher();
        } catch (CredentialManagerBackendException exception) {
            if (exception.errorCode != CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED) return;

            showGmsUpdateDialog(modalDialogManagerSupplier, context);
            return;
        }

        loadingDialogCoordinator.show();

        long startTimeMs = SystemClock.elapsedRealtime();
        credentialManagerLauncher.getAccountCredentialManagerIntent(referrer,
                CoreAccountInfo.getEmailFrom(syncService.getAccountInfo()),
                (intent)
                        -> PasswordManagerHelper.launchCredentialManagerIntent(
                                intent, startTimeMs, true, loadingDialogCoordinator),
                (exception) -> {
                    PasswordManagerHelper.recordFailureMetrics(exception, true);
                    loadingDialogCoordinator.dismiss();
                });
    }

    @VisibleForTesting
    static void launchPasswordCheckup(@PasswordCheckReferrer int referrer, Optional<String> account,
            LoadingModalDialogCoordinator loadingDialogCoordinator,
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        assert canUseUpm();

        PasswordCheckupClientHelper checkupClient;
        try {
            checkupClient = getPasswordCheckupClientHelper();
        } catch (PasswordCheckBackendException exception) {
            if (exception.errorCode != CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED) return;

            showGmsUpdateDialog(modalDialogManagerSupplier, context);
            return;
        }

        loadingDialogCoordinator.show();
        PasswordCheckupClientMetricsRecorder passwordCheckupMetricsRecorder =
                new PasswordCheckupClientMetricsRecorder(
                        (PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT));
        checkupClient.getPasswordCheckupIntent(referrer, account,
                (intent)
                        -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.empty());
                    maybeLaunchIntentWithLoadingDialog(loadingDialogCoordinator, intent,
                            PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM);
                },
                (error) -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    loadingDialogCoordinator.dismiss();
                });
    }

    private static void recordFailureMetrics(Exception exception, boolean forAccount) {
        // While support for the local storage API exists in Chrome, it isn't used at this time.
        assert forAccount : "Local storage for preferences not ready for use";
        final String kGetIntentSuccessHistogram = forAccount ? ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                                                             : LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;
        final String kGetIntentErrorHistogram =
                forAccount ? ACCOUNT_GET_INTENT_ERROR_HISTOGRAM : LOCAL_GET_INTENT_ERROR_HISTOGRAM;
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, false);
        if (exception instanceof CredentialManagerBackendException) {
            int errorCode = ((CredentialManagerBackendException) exception).errorCode;
            RecordHistogram.recordEnumeratedHistogram(
                    kGetIntentErrorHistogram, errorCode, CredentialManagerError.COUNT);
            return;
        }

        // If the exception is not a Chrome-defined one, it means that the call failed at the
        // API call level.
        RecordHistogram.recordEnumeratedHistogram(kGetIntentErrorHistogram,
                CredentialManagerError.API_ERROR, CredentialManagerError.COUNT);

        if (!(exception instanceof ApiException)) return;

        final String kGetIntentApiErrorHistogram = forAccount
                ? ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM
                : LOCAL_GET_INTENT_API_ERROR_HISTOGRAM;
        final String kGetIntentErrorConnectionResultCodeHistogram = forAccount
                ? ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM
                : LOCAL_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM;

        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
        RecordHistogram.recordSparseHistogram(kGetIntentApiErrorHistogram, apiErrorCode);
        Integer connectionResultCode =
                PasswordManagerAndroidBackendUtil.getConnectionResultCode(exception);
        if (connectionResultCode == null) return;

        RecordHistogram.recordSparseHistogram(
                kGetIntentErrorConnectionResultCodeHistogram, connectionResultCode);
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
                           : LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM);
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
     * Launches the pending intent and reports metrics if the loading dialog was not cancelled
     * or timed out. Intent launch metric is not recorded if the loading was cancelled or timed
     * out.
     *
     * @param loadingDialogCoordinator {@link LoadingModalDialogCoordinator}.
     * @param intent {@link PendingIntent} to be launched.
     * @param intentLaunchSuccessHistogram Name of the intent launch success histogram.
     */
    private static void maybeLaunchIntentWithLoadingDialog(
            LoadingModalDialogCoordinator loadingDialogCoordinator, PendingIntent intent,
            String intentLaunchSuccessHistogram) {
        @LoadingModalDialogCoordinator.State
        int loadingDialogState = loadingDialogCoordinator.getState();
        if (loadingDialogState == LoadingModalDialogCoordinator.State.CANCELLED
                || loadingDialogState == LoadingModalDialogCoordinator.State.TIMED_OUT) {
            // Dialog was dismissed or timeout occurred before the loading finished, do not
            // launch the intent.
            return;
        }

        if (loadingDialogState == LoadingModalDialogCoordinator.State.PENDING) {
            // Dialog is not yet visible, dismiss immediately.
            loadingDialogCoordinator.dismiss();
            launchIntentAndRecordSuccess(intent, intentLaunchSuccessHistogram);
            return;
        }

        if (loadingDialogCoordinator.isImmediatelyDismissable()) {
            // Dialog is visible and dismissable. Dismiss with a small delay to cover the intent
            // launch delay.
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
                launchIntentAndRecordSuccess(intent, intentLaunchSuccessHistogram);
                new Handler(Looper.getMainLooper())
                        .postDelayed(
                                loadingDialogCoordinator::dismiss, LOADING_DIALOG_DISMISS_DELAY_MS);
            }

            @Override
            public void onDismissedWithState(@LoadingModalDialogCoordinator.State int finalState) {}
        });
    }

    private static void showGmsUpdateDialog(
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        ModalDialogManager modalDialogManager = modalDialogManagerSupplier.get();
        if (modalDialogManager == null) return;

        OutdatedGmsCoreDialog dialog =
                new OutdatedGmsCoreDialog(modalDialogManager, context, isAccepted -> {
                    if (isAccepted) launchGmsUpdate(context);
                });
        dialog.show();
    }

    // TODO(crbug.com/1327578): Exceptions should be thrown by factory, remove this method.
    private static PasswordCheckupClientHelper getPasswordCheckupClientHelper()
            throws PasswordCheckBackendException {
        PasswordCheckupClientHelper helper =
                PasswordCheckupClientHelperFactory.getInstance().createHelper();
        if (helper != null) return helper;

        if (PasswordManagerBackendSupportHelper.getInstance().isUpdateNeeded()) {
            throw new PasswordCheckBackendException("Backend version is not supported.",
                    CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED);
        }
        if (!PasswordManagerBackendSupportHelper.getInstance().isBackendPresent()) {
            throw new PasswordCheckBackendException(
                    "Backend downstream implementation is not available.",
                    CredentialManagerError.BACKEND_NOT_AVAILABLE);
        }

        throw new PasswordCheckBackendException(
                "Can not instantiate backend client.", CredentialManagerError.UNCATEGORIZED);
    }

    // TODO(crbug.com/1346239): Exceptions should be thrown by factory, remove this method.
    private static CredentialManagerLauncher getCredentialManagerLauncher()
            throws CredentialManagerBackendException {
        CredentialManagerLauncher launcher =
                CredentialManagerLauncherFactory.getInstance().createLauncher();
        if (launcher != null) return launcher;

        if (PasswordManagerBackendSupportHelper.getInstance().isUpdateNeeded()) {
            throw new CredentialManagerBackendException("Backend version is not supported.",
                    CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED);
        }
        if (!PasswordManagerBackendSupportHelper.getInstance().isBackendPresent()) {
            throw new CredentialManagerBackendException(
                    "Backend downstream implementation is not available.",
                    CredentialManagerError.BACKEND_NOT_AVAILABLE);
        }

        throw new CredentialManagerBackendException(
                "Can not instantiate backend client.", CredentialManagerError.UNCATEGORIZED);
    }
}
