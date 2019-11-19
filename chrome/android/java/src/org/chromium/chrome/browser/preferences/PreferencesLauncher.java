// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.Fragment;

import androidx.annotation.Nullable;

import com.google.android.gms.common.ConnectionResult;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.GooglePasswordManagerUIProvider;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.preferences.autofill.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfilesFragment;
import org.chromium.chrome.browser.preferences.password.SavePasswordsPreferences;
import org.chromium.chrome.browser.preferences.website.SettingsNavigationSource;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * A utility class for launching Chrome Settings.
 */
public class PreferencesLauncher {
    private static final String TAG = "PreferencesLauncher";

    private static final String GOOGLE_ACCOUNT_PWM_UI = "google-password-manager";

    // Name of the parameter for the google-password-manager feature, used to override the default
    // minimum version for Google Play Services.
    private static final String MIN_GOOGLE_PLAY_SERVICES_VERSION_PARAM =
            "min-google-play-services-version";

    // Default value for the minimum version for Google Play Services, such that the Google Account
    // password manager is available. Set to v21.
    // This can be overridden via Finch.
    private static final int DEFAULT_MIN_GOOGLE_PLAY_SERVICES_APK_VERSION = 13400000;

    /**
     * Launches settings, either on the top-level page or on a subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the top-level page.
     */
    public static void launchSettingsPage(
            Context context, @Nullable Class<? extends Fragment> fragment) {
        launchSettingsPage(context, fragment, null);
    }

    /**
     * Launches settings, either on the top-level page or on a subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The name of the fragment to show, or null to show the top-level page.
     * @param fragmentArgs The arguments bundle to initialize the instance of subpage fragment.
     */
    public static void launchSettingsPage(Context context,
            @Nullable Class<? extends Fragment> fragment, @Nullable Bundle fragmentArgs) {
        String fragmentName = fragment != null ? fragment.getName() : null;
        Intent intent = createIntentForSettingsPage(context, fragmentName, fragmentArgs);
        IntentUtils.safeStartActivity(context, intent);
    }

    /**
     * Creates an intent for launching settings, either on the top-level settings page or a specific
     * subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the top-level page.
     */
    public static Intent createIntentForSettingsPage(
            Context context, @Nullable String fragmentName) {
        return createIntentForSettingsPage(context, fragmentName, null);
    }

    /**
     * Creates an intent for launching settings, either on the top-level settings page or a specific
     * subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the top-level page.
     * @param fragmentArgs The arguments bundle to initialize the instance of subpage fragment.
     */
    public static Intent createIntentForSettingsPage(
            Context context, @Nullable String fragmentName, @Nullable Bundle fragmentArgs) {
        Intent intent = new Intent();
        intent.setClass(context, Preferences.class);
        if (!(context instanceof Activity)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }
        if (fragmentName != null) {
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT, fragmentName);
        }
        if (fragmentArgs != null) {
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        }
        return intent;
    }

    /**
     * Creates an intent to launch single website preferences for the specified {@param url}.
     */
    public static Intent createIntentForSingleWebsitePreferences(
            Context context, String url, @SettingsNavigationSource int navigationSource) {
        Bundle args = SingleWebsitePreferences.createFragmentArgsForSite(url);
        args.putInt(SettingsNavigationSource.EXTRA_KEY, navigationSource);
        return createIntentForSettingsPage(context, SingleWebsitePreferences.class.getName(), args);
    }

    /**
     * Launches the password settings in or the Google Password Manager if available.
     * @param activity used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(
            Activity activity, @ManagePasswordsReferrer int referrer) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);
        if (isSyncingPasswordsWithoutCustomPassphrase()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.ManagePasswordsReferrerSignedInAndSyncing", referrer,
                    ManagePasswordsReferrer.MAX_VALUE + 1);
            if (!PrefServiceBridge.getInstance().isManagedPreference(
                        Pref.REMEMBER_PASSWORDS_ENABLED)) {
                if (tryShowingTheGooglePasswordManager(activity)) return;
            }
        }

        launchSettingsPage(activity, SavePasswordsPreferences.class);
    }

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        RecordUserAction.record("AutofillAddressesViewed");
        showSettingSubpage(webContents, AutofillProfilesFragment.class);
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        RecordUserAction.record("AutofillCreditCardsViewed");
        showSettingSubpage(webContents, AutofillPaymentMethodsFragment.class);
    }

    @CalledByNative
    private static void showPasswordSettings(
            WebContents webContents, @ManagePasswordsReferrer int referrer) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        WeakReference<Activity> currentActivity = window.getActivity();
        showPasswordSettings(currentActivity.get(), referrer);
    }

    private static void showSettingSubpage(
            WebContents webContents, Class<? extends Fragment> fragment) {
        WeakReference<Activity> currentActivity =
                webContents.getTopLevelNativeWindow().getActivity();
        launchSettingsPage(currentActivity.get(), fragment);
    }

    public static boolean isSyncingPasswordsWithoutCustomPassphrase() {
        ChromeSigninController signInController = ChromeSigninController.get();
        if (signInController == null || !signInController.isSignedIn()) return false;

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null
                || !profileSyncService.getActiveDataTypes().contains(ModelType.PASSWORDS)) {
            return false;
        }

        if (profileSyncService.isUsingSecondaryPassphrase()) return false;

        return true;
    }

    private static boolean tryShowingTheGooglePasswordManager(Activity activity) {
        GooglePasswordManagerUIProvider googlePasswordManagerUIProvider =
                AppHooks.get().createGooglePasswordManagerUIProvider();
        if (googlePasswordManagerUIProvider == null) return false;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return false;

        int minGooglePlayServicesVersion = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                GOOGLE_ACCOUNT_PWM_UI, MIN_GOOGLE_PLAY_SERVICES_VERSION_PARAM,
                DEFAULT_MIN_GOOGLE_PLAY_SERVICES_APK_VERSION);
        if (AppHooks.get().isGoogleApiAvailableWithMinApkVersion(minGooglePlayServicesVersion)
                != ConnectionResult.SUCCESS) {
            return false;
        }

        if (!ChromeFeatureList.isEnabled(GOOGLE_ACCOUNT_PWM_UI)) return false;

        return googlePasswordManagerUIProvider.showGooglePasswordManager(activity);
    }
}
