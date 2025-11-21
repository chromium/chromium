// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.sync.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ApkInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.components.trusted_vault.TrustedVaultUserActionTriggerForUMA;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper methods for sync settings. */
@NullMarked
public class SyncSettingsUtils {
    private static final String MY_ACCOUNT_URL = "https://myaccount.google.com/smartlink/home";
    private static final String BOOKMARK_LIMIT_HELP_PAGE_URL =
            "https://support.google.com/chrome/answer/165139";
    private static final String TAG = "SyncSettingsUtils";

    @IntDef({TitlePreference.FULL_NAME, TitlePreference.EMAIL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TitlePreference {
        int FULL_NAME = 0;
        int EMAIL = 1;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // These are the actions users can taken on error cards, messages, and notifications.
    // Keep in sync with SyncErrorUiAction enum in sync/enums.xml, and SyncErrorPromptUIAction enum
    // in signin/enums.xml.
    // LINT.IfChange(SyncErrorUiAction)
    @IntDef({
        ErrorUiAction.SHOWN,
        ErrorUiAction.DISMISSED,
        ErrorUiAction.BUTTON_CLICKED,
        ErrorUiAction.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ErrorUiAction {
        int SHOWN = 0;
        int DISMISSED = 1;
        int BUTTON_CLICKED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncErrorUiAction)

    // Class to wrap the details of an error card.
    public static class ErrorCardDetails {
        public @StringRes int message;
        public @StringRes int buttonLabel;

        public ErrorCardDetails(@StringRes int message, @StringRes int buttonLabel) {
            this.message = message;
            this.buttonLabel = buttonLabel;
        }
    }

    /** Returns the type of the sync error */
    public static @UserActionableError int getSyncError(@Nullable Profile profile) {
        assert profile != null;
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        if (syncService == null) {
            return UserActionableError.NONE;
        }
        return syncService.getUserActionableError();
    }

    /**
     * Gets hint message to resolve sync error.
     *
     * @param context The application context.
     * @param error The sync error.
     */
    public static @Nullable String getSyncErrorHint(
            Context context, @UserActionableError int error) {
        switch (error) {
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                return context.getString(R.string.hint_sync_auth_error_modern);
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                return context.getString(
                        R.string.hint_client_out_of_date, ApkInfo.getHostPackageLabel());
            case UserActionableError.UNRECOVERABLE_ERROR:
                return context.getString(R.string.hint_other_sync_errors);
            case UserActionableError.NEEDS_PASSPHRASE:
                return context.getString(R.string.hint_passphrase_required);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                return context.getString(R.string.hint_sync_retrieve_keys_for_everything);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return context.getString(R.string.hint_sync_retrieve_keys_for_passwords);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return context.getString(R.string.hint_sync_recoverability_degraded_for_everything);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.hint_sync_recoverability_degraded_for_passwords);
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
                return context.getString(R.string.hint_sync_settings_not_confirmed_description);
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                return context.getString(R.string.sync_error_card_outdated_gms);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.bookmark_sync_limit_error_description);
            case UserActionableError.NONE:
            default:
                return null;
        }
    }

    /**
     * Gets the title for a sync error.
     *
     * @param context The application context.
     * @param error The sync error.
     */
    public static @Nullable String getSyncErrorCardTitle(
            Context context, @UserActionableError int error) {
        switch (error) {
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
            case UserActionableError.UNRECOVERABLE_ERROR:
            case UserActionableError.NEEDS_PASSPHRASE:
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                return context.getString(R.string.sync_error_card_title);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return context.getString(R.string.password_sync_error_summary);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.sync_needs_verification_title);
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                return context.getString(R.string.sync_error_outdated_gms);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.bookmark_sync_limit_error_title);
            case UserActionableError.NONE:
            default:
                return null;
        }
    }

    public static @Nullable String getSyncErrorCardButtonLabel(
            Context context, @UserActionableError int error) {
        switch (error) {
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
            case UserActionableError.UNRECOVERABLE_ERROR:
                // Both these errors should be resolved by signing the user again.
                return context.getString(R.string.auth_error_card_button);
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                return context.getString(
                        R.string.client_out_of_date_error_card_button,
                        ApkInfo.getHostPackageLabel());
            case UserActionableError.NEEDS_PASSPHRASE:
                return context.getString(R.string.passphrase_required_error_card_button);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.trusted_vault_error_card_button);
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
                return context.getString(R.string.sync_promo_turn_on_sync);
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                return context.getString(R.string.password_manager_outdated_gms_positive_button);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.learn_more);
            case UserActionableError.NONE:
            default:
                return null;
        }
    }

    /** Return a short summary of the current sync status. */
    public static String getSyncStatusSummary(Context context, Profile profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        if (syncService == null) {
            return context.getString(R.string.sync_off);
        }

        if (!syncService.hasSyncConsent()) {
            // There is no account with sync consent available.
            return context.getString(R.string.sync_off);
        }

        if (syncService.isSyncDisabledByEnterprisePolicy()) {
            return context.getString(R.string.sync_is_disabled_by_administrator);
        }

        if (syncService.getSelectedTypes().isEmpty()) {
            return context.getString(R.string.sync_data_types_off);
        }

        @UserActionableError int userActionableError = syncService.getUserActionableError();

        switch (userActionableError) {
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
                return context.getString(R.string.sync_settings_not_confirmed);
            case UserActionableError.UNRECOVERABLE_ERROR:
                return context.getString(R.string.sync_error_generic);
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                return getSyncStatusSummaryForAuthError(
                        context, syncService.getAuthError().getState());
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                return context.getString(
                        R.string.sync_error_upgrade_client, ApkInfo.getHostPackageLabel());
            case UserActionableError.NEEDS_PASSPHRASE:
                return context.getString(R.string.sync_need_passphrase);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                return context.getString(R.string.sync_error_card_title);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return context.getString(R.string.password_sync_error_summary);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return context.getString(R.string.sync_needs_verification_title);
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                return context.getString(R.string.sync_error_outdated_gms);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return context.getString(R.string.bookmark_sync_limit_error_title);
            case UserActionableError.NONE:
                break;
        }

        if (!syncService.isSyncFeatureActive()) {
            return context.getString(R.string.sync_setup_progress);
        }

        return context.getString(R.string.sync_on);
    }

    /**
     * Gets the sync status summary for a given {@link GoogleServiceAuthErrorState}.
     *
     * @param context The application context, used by the method to get string resources.
     * @param state Must not be GoogleServiceAuthError.State.None.
     */
    private static String getSyncStatusSummaryForAuthError(
            Context context, @GoogleServiceAuthErrorState int state) {
        return switch (state) {
            case GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS -> context.getString(
                    R.string.sync_error_ga);
            case GoogleServiceAuthErrorState.CONNECTION_FAILED -> context.getString(
                    R.string.sync_error_connection);
            case GoogleServiceAuthErrorState.SERVICE_UNAVAILABLE -> context.getString(
                    R.string.sync_error_service_unavailable);
            case GoogleServiceAuthErrorState.REQUEST_CANCELED,
                    GoogleServiceAuthErrorState.UNEXPECTED_SERVICE_RESPONSE,
                    GoogleServiceAuthErrorState.SERVICE_ERROR -> context.getString(
                    R.string.sync_error_generic);
            case GoogleServiceAuthErrorState.NONE -> {
                assert false : "No summary if there's no auth error";
                yield "";
            }
            default -> {
                assert false : "Unknown auth error state";
                yield "";
            }
        };
    }

    /** Returns an icon that represents the current sync state. */
    public static @Nullable Drawable getSyncStatusIcon(Context context, Profile profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        if (syncService == null
                || !syncService.hasSyncConsent()
                || syncService.getSelectedTypes().isEmpty()
                || syncService.isSyncDisabledByEnterprisePolicy()) {
            return AppCompatResources.getDrawable(context, R.drawable.ic_sync_off_48dp);
        }

        if (getSyncError(profile) != UserActionableError.NONE) {
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

        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        activity, customTabIntent.intent);
        intent.setPackage(activity.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);

        IntentUtils.safeStartActivity(activity, intent);
    }

    /**
     * Opens web dashboard to manage sync in a custom tab.
     *
     * @param activity The activity to use for starting the intent.
     */
    public static void openSyncDashboard(Activity activity) {
        // TODO(crbug.com/41450409): Create a builder for custom tab intents.
        openCustomTabWithURL(
                activity,
                ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ENABLE_NEW_SYNC_DASHBOARD_URL)
                        ? ChromeStringConstants.NEW_SYNC_DASHBOARD_URL
                        : ChromeStringConstants.LEGACY_SYNC_DASHBOARD_URL);
    }

    /**
     * Opens web dashboard to manage google account in a custom tab.
     *
     * <p>Callers should ensure the current account has sync consent prior to calling.
     *
     * @param activity The activity to use for starting the intent.
     */
    public static void openGoogleMyAccount(Activity activity) {
        RecordUserAction.record("SyncPreferences_ManageGoogleAccountClicked");
        openCustomTabWithURL(activity, MY_ACCOUNT_URL);
    }

    /**
     * Opens a help center article for the bookmark sync limit and acknowledges the error.
     *
     * @param activity The activity to use for starting the intent.
     * @param profile The profile to acknowledge the error for.
     */
    public static void openBookmarkLimitHelpPage(Activity activity, SyncService syncService) {
        assert syncService != null;
        syncService.acknowledgeBookmarksLimitExceededError();
        openCustomTabWithURL(activity, BOOKMARK_LIMIT_HELP_PAGE_URL);
    }

    /**
     * Upon promise completion, opens a dialog by starting the intent representing a user action
     * required for managing a trusted vault.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *     Fragment.onActivityResult().
     * @param pendingIntentPromise promise that provides the intent to be started.
     */
    private static void openTrustedVaultDialogForPendingIntent(
            Fragment fragment, int requestCode, Promise<PendingIntent> pendingIntentPromise) {
        pendingIntentPromise.then(
                (pendingIntent) -> {
                    try {
                        // startIntentSenderForResult() will fail if the fragment is
                        // already gone, see crbug.com/1362141.
                        if (!fragment.isAdded()) {
                            return;
                        }

                        fragment.startIntentSenderForResult(
                                pendingIntent.getIntentSender(),
                                requestCode,
                                /* fillInIntent= */ null,
                                /* flagsMask= */ 0,
                                /* flagsValues= */ 0,
                                /* extraFlags= */ 0,
                                /* options= */ null);
                    } catch (IntentSender.SendIntentException exception) {
                        Log.w(
                                TAG,
                                "Error sending trusted vault intent for code ",
                                requestCode,
                                ": ",
                                exception);
                    }
                },
                (exception) -> {
                    Log.e(
                            TAG,
                            "Error opening trusted vault dialog for code ",
                            requestCode,
                            ": ",
                            assumeNonNull(exception));
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
        TrustedVaultClient.get()
                .recordKeyRetrievalTrigger(TrustedVaultUserActionTriggerForUMA.SETTINGS);
        openTrustedVaultDialogForPendingIntent(
                fragment,
                requestCode,
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
        TrustedVaultClient.get()
                .recordRecoverabilityDegradedFixTrigger(
                        TrustedVaultUserActionTriggerForUMA.SETTINGS);
        openTrustedVaultDialogForPendingIntent(
                fragment,
                requestCode,
                TrustedVaultClient.get().createRecoverabilityDegradedIntent(accountInfo));
    }

    /**
     * Displays a UI that allows the user to opt in into the trusted vault passphrase type.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param accountInfo Account representing the user.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *     Fragment.onActivityResult().
     */
    public static void openTrustedVaultOptInDialog(
            Fragment fragment, CoreAccountInfo accountInfo, int requestCode) {
        openTrustedVaultDialogForPendingIntent(
                fragment, requestCode, TrustedVaultClient.get().createOptInIntent(accountInfo));
    }

    /**
     * Shows a toast indicating that sync is disabled for the account by the system administrator.
     *
     * @param context The context where the toast will be shown.
     */
    public static void showSyncDisabledByAdministratorToast(Context context) {
        Toast.makeText(
                        context,
                        context.getString(R.string.sync_is_disabled_by_administrator),
                        Toast.LENGTH_LONG)
                .show();
    }

    /**
     * Returns either the full name or the email address of a DisplayableProfileData according to
     * preference. If the preferred string is not displayable, returns the other displayable string,
     * or fallback to default string.
     *
     * <p>This method is used by {@link Preference#setTitle(CharSequence)} callers.
     *
     * @param profileData DisplayableProfileData containing the user's full name and email address.
     * @param context The context where the returned string is passed to setTitle(CharSequence).
     * @param preference Whether the full name or the email is preferred.
     */
    public static @Nullable String getDisplayableFullNameOrEmailWithPreference(
            DisplayableProfileData profileData, Context context, @TitlePreference int preference) {
        final String fullName = profileData.getFullName();
        final String accountEmail = profileData.getAccountEmail();
        final String defaultString = context.getString(R.string.default_google_account_username);
        final boolean canShowFullName = !TextUtils.isEmpty(fullName);
        final boolean canShowEmailAddress = profileData.hasDisplayableEmailAddress();
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

    /**
     * Gets text for the identity error card.
     *
     * @param error The identity error.
     * @return A ErrorCardDetails instance containing the error message and the button text for the
     *     identity error.
     */
    public static @Nullable ErrorCardDetails getIdentityErrorErrorCardDetails(
            @UserActionableError int error) {
        switch (error) {
            case UserActionableError.NEEDS_PASSPHRASE:
                return new ErrorCardDetails(
                        R.string.identity_error_card_passphrase_required,
                        R.string.identity_error_card_button_passphrase_required);
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                return new ErrorCardDetails(
                        R.string.identity_error_card_client_out_of_date,
                        R.string.identity_error_card_button_client_out_of_date);
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                return new ErrorCardDetails(
                        R.string.identity_error_card_auth_error,
                        R.string.identity_error_card_button_verify);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                return new ErrorCardDetails(
                        R.string.identity_error_card_sync_retrieve_keys_for_everything,
                        R.string.identity_error_card_button_verify);
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return new ErrorCardDetails(
                        R.string.identity_error_card_sync_retrieve_keys_for_passwords,
                        R.string.identity_error_card_button_verify);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return new ErrorCardDetails(
                        R.string.identity_error_card_sync_recoverability_degraded_for_everything,
                        R.string.identity_error_card_button_verify);
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return new ErrorCardDetails(
                        R.string.identity_error_card_sync_recoverability_degraded_for_passwords,
                        R.string.identity_error_card_button_verify);
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                return new ErrorCardDetails(
                        R.string.sync_error_card_outdated_gms,
                        R.string.password_manager_outdated_gms_positive_button);
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return new ErrorCardDetails(
                        R.string.bookmark_sync_limit_error_description, R.string.learn_more);
            case UserActionableError.UNRECOVERABLE_ERROR:
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
            case UserActionableError.NONE:
                assert false; // NOTREACHED()
                // fall through
            default:
                return null;
        }
    }

    /**
     * Gets the corresponding histogram name suffix for the error.
     *
     * @param error Error reason.
     * @return Suffix for the histogram.
     */
    public static String getHistogramSuffixForError(@UserActionableError int error) {
        assert error != UserActionableError.NONE;
        switch (error) {
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                return ".AuthError";
            case UserActionableError.NEEDS_PASSPHRASE:
                return ".PassphraseRequired";
            case UserActionableError.NEEDS_SETTINGS_CONFIRMATION:
                return ".SyncSetupIncomplete";
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                return ".ClientOutOfDate";
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                return ".TrustedVaultKeyRequiredForEverything";
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                return ".TrustedVaultKeyRequiredForPasswords";
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                return ".TrustedVaultRecoverabilityDegradedForEverything";
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                return ".TrustedVaultRecoverabilityDegradedForPasswords";
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                return ".UpmBackendOutdated";
            case UserActionableError.UNRECOVERABLE_ERROR:
                return ".OtherErrors";
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                return ".BookmarkLimitReached";
            default:
                assert false;
                return "";
        }
    }
}
