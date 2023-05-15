// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.sync.settings;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper methods for sync settings. */
public class SyncSettingsUtils {
    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";
    private static final String MY_ACCOUNT_URL = "https://myaccount.google.com/smartlink/home";
    private static final String TAG = "SyncSettingsUtils";

    @IntDef({TitlePreference.FULL_NAME, TitlePreference.EMAIL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TitlePreference {
        int FULL_NAME = 0;
        int EMAIL = 1;
    }

    @IntDef({SyncError.NO_ERROR, SyncError.AUTH_ERROR, SyncError.PASSPHRASE_REQUIRED,
            SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
            SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS,
            SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING,
            SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS,
            SyncError.CLIENT_OUT_OF_DATE, SyncError.SYNC_SETUP_INCOMPLETE, SyncError.OTHER_ERRORS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncError {
        int NO_ERROR = -1;
        int AUTH_ERROR = 0;
        int PASSPHRASE_REQUIRED = 1;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING = 2;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS = 3;
        int TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING = 4;
        int TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS = 5;
        int CLIENT_OUT_OF_DATE = 6;
        int SYNC_SETUP_INCOMPLETE = 7;
        int OTHER_ERRORS = 128;
    }

    /** Returns the type of the sync error. */
    @SyncError
    public static int getSyncError() {
        SyncService syncService = SyncService.get();
        if (syncService == null) {
            return SyncError.NO_ERROR;
        }

        if (!syncService.hasSyncConsent()) {
            return SyncError.NO_ERROR;
        }

        if (syncService.getAuthError() == GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS) {
            return SyncError.AUTH_ERROR;
        }

        if (syncService.requiresClientUpgrade()) {
            return SyncError.CLIENT_OUT_OF_DATE;
        }

        if (syncService.getAuthError() != GoogleServiceAuthError.State.NONE
                || syncService.hasUnrecoverableError()) {
            return SyncError.OTHER_ERRORS;
        }

        if (syncService.isEngineInitialized()
                && syncService.isPassphraseRequiredForPreferredDataTypes()) {
            return SyncError.PASSPHRASE_REQUIRED;
        }

        if (syncService.isEngineInitialized()
                && syncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            return syncService.isEncryptEverythingEnabled()
                    ? SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING
                    : SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS;
        }

        if (syncService.isEngineInitialized()
                && syncService.isTrustedVaultRecoverabilityDegraded()) {
            return syncService.isEncryptEverythingEnabled()
                    ? SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING
                    : SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS;
        }

        if (!syncService.isInitialSyncFeatureSetupComplete()) {
            return SyncError.SYNC_SETUP_INCOMPLETE;
        }

        return SyncError.NO_ERROR;
    }

    /**
     * Gets hint message to resolve sync error.
     *
     * @param context The application context.
     * @param error The sync error.
     */
    public static String getSyncErrorHint(Context context, @SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
                return context.getString(R.string.hint_sync_auth_error_modern);
            case SyncError.CLIENT_OUT_OF_DATE:
                return context.getString(
                        R.string.hint_client_out_of_date, BuildInfo.getInstance().hostPackageLabel);
            case SyncError.OTHER_ERRORS:
                return context.getString(R.string.hint_other_sync_errors);
            case SyncError.PASSPHRASE_REQUIRED:
                return context.getString(R.string.hint_passphrase_required);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return context.getString(R.string.hint_sync_retrieve_keys_for_everything);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return context.getString(R.string.hint_sync_retrieve_keys_for_passwords);
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return context.getString(R.string.hint_sync_recoverability_degraded_for_everything);
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.hint_sync_recoverability_degraded_for_passwords);
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return context.getString(R.string.hint_sync_settings_not_confirmed_description);
            case SyncError.NO_ERROR:
            default:
                return null;
        }
    }

    /**
     * Gets the title for a sync error.
     * @param context The application context.
     * @param error The sync error.
     */
    public static String getSyncErrorCardTitle(Context context, @SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
            case SyncError.CLIENT_OUT_OF_DATE:
            case SyncError.OTHER_ERRORS:
            case SyncError.PASSPHRASE_REQUIRED:
            case SyncError.SYNC_SETUP_INCOMPLETE:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
                return context.getString(R.string.sync_error_card_title);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return context.getString(R.string.password_sync_error_summary);
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.sync_needs_verification_title);
            case SyncError.NO_ERROR:
            default:
                return null;
        }
    }

    public static @Nullable String getSyncErrorCardButtonLabel(
            Context context, @SyncError int error) {
        switch (error) {
            case SyncError.AUTH_ERROR:
            case SyncError.OTHER_ERRORS:
                // Both these errors should be resolved by signing the user again.
                return context.getString(R.string.auth_error_card_button);
            case SyncError.CLIENT_OUT_OF_DATE:
                return context.getString(R.string.client_out_of_date_error_card_button,
                        BuildInfo.getInstance().hostPackageLabel);
            case SyncError.PASSPHRASE_REQUIRED:
                return context.getString(R.string.passphrase_required_error_card_button);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.trusted_vault_error_card_button);
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return context.getString(R.string.sync_promo_turn_on_sync);
            case SyncError.NO_ERROR:
            default:
                return null;
        }
    }

    /**
     * Return a short summary of the current sync status.
     */
    public static String getSyncStatusSummary(Context context) {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC)) {
            // There is no account with sync consent available.
            return context.getString(R.string.sync_off);
        }

        SyncService syncService = SyncService.get();
        if (syncService == null) {
            return context.getString(R.string.sync_off);
        }

        if (syncService.isSyncDisabledByEnterprisePolicy()) {
            return context.getString(R.string.sync_is_disabled_by_administrator);
        }

        if (!syncService.isInitialSyncFeatureSetupComplete()) {
            return context.getString(R.string.sync_settings_not_confirmed);
        }

        if (syncService.getAuthError() != GoogleServiceAuthError.State.NONE) {
            return getSyncStatusSummaryForAuthError(context, syncService.getAuthError());
        }

        if (syncService.requiresClientUpgrade()) {
            return context.getString(
                    R.string.sync_error_upgrade_client, BuildInfo.getInstance().hostPackageLabel);
        }

        if (syncService.hasUnrecoverableError()) {
            return context.getString(R.string.sync_error_generic);
        }

        if (syncService.getSelectedTypes().isEmpty()) {
            return context.getString(R.string.sync_data_types_off);
        }

        if (!syncService.isSyncFeatureActive()) {
            return context.getString(R.string.sync_setup_progress);
        }

        if (syncService.isPassphraseRequiredForPreferredDataTypes()) {
            return context.getString(R.string.sync_need_passphrase);
        }

        if (syncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            return syncService.isEncryptEverythingEnabled()
                    ? context.getString(R.string.sync_error_card_title)
                    : context.getString(R.string.password_sync_error_summary);
        }

        if (syncService.isTrustedVaultRecoverabilityDegraded()) {
            return context.getString(R.string.sync_needs_verification_title);
        }

        return context.getString(R.string.sync_on);
    }

    /**
     * Gets the sync status summary for a given {@link GoogleServiceAuthError.State}.
     * @param context The application context, used by the method to get string resources.
     * @param state Must not be GoogleServiceAuthError.State.None.
     */
    private static String getSyncStatusSummaryForAuthError(
            Context context, @GoogleServiceAuthError.State int state) {
        switch (state) {
            case GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS:
                return context.getString(R.string.sync_error_ga);
            case GoogleServiceAuthError.State.CONNECTION_FAILED:
                return context.getString(R.string.sync_error_connection);
            case GoogleServiceAuthError.State.SERVICE_UNAVAILABLE:
                return context.getString(R.string.sync_error_service_unavailable);
            case GoogleServiceAuthError.State.REQUEST_CANCELED:
            case GoogleServiceAuthError.State.UNEXPECTED_SERVICE_RESPONSE:
            case GoogleServiceAuthError.State.SERVICE_ERROR:
                return context.getString(R.string.sync_error_generic);
            case GoogleServiceAuthError.State.NONE:
                assert false : "No summary if there's no auth error";
                return "";
            default:
                assert false : "Unknown auth error state";
                return "";
        }
    }

    /**
     * Returns an icon that represents the current sync state.
     */
    public static @Nullable Drawable getSyncStatusIcon(Context context) {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp);
        }

        SyncService syncService = SyncService.get();
        if (syncService == null || syncService.getSelectedTypes().isEmpty()) {
            return AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp);
        }
        if (syncService.isSyncDisabledByEnterprisePolicy()) {
            return AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp);
        }

        if (getSyncError() != SyncError.NO_ERROR) {
            return AppCompatResources.getDrawable(context, R.drawable.ic_sync_error_48dp);
        }

        return AppCompatResources.getDrawable(context, R.drawable.ic_sync_on_48dp);
    }

    /**
     * Creates a wrapper around {@link Runnable} that calls the runnable only if
     * {@link PreferenceFragmentCompat} is still in resumed state. Click events that arrive after
     * the fragment has been paused will be ignored. See http://b/5983282.
     * @param fragment The fragment that hosts the preference.
     * @param runnable The runnable to call from {@link Preference.OnPreferenceClickListener}.
     */
    static Preference.OnPreferenceClickListener toOnClickListener(
            PreferenceFragmentCompat fragment, Runnable runnable) {
        return preference -> {
            if (!fragment.isResumed()) {
                // This event could come in after onPause if the user clicks back and the preference
                // at roughly the same time. See http://b/5983282.
                return false;
            }
            runnable.run();
            return false;
        };
    }

    /**
     * Opens web dashboard to specified url in a custom tab.
     * @param activity The activity to use for starting the intent.
     * @param url The url link to open in the custom tab.
     */
    private static void openCustomTabWithURL(Activity activity, String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(false).build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                activity, customTabIntent.intent);
        intent.setPackage(activity.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);

        IntentUtils.safeStartActivity(activity, intent);
    }

    /**
     * Opens web dashboard to manage sync in a custom tab.
     * @param activity The activity to use for starting the intent.
     */
    public static void openSyncDashboard(Activity activity) {
        // TODO(https://crbug.com/948103): Create a builder for custom tab intents.
        openCustomTabWithURL(activity, DASHBOARD_URL);
    }

    /**
     * Opens web dashboard to manage google account in a custom tab.
     * @param activity The activity to use for starting the intent.
     */
    public static void openGoogleMyAccount(Activity activity) {
        assert IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC);
        RecordUserAction.record("SyncPreferences_ManageGoogleAccountClicked");
        openCustomTabWithURL(activity, MY_ACCOUNT_URL);
    }

    /**
     * Upon promise completion, opens a dialog by starting the intent representing a user action
     * required for managing a trusted vault.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param accountInfo Account representing the user.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *         Fragment.onActivityResult().
     * @param pendingIntentPromise promise that provides the intent to be started.
     */
    private static void openTrustedVaultDialogForPendingIntent(Fragment fragment,
            CoreAccountInfo accountInfo, int requestCode,
            Promise<PendingIntent> pendingIntentPromise) {
        pendingIntentPromise.then(
                (pendingIntent)
                        -> {
                    try {
                        // startIntentSenderForResult() will fail if the fragment is
                        // already gone, see crbug.com/1362141.
                        if (!fragment.isAdded()) {
                            return;
                        }

                        fragment.startIntentSenderForResult(pendingIntent.getIntentSender(),
                                requestCode,
                                /* fillInIntent */ null, /* flagsMask */ 0,
                                /* flagsValues */ 0, /* extraFlags */ 0,
                                /* options */ null);
                    } catch (IntentSender.SendIntentException exception) {
                        Log.w(TAG, "Error sending trusted vault intent for code ", requestCode,
                                ": ", exception);
                    }
                },
                (exception) -> {
                    Log.e(TAG, "Error opening trusted vault dialog for code ", requestCode, ": ",
                            exception);
                });
    }

    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys
     * from a trusted vault.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param accountInfo Account representing the user.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *         Fragment.onActivityResult().
     */
    public static void openTrustedVaultKeyRetrievalDialog(
            Fragment fragment, CoreAccountInfo accountInfo, int requestCode) {
        TrustedVaultClient.get().recordKeyRetrievalTrigger(
                TrustedVaultUserActionTriggerForUMA.SETTINGS);
        openTrustedVaultDialogForPendingIntent(fragment, accountInfo, requestCode,
                TrustedVaultClient.get().createKeyRetrievalIntent(accountInfo));
    }

    /**
     * Displays a UI that allows the user to improve recoverability of the trusted vault data,
     * typically involving reauthentication.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param accountInfo Account representing the user.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *         Fragment.onActivityResult().
     */
    public static void openTrustedVaultRecoverabilityDegradedDialog(
            Fragment fragment, CoreAccountInfo accountInfo, int requestCode) {
        TrustedVaultClient.get().recordRecoverabilityDegradedFixTrigger(
                TrustedVaultUserActionTriggerForUMA.SETTINGS);
        openTrustedVaultDialogForPendingIntent(fragment, accountInfo, requestCode,
                TrustedVaultClient.get().createRecoverabilityDegradedIntent(accountInfo));
    }

    /**
     * Displays a UI that allows the user to opt in into the trusted vault passphrase type.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param accountInfo Account representing the user.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *         Fragment.onActivityResult().
     */
    public static void openTrustedVaultOptInDialog(
            Fragment fragment, CoreAccountInfo accountInfo, int requestCode) {
        openTrustedVaultDialogForPendingIntent(fragment, accountInfo, requestCode,
                TrustedVaultClient.get().createOptInIntent(accountInfo));
    }

    /**
     * Shows a toast indicating that sync is disabled for the account by the system administrator.
     *
     * @param context The context where the toast will be shown.
     */
    public static void showSyncDisabledByAdministratorToast(Context context) {
        Toast.makeText(context, context.getString(R.string.sync_is_disabled_by_administrator),
                     Toast.LENGTH_LONG)
                .show();
    }

    /**
     * Returns either the full name or the email address of a DisplayableProfileData according
     * to preference. If the preferred string is not displayable, returns the other displayable
     * string, or fallback to default string.
     *
     * This method is used by {@link Preference#setTitle(CharSequence)} callers.
     *
     * @param profileData DisplayableProfileData containing the user's full name and email address.
     * @param context The context where the returned string is passed to setTitle(CharSequence).
     * @param preference Whether the full name or the email is preferred.
     */
    public static String getDisplayableFullNameOrEmailWithPreference(
            DisplayableProfileData profileData, Context context, @TitlePreference int preference) {
        final String fullName = profileData.getFullName();
        final String accountEmail = profileData.getAccountEmail();
        final String defaultString = context.getString(R.string.default_google_account_username);
        final boolean canShowFullName = !TextUtils.isEmpty(fullName);
        final boolean canShowEmailAddress = profileData.hasDisplayableEmailAddress()
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.HIDE_NON_DISPLAYABLE_ACCOUNT_EMAIL);
        // Both strings are not displayable, use generic string.
        if (!canShowFullName && !canShowEmailAddress) {
            return defaultString;
        }
        // Both strings are displayable, use the preferred one.
        if (canShowFullName && canShowEmailAddress) {
            switch (preference) {
                case TitlePreference.FULL_NAME:
                    return fullName;
                case TitlePreference.EMAIL:
                    return accountEmail;
                default:
                    return defaultString;
            }
        }
        // The preference cannot be fulfilled, use the other displayable string.
        return canShowFullName ? fullName : accountEmail;
    }
}
