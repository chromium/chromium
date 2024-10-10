// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.common.api.ApiException;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Optional;

/** A helper class for showing PasswordSettings. TODO(crbug.com/40853413): Split up this class */
public class PasswordManagerHelper {
    // Key for the argument with which PasswordsSettings will be launched. The value for
    // this argument should be part of the ManagePasswordsReferrer enum, which contains
    // all points of entry to the passwords settings.
    public static final String MANAGE_PASSWORDS_REFERRER = "manage-passwords-referrer";

    // Indicates the operation that was requested from the {@link PasswordCheckupClientHelper}.
    @IntDef({
        PasswordCheckOperation.RUN_PASSWORD_CHECKUP,
        PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT,
        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordCheckOperation {
        /** Run password checkup. */
        int RUN_PASSWORD_CHECKUP = 0;

        /** Obtain the number of breached credentials. */
        int GET_BREACHED_CREDENTIALS_COUNT = 1;

        /** Obtain pending intent for launching password checkup UI */
        int GET_PASSWORD_CHECKUP_INTENT = 2;
    }

    // Loading dialog is dismissed with this delay after sending an intent to prevent
    // the old activity from showing up before the new one is shown.
    private static final long LOADING_DIALOG_DISMISS_DELAY_MS = 300L;

    /**
     * The identifier of the loading dialog outcome.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. Please, keep in sync with tools/metrics/histograms/enums.xml.
     */
    @VisibleForTesting
    @IntDef({
        LoadingDialogOutcome.NOT_SHOWN_LOADED,
        LoadingDialogOutcome.SHOWN_LOADED,
        LoadingDialogOutcome.SHOWN_CANCELLED,
        LoadingDialogOutcome.SHOWN_TIMED_OUT,
        LoadingDialogOutcome.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface LoadingDialogOutcome {
        /** The loading dialog was requested but loading finished before it got shown. */
        int NOT_SHOWN_LOADED = 0;

        /** The loading dialog was shown, loading process finished. */
        int SHOWN_LOADED = 1;

        /** The loading dialog was shown and cancelled by user before loading finished. */
        int SHOWN_CANCELLED = 2;

        /** The loading dialog was shown and timed out before loading finished. */
        int SHOWN_TIMED_OUT = 3;

        int NUM_ENTRIES = 4;
    }

    private static ProfileKeyedMap<PasswordManagerHelper> sProfileMap;

    private final Profile mProfile;

    @VisibleForTesting
    PasswordManagerHelper(Profile profile) {
        assert profile != null;
        mProfile = profile;
    }

    /**
     * Return the {@link PasswordManagerHelper} associated with the passed in {@link
     * Profile#getOriginalProfile()}.
     */
    public static PasswordManagerHelper getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    new ProfileKeyedMap<>(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL,
                            ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, PasswordManagerHelper::new);
    }

    /**
     * Launches the password settings or, if available, the credential manager from Google Play
     * Services.
     *
     * @param context used to show the UI to manage passwords.
     * @param referrer indicates where the request to show the password settings UI comes from.
     * @param modalDialogManagerSupplier provides a {@link ModalDialogManager}. Used to show error
     *     modal dialogs or a loading dialog if opening the UI takes too long.
     * @param managePasskeys indicates whether passkey management is needed, which when true will
     *     attempt to launch the credential manager even without syncing enabled.
     * @param account the account for which to open the UI. An empty or null account signals that
     *     the UI should be opened for the local storage.
     * @param customTabIntentHelper provides an intent to open a custom tab displaying a help center
     *     article.
     */
    public void showPasswordSettings(
            Context context,
            @ManagePasswordsReferrer int referrer,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            boolean managePasskeys,
            @Nullable String account,
            CustomTabIntentHelper customTabIntentHelper) {
        RecordHistogram.recordEnumeratedHistogram(
                "PasswordManager.ManagePasswordsReferrer",
                referrer,
                ManagePasswordsReferrer.MAX_VALUE + 1);
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);

        if (PasswordAccessLossDialogHelper.tryShowAccessLossWarning(
                mProfile,
                context,
                referrer,
                modalDialogManagerSupplier,
                customTabIntentHelper,
                BuildInfo.getInstance())) {
            return;
        }

        // Force instantiation of GMSCore password settings if GMSCore update is required. Launching
        // Password settings will fail and instead the blocking dialog with the suggestion to update
        // will be displayed.
        if (canUseUpm()
                || PasswordManagerUtilBridge.isGmsCoreUpdateRequired(prefService, syncService)) {
            LoadingModalDialogCoordinator loadingDialogCoordinator =
                    LoadingModalDialogCoordinator.create(modalDialogManagerSupplier, context);
            launchTheCredentialManager(
                    referrer,
                    syncService,
                    loadingDialogCoordinator,
                    modalDialogManagerSupplier,
                    context,
                    account);
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

            String accountName =
                    (syncService != null)
                            ? CoreAccountInfo.getEmailFrom(syncService.getAccountInfo())
                            : "";
            // TODO(crbug.com/40948486): Find an alternative to account settings intent.
            credentialManagerLauncher.getAccountSettingsIntent(
                    accountName,
                    (intent) ->
                            PasswordManagerHelper.startAccountSettingsActivity(context, intent));
            return;
        }

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(MANAGE_PASSWORDS_REFERRER, referrer);
        context.startActivity(
                SettingsNavigationFactory.createSettingsNavigation()
                        .createSettingsIntent(context, SettingsFragment.PASSWORDS, fragmentArgs));
    }

    /**
     * Checks the availability and status of the UPM feature. All clients should check this before
     * trying to use UPM methods. Checks for the UPM to be anabled and downstream backend to be
     * available.
     *
     * <p>TODO(crbug.com/40226137): Make sure we rely on the same util in all places that need to
     * check whether UPM can be used (for password check as well as for all other cases that share
     * the same preconditions, e.g. launching the credential manager).
     *
     * @return True if Unified Password Manager can be used, false otherwise.
     */
    public boolean canUseUpm() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);
        // TODO(crbug.com/40226137): Move the backend presence checks in the util.
        return syncService != null
                && PasswordManagerUtilBridge.shouldUseUpmWiring(syncService, prefService)
                && PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    /**
     * Checks the ability to use an AccountSettings intent to launch the password manager. This
     * provides a fallback for users who attempt to manage passkeys when UPM is not available.
     * Passkeys cannot be managed from the Chrome password settings page.
     *
     * <p>Since there is not necessarily a signed in Chrome user, the intent might show an account
     * chooser before showing the password manager.
     *
     * @return True if the AccountSettings intent is available for use, false otherwise.
     */
    public static boolean canUseAccountSettings() {
        return PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    /**
     * Launches the Password Checkup UI from Google Play Services.
     *
     * @param context used to show the loading dialog.
     * @param referrer the place that requested to show the UI.
     * @param modalDialogManagerSupplier The supplier of the ModalDialogManager to be used by
     *     loading dialog.
     * @param accountEmail is the email of the account syncing passwords. If it's empty, the checkup
     *     for local will show. The purpose of this is to enable showing the checkup for local
     *     storage if the password checkup is launched from the leak detection dialog and the leaked
     *     credential is only saved in the local password storage.
     */
    public void showPasswordCheckup(
            Context context,
            @PasswordCheckReferrer int referrer,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable String accountEmail) {
        assert accountEmail == null || !accountEmail.isEmpty();

        // TODO(crbug.com/40945093): Change PasswordCheckupClientHelper.getPasswordCheckupIntent to
        // take the accountEmail as String.
        Optional<String> account =
                accountEmail == null ? Optional.empty() : Optional.of(accountEmail);

        LoadingModalDialogCoordinator loadingDialogCoordinator =
                LoadingModalDialogCoordinator.create(modalDialogManagerSupplier, context);

        launchPasswordCheckup(
                referrer, account, loadingDialogCoordinator, modalDialogManagerSupplier, context);
    }

    /**
     * Asynchronously runs Password Checkup in GMS Core and stores the result in PasswordSpecifics
     * then saves it to the ChromeSync module.
     *
     * @param referrer the place that requested to start a check.
     * @param accountName the account name that is syncing passwords. If no value was provided, the
     *     local account will be used
     * @param successCallback callback called when password check finishes successfully
     * @param failureCallback callback called if password check encountered an error
     */
    public void runPasswordCheckupInBackground(
            @PasswordCheckReferrer int referrer,
            String accountName,
            Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
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

        checkupClient.runPasswordCheckupInBackground(
                referrer,
                accountName,
                result -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.empty());
                    successCallback.onResult(null);
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
     *     local account will be used.
     * @param successCallback callback called with the number of breached passwords.
     * @param failureCallback callback called if encountered an error.
     */
    public void getBreachedCredentialsCount(
            @PasswordCheckReferrer int referrer,
            String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
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

        checkupClient.getBreachedCredentialsCount(
                referrer,
                accountName,
                result -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.empty());
                    successCallback.onResult(result);
                },
                error -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    failureCallback.onResult(error);
                });
    }

    /**
     * Checks whether the user has chosen to store passwords in their Google Account (no matter
     * whether sync-the-feature is on or not). Note that this doesn't mean that passwords are
     * actively syncing.
     *
     * @param syncService the service to query about the sync status.
     * @return true if syncing passwords is enabled
     */
    public static boolean hasChosenToSyncPasswords(SyncService syncService) {
        return PasswordManagerHelperJni.get().hasChosenToSyncPasswords(syncService);
    }

    /**
     * Checks whether the user is actively syncing passwords without a custom passphrase. The caller
     * should make sure that the sync engine is initialized before calling this method.
     *
     * @param syncService the service to query about the sync status.
     * @return true if actively syncing passwords and no custom passphrase was set.
     */
    public static boolean isSyncingPasswordsWithNoCustomPassphrase(SyncService syncService) {
        assert syncService.isEngineInitialized();
        if (syncService == null || !syncService.hasSyncConsent()) return false;
        if (!syncService.getActiveDataTypes().contains(DataType.PASSWORDS)) return false;
        if (syncService.isUsingExplicitPassphrase()) return false;
        return true;
    }

    // TODO(http://crbug.com/1371422): Remove method and manage eviction from native code
    // as this is covered by chrome://password-manager-internals page.
    public void resetUpmUnenrollment() {
        PrefService prefs = UserPrefs.get(mProfile);

        // Exit early if the user is not unenrolled.
        if (!prefs.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS)) return;

        // Re-enroll the user by resetting the enroll pref. Other state reset happens on
        // unenroll.
        prefs.setBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS, false);
    }

    @VisibleForTesting
    public void launchTheCredentialManager(
            @ManagePasswordsReferrer int referrer,
            SyncService syncService,
            LoadingModalDialogCoordinator loadingDialogCoordinator,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Context context,
            @Nullable String account) {
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
        if (!TextUtils.isEmpty(account)) {
            credentialManagerLauncher.getAccountCredentialManagerIntent(
                    referrer,
                    account,
                    (intent) ->
                            PasswordManagerHelper.launchCredentialManagerIntent(
                                    intent, startTimeMs, true, loadingDialogCoordinator),
                    (exception) -> {
                        PasswordManagerHelper.recordFailureMetrics(exception, true);
                        loadingDialogCoordinator.dismiss();
                    });
        } else {
            credentialManagerLauncher.getLocalCredentialManagerIntent(
                    referrer,
                    (intent) ->
                            PasswordManagerHelper.launchCredentialManagerIntent(
                                    intent, startTimeMs, false, loadingDialogCoordinator),
                    (exception) -> {
                        PasswordManagerHelper.recordFailureMetrics(exception, false);
                        loadingDialogCoordinator.dismiss();
                    });
        }
    }

    @VisibleForTesting
    void launchPasswordCheckup(
            @PasswordCheckReferrer int referrer,
            Optional<String> account,
            LoadingModalDialogCoordinator loadingDialogCoordinator,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Context context) {
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
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        // TODO(crbug.com/40945093): Change PasswordCheckupClientHelper.getPasswordCheckupIntent to
        // take the accountEmail as String.
        checkupClient.getPasswordCheckupIntent(
                referrer,
                account,
                (intent) -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.empty());
                    maybeLaunchIntentWithLoadingDialog(
                            loadingDialogCoordinator,
                            intent,
                            PasswordMetricsUtil
                                    .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM);
                },
                (error) -> {
                    passwordCheckupMetricsRecorder.recordMetrics(Optional.of(error));
                    loadingDialogCoordinator.dismiss();
                });
    }

    private static void recordFailureMetrics(Exception exception, boolean forAccount) {
        final String kGetIntentSuccessHistogram =
                forAccount
                        ? PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                        : PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;
        final String kGetIntentErrorHistogram =
                forAccount
                        ? PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM
                        : PasswordMetricsUtil.LOCAL_GET_INTENT_ERROR_HISTOGRAM;
        RecordHistogram.recordBooleanHistogram(kGetIntentSuccessHistogram, false);
        if (exception instanceof CredentialManagerBackendException) {
            int errorCode = ((CredentialManagerBackendException) exception).errorCode;
            RecordHistogram.recordEnumeratedHistogram(
                    kGetIntentErrorHistogram, errorCode, CredentialManagerError.COUNT);
            return;
        }

        // If the exception is not a Chrome-defined one, it means that the call failed at the
        // API call level. It could have either failed with a known ApiException or because of a
        // different error (e.g. a different exception thrown by the implementation of the API).
        if (!(exception instanceof ApiException)) {
            RecordHistogram.recordEnumeratedHistogram(
                    kGetIntentErrorHistogram,
                    CredentialManagerError.OTHER_API_ERROR,
                    CredentialManagerError.COUNT);
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                kGetIntentErrorHistogram,
                CredentialManagerError.API_EXCEPTION,
                CredentialManagerError.COUNT);

        final String kGetIntentApiErrorHistogram =
                forAccount
                        ? PasswordMetricsUtil.ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM
                        : PasswordMetricsUtil.LOCAL_GET_INTENT_API_ERROR_HISTOGRAM;
        final String kGetIntentErrorConnectionResultCodeHistogram =
                forAccount
                        ? PasswordMetricsUtil
                                .ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM
                        : PasswordMetricsUtil
                                .LOCAL_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM;

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

    private static void launchCredentialManagerIntent(
            PendingIntent intent,
            long startTimeMs,
            boolean forAccount,
            LoadingModalDialogCoordinator loadingDialogCoordinator) {
        recordSuccessMetrics(SystemClock.elapsedRealtime() - startTimeMs, forAccount);

        maybeLaunchIntentWithLoadingDialog(
                loadingDialogCoordinator,
                intent,
                forAccount
                        ? PasswordMetricsUtil.ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM
                        : PasswordMetricsUtil.LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM);
    }

    private static void recordSuccessMetrics(long elapsedTimeMs, boolean forAccount) {
        final String kGetIntentLatencyHistogram =
                forAccount
                        ? PasswordMetricsUtil.ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM
                        : PasswordMetricsUtil.LOCAL_GET_INTENT_LATENCY_HISTOGRAM;
        final String kGetIntentSuccessHistogram =
                forAccount
                        ? PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM
                        : PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM;

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
            LoadingModalDialogCoordinator loadingDialogCoordinator,
            PendingIntent intent,
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
        loadingDialogCoordinator.addObserver(
                new LoadingModalDialogCoordinator.Observer() {
                    @Override
                    public void onDismissable() {
                        // Record the known state - if the dialog was cancelled or timed out,
                        // {@link #onCancelledOrTimedOut()} would be called.
                        launchIntentAndRecordSuccess(intent, intentLaunchSuccessHistogram);
                        new Handler(Looper.getMainLooper())
                                .postDelayed(
                                        loadingDialogCoordinator::dismiss,
                                        LOADING_DIALOG_DISMISS_DELAY_MS);
                    }

                    @Override
                    public void onDismissedWithState(
                            @LoadingModalDialogCoordinator.State int finalState) {}
                });
    }

    private static void showGmsUpdateDialog(
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        ModalDialogManager modalDialogManager = modalDialogManagerSupplier.get();
        if (modalDialogManager == null) return;

        OutdatedGmsCoreDialog dialog =
                new OutdatedGmsCoreDialog(
                        modalDialogManager,
                        context,
                        isAccepted -> {
                            if (isAccepted) GmsUpdateLauncher.launch(context);
                        });
        dialog.show();
    }

    // TODO(crbug.com/40841269): Exceptions should be thrown by factory, remove this method.
    private PasswordCheckupClientHelper getPasswordCheckupClientHelper()
            throws PasswordCheckBackendException {
        if (!PasswordManagerBackendSupportHelper.getInstance().isBackendPresent()) {
            throw new PasswordCheckBackendException(
                    "Backend downstream implementation is not available.",
                    CredentialManagerError.BACKEND_NOT_AVAILABLE);
        }
        // This checks against GMSCore version required for using the account store (technically it
        // also checks if the internal backend is present, but the check above guarantees that if
        // this is executed then it is).
        if (!PasswordManagerUtilBridge.areMinUpmRequirementsMet()) {
            throw new PasswordCheckBackendException(
                    "Backend version is not supported.",
                    CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED);
        }
        // This checks against the account store GMSCore version if the user is syncing and against
        // the local version if the user is not syncing.
        if (PasswordManagerUtilBridge.isGmsCoreUpdateRequired(
                UserPrefs.get(mProfile), SyncServiceFactory.getForProfile(mProfile))) {
            throw new PasswordCheckBackendException(
                    "Backend version is not supported.",
                    CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED);
        }

        PasswordCheckupClientHelper helper =
                PasswordCheckupClientHelperFactory.getInstance().createHelper();
        if (helper != null) return helper;

        throw new PasswordCheckBackendException(
                "Can not instantiate backend client.", CredentialManagerError.UNCATEGORIZED);
    }

    // TODO(crbug.com/40854052): Exceptions should be thrown by factory, remove this method.
    private CredentialManagerLauncher getCredentialManagerLauncher()
            throws CredentialManagerBackendException {
        if (!PasswordManagerBackendSupportHelper.getInstance().isBackendPresent()) {
            throw new CredentialManagerBackendException(
                    "Backend downstream implementation is not available.",
                    CredentialManagerError.BACKEND_NOT_AVAILABLE);
        }
        if (!PasswordManagerUtilBridge.areMinUpmRequirementsMet()) {
            throw new CredentialManagerBackendException(
                    "Backend version is not supported.",
                    CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED);
        }
        // This checks against the account store GMSCore version if the user is syncing and against
        // the local version if the user is not syncing.
        if (PasswordManagerUtilBridge.isGmsCoreUpdateRequired(
                UserPrefs.get(mProfile), SyncServiceFactory.getForProfile(mProfile))) {
            throw new CredentialManagerBackendException(
                    "Backend version is not supported.",
                    CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED);
        }

        CredentialManagerLauncher launcher =
                CredentialManagerLauncherFactory.getInstance().createLauncher();
        if (launcher != null) return launcher;

        throw new CredentialManagerBackendException(
                "Can not instantiate backend client.", CredentialManagerError.UNCATEGORIZED);
    }

    private static void startAccountSettingsActivity(Context context, Intent intent) {
        Activity activity = ContextUtils.activityFromContext(context);
        if (activity != null) {
            try {
                activity.startActivityForResult(intent, 0);
            } catch (ActivityNotFoundException e) {
            }
        }
    }

    @NativeMethods
    public interface Natives {
        boolean hasChosenToSyncPasswords(@JniType("syncer::SyncService*") SyncService syncService);
    }
}
