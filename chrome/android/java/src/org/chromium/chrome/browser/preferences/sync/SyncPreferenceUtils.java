// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.preferences.sync;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Browser;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.StopSource;
import org.chromium.ui.UiUtils;

/**
 * Helper methods for sync preferences.
 */
public class SyncPreferenceUtils {
    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";
    private static final String MY_ACCOUNT_URL = "https://myaccount.google.com/smartlink/home";

    /**
     * Checks if sync error icon should be shown. Show sync error icon if sync is off because
     * of error, passphrase required or disabled in Android.
     */
    public static boolean showSyncErrorIcon(Context context) {
        if (!AndroidSyncSettings.get().isMasterSyncEnabled()) {
            return true;
        }

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService != null) {
            if (profileSyncService.hasUnrecoverableError()) {
                return true;
            }

            if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE) {
                return true;
            }

            if (profileSyncService.isSyncActive()
                    && profileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
                return true;
            }
        }

        return false;
    }

    /**
     * Return a short summary of the current sync status.
     */
    public static String getSyncStatusSummary(Context context) {
        if (!ChromeSigninController.get().isSignedIn()) return "";

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        Resources res = context.getResources();

        if (!AndroidSyncSettings.get().isMasterSyncEnabled()) {
            return res.getString(R.string.sync_android_master_sync_disabled);
        }

        if (profileSyncService == null) {
            return res.getString(R.string.sync_is_disabled);
        }

        if (profileSyncService.isSyncDisabledByEnterprisePolicy()) {
            return res.getString(R.string.sync_is_disabled_by_administrator);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                && !profileSyncService.isFirstSetupComplete()) {
            return res.getString(R.string.sync_settings_not_confirmed);
        }

        if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE) {
            return res.getString(
                    GoogleServiceAuthError.getMessageID(profileSyncService.getAuthError()));
        }

        if (profileSyncService.requiresClientUpgrade()) {
            return res.getString(
                    R.string.sync_error_upgrade_client, BuildInfo.getInstance().hostPackageLabel);
        }

        if (profileSyncService.hasUnrecoverableError()) {
            return res.getString(R.string.sync_error_generic);
        }

        String accountName = ChromeSigninController.get().getSignedInAccountName();
        boolean syncEnabled = AndroidSyncSettings.get().isSyncEnabled();
        if (syncEnabled) {
            if (!profileSyncService.isSyncActive()) {
                return res.getString(R.string.sync_setup_progress);
            }

            if (profileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
                return res.getString(R.string.sync_need_passphrase);
            }
            return context.getString(R.string.sync_and_services_summary_sync_on);
        }
        return context.getString(R.string.sync_is_disabled);
    }

    /**
     * Returns an icon that represents the current sync state.
     */
    public static @Nullable Drawable getSyncStatusIcon(Context context) {
        if (!ChromeSigninController.get().isSignedIn()) return null;

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null || !AndroidSyncSettings.get().isSyncEnabled()) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_green_40dp, R.color.default_icon_color);
        }

        if (profileSyncService.isSyncDisabledByEnterprisePolicy()) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_error_40dp, R.color.default_icon_color);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                && !profileSyncService.isFirstSetupComplete()) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_error_40dp, R.color.default_red);
        }

        if (profileSyncService.isEngineInitialized()
                && (profileSyncService.hasUnrecoverableError()
                        || profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE
                        || profileSyncService.isPassphraseRequiredForPreferredDataTypes())) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_error_40dp, R.color.default_red);
        }

        return UiUtils.getTintedDrawable(
                context, R.drawable.ic_sync_green_40dp, R.color.default_green);
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
            profileSyncService.requestStart();
        } else {
            RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                    StopSource.CHROME_SYNC_SETTINGS, StopSource.STOP_SOURCE_LIMIT);
            profileSyncService.requestStop();
        }
    }

    /**
     * Creates a wrapper around {@link Runnable} that calls the runnable only if
     * {@link PreferenceFragment} is still in resumed state. Click events that arrive after the
     * fragment has been paused will be ignored. See http://b/5983282.
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
        assert ChromeSigninController.get().isSignedIn();
        RecordUserAction.record("SyncPreferences_ManageGoogleAccountClicked");
        openCustomTabWithURL(activity, MY_ACCOUNT_URL);
    }
}
