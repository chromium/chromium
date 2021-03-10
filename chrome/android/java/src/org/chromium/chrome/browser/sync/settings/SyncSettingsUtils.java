// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.sync.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Browser;

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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.sync.KeyRetrievalTriggerForUMA;
import org.chromium.components.sync.StopSource;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper methods for sync settings.
 */
public class SyncSettingsUtils {
    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";
    private static final String MY_ACCOUNT_URL = "https://myaccount.google.com/smartlink/home";
    private static final String TAG = "SyncSettingsUtils";

    @IntDef({SyncError.NO_ERROR, SyncError.ANDROID_SYNC_DISABLED, SyncError.AUTH_ERROR,
            SyncError.PASSPHRASE_REQUIRED, SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
            SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS, SyncError.CLIENT_OUT_OF_DATE,
            SyncError.SYNC_SETUP_INCOMPLETE, SyncError.OTHER_ERRORS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncError {
        int NO_ERROR = -1;
        int ANDROID_SYNC_DISABLED = 0;
        int AUTH_ERROR = 1;
        int PASSPHRASE_REQUIRED = 2;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING = 3;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS = 4;
        int CLIENT_OUT_OF_DATE = 5;
        int SYNC_SETUP_INCOMPLETE = 6;
        int OTHER_ERRORS = 128;
    }

    /**
     * Returns the type of the sync error.
     */
    @SyncError
    public static int getSyncError() {
        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null) {
            return SyncError.NO_ERROR;
        }

        if (!profileSyncService.isSyncAllowedByPlatform()) {
            return SyncError.ANDROID_SYNC_DISABLED;
        }

        if (!profileSyncService.isSyncRequested()) {
            return SyncError.NO_ERROR;
        }

        if (profileSyncService.getAuthError()
                == GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS) {
            return SyncError.AUTH_ERROR;
        }

        if (profileSyncService.requiresClientUpgrade()) {
            return SyncError.CLIENT_OUT_OF_DATE;
        }

        if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE
                || profileSyncService.hasUnrecoverableError()) {
            return SyncError.OTHER_ERRORS;
        }

        if (profileSyncService.isEngineInitialized()
                && profileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            return SyncError.PASSPHRASE_REQUIRED;
        }

        if (profileSyncService.isEngineInitialized()
                && profileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            return profileSyncService.isEncryptEverythingEnabled()
                    ? SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING
                    : SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS;
        }

        if (!profileSyncService.isFirstSetupComplete()) {
            return SyncError.SYNC_SETUP_INCOMPLETE;
        }

        return SyncError.NO_ERROR;
    }

    /**
     * Gets hint message to resolve sync error.
     * @param context The application context.
     * @param error The sync error.
     */
    public static String getSyncErrorHint(Context context, @SyncError int error) {
        switch (error) {
            case SyncError.ANDROID_SYNC_DISABLED:
                return context.getString(R.string.hint_android_sync_disabled);
            case SyncError.AUTH_ERROR:
                return context.getString(R.string.hint_sync_auth_error);
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
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return context.getString(
                        ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                                ? R.string.hint_sync_settings_not_confirmed_description
                                : R.string.hint_sync_settings_not_confirmed_description_legacy);
            case SyncError.NO_ERROR:
            default:
                return null;
        }
    }

    public static @Nullable String getSyncErrorCardButtonLabel(
            Context context, @SyncError int error) {
        switch (error) {
            case SyncError.ANDROID_SYNC_DISABLED:
                return context.getString(R.string.android_sync_disabled_error_card_button);
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
     * TODO(https://crbug.com/1129930): Refactor this method
     */
    public static String getSyncStatusSummary(Context context) {
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount()) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
                // There is no account with sync consent available.
                return context.getString(R.string.sync_is_disabled);
            }
            return "";
        }

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null) {
            return context.getString(R.string.sync_is_disabled);
        }

        if (!profileSyncService.isSyncAllowedByPlatform()) {
            return context.getString(R.string.sync_android_system_sync_disabled);
        }

        if (profileSyncService.isSyncDisabledByEnterprisePolicy()) {
            return context.getString(R.string.sync_is_disabled_by_administrator);
        }

        if (!profileSyncService.isFirstSetupComplete()) {
            return ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                    ? context.getString(R.string.sync_settings_not_confirmed)
                    : context.getString(R.string.sync_settings_not_confirmed_legacy);
        }

        if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE) {
            return getSyncStatusSummaryForAuthError(context, profileSyncService.getAuthError());
        }

        if (profileSyncService.requiresClientUpgrade()) {
            return context.getString(
                    R.string.sync_error_upgrade_client, BuildInfo.getInstance().hostPackageLabel);
        }

        if (profileSyncService.hasUnrecoverableError()) {
            return context.getString(R.string.sync_error_generic);
        }

        if (!profileSyncService.isSyncRequested()) {
            return ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                    ? context.getString(R.string.sync_data_types_off)
                    : context.getString(R.string.sync_is_disabled);
        }

        if (!profileSyncService.isSyncFeatureActive()) {
            return context.getString(R.string.sync_setup_progress);
        }

        if (profileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            return context.getString(R.string.sync_need_passphrase);
        }

        if (profileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            return profileSyncService.isEncryptEverythingEnabled()
                    ? context.getString(R.string.sync_error_card_title)
                    : context.getString(R.string.password_sync_error_summary);
        }

        return context.getString(R.string.sync_and_services_summary_sync_on);
    }

    /**
     * Gets the sync status summary for a given {@link GoogleServiceAuthError.State}.
     * @param context The application context, used by the method to get string resources.
     * @param state Must not be GoogleServiceAuthError.State.None.
     */
    public static String getSyncStatusSummaryForAuthError(
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
        boolean useNewIcon =
                ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY);

        if (!IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount()) {
            return useNewIcon ? AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp)
                              : null;
        }

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null || !profileSyncService.isSyncRequested()) {
            return useNewIcon
                    ? AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp)
                    : UiUtils.getTintedDrawable(context, R.drawable.ic_sync_green_legacy_40dp,
                            R.color.default_icon_color);
        }
        if (profileSyncService.isSyncDisabledByEnterprisePolicy()) {
            return useNewIcon
                    ? AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp)
                    : UiUtils.getTintedDrawable(context, R.drawable.ic_sync_error_legacy_40dp,
                            R.color.default_icon_color);
        }

        if (getSyncError() != SyncError.NO_ERROR) {
            return useNewIcon
                    ? AppCompatResources.getDrawable(context, R.drawable.ic_sync_error_48dp)
                    : UiUtils.getTintedDrawable(
                            context, R.drawable.ic_sync_error_legacy_40dp, R.color.default_red);
        }

        return useNewIcon ? AppCompatResources.getDrawable(context, R.drawable.ic_sync_on_48dp)
                          : UiUtils.getTintedDrawable(context, R.drawable.ic_sync_green_legacy_40dp,
                                  R.color.default_green);
    }

    /**
     * Enables or disables {@link ProfileSyncService} and optionally records metrics that the sync
     * was disabled from settings. Requires that {@link ProfileSyncService#get()} returns non-null
     * reference.
     */
    public static void enableSync(boolean enable) {
        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (enable == profileSyncService.isSyncRequested()) return;

        if (enable) {
            profileSyncService.setSyncRequested(true);
        } else {
            RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                    StopSource.CHROME_SYNC_SETTINGS, StopSource.STOP_SOURCE_LIMIT);
            profileSyncService.setSyncRequested(false);
        }
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
        IntentHandler.addTrustedIntentExtras(intent);

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
                .hasPrimaryAccount();
        RecordUserAction.record("SyncPreferences_ManageGoogleAccountClicked");
        openCustomTabWithURL(activity, MY_ACCOUNT_URL);
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
        ProfileSyncService.get().recordKeyRetrievalTrigger(KeyRetrievalTriggerForUMA.SETTINGS);
        TrustedVaultClient.get()
                .createKeyRetrievalIntent(accountInfo)
                .then(
                        (pendingIntent)
                                -> {
                            try {
                                fragment.startIntentSenderForResult(pendingIntent.getIntentSender(),
                                        requestCode,
                                        /* fillInIntent */ null, /* flagsMask */ 0,
                                        /* flagsValues */ 0, /* extraFlags */ 0,
                                        /* options */ null);
                            } catch (IntentSender.SendIntentException exception) {
                                Log.w(TAG, "Error sending key retrieval intent: ", exception);
                            }
                        },
                        (exception) -> {
                            Log.e(TAG, "Error opening key retrieval dialog: ", exception);
                        });
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
}
