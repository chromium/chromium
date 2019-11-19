// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.sync.AccountManagementFragment;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.SigninActivityMonitor;
import org.chromium.ui.base.WindowAndroid;

/**
 * Helper functions for sign-in and accounts.
 */
public class SigninUtils {
    private static final String ACCOUNT_SETTINGS_ACTION = "android.settings.ACCOUNT_SYNC_SETTINGS";
    private static final String ACCOUNT_SETTINGS_ACCOUNT_KEY = "account";

    private SigninUtils() {}

    /**
     * Opens a Settings page to configure settings for a single account.
     * Note: on Android O+, this method is identical to {@link #openSettingsForAllAccounts}.
     * @param context Context to use when starting the Activity.
     * @param account The account for which the Settings page should be opened.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAccount(Context context, Account account) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // ACCOUNT_SETTINGS_ACTION no longer works on Android O+, always open all accounts page.
            return openSettingsForAllAccounts(context);
        }
        Intent intent = new Intent(ACCOUNT_SETTINGS_ACTION);
        intent.putExtra(ACCOUNT_SETTINGS_ACCOUNT_KEY, account);
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return IntentUtils.safeStartActivity(context, intent);
    }

    // TODO(https://crbug.com/955501): Migrate all clients to WindowAndroid and remove this.
    /**
     * Opens a Settings page with all accounts on the device.
     * @param context Context to use when starting the Activity.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAllAccounts(Context context) {
        Intent intent = new Intent(Settings.ACTION_SYNC_SETTINGS);
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return IntentUtils.safeStartActivity(context, intent);
    }

    /**
     * Opens a Settings page with all accounts on the device.
     * @param windowAndroid WindowAndroid to use when starting the Activity.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAllAccounts(WindowAndroid windowAndroid) {
        Intent intent = new Intent(Settings.ACTION_SYNC_SETTINGS);
        return startActivity(windowAndroid, intent);
    }

    @CalledByNative
    private static void openAccountManagementScreen(WindowAndroid windowAndroid,
            @GAIAServiceType int gaiaServiceType, @Nullable String email) {
        ThreadUtils.assertOnUiThread();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            // If Mice is enabled, directly use the system account management flows.
            switch (gaiaServiceType) {
                case GAIAServiceType.GAIA_SERVICE_TYPE_SIGNUP:
                case GAIAServiceType.GAIA_SERVICE_TYPE_ADDSESSION:
                    AccountManagerFacade accountManagerFacade = AccountManagerFacade.get();
                    @Nullable
                    Account account =
                            email == null ? null : accountManagerFacade.getAccountFromName(email);
                    if (account == null) {
                        // Empty or unknown account: add a new account.
                        // TODO(bsazonov): if email is not empty, pre-fill the account name.
                        startAddAccountActivity(windowAndroid, gaiaServiceType);
                    } else {
                        // Existing account indicates authentication error. Fix it.
                        accountManagerFacade.updateCredentials(
                                account, windowAndroid.getActivity().get(), null);
                    }
                    break;
                default:
                    // Open generic accounts settings.
                    openSettingsForAllAccounts(windowAndroid);
                    break;
            }
            return;
        }

        // If Mice is not enabled, open Chrome's account management screen.
        AccountManagementFragment.openAccountManagementScreen(gaiaServiceType);
    }

    /**
     * Tries starting an Activity to add a Google account to the device. If this activity cannot
     * be started, opens "Accounts" page in the Android Settings app.
     */
    private static void startAddAccountActivity(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceTypeSignup) {
        logEvent(ProfileAccountManagementMetrics.DIRECT_ADD_ACCOUNT, gaiaServiceTypeSignup);

        AccountManagerFacade.get().createAddAccountIntent((@Nullable Intent intent) -> {
            if (intent != null && startActivity(windowAndroid, intent)) {
                return;
            }
            // Failed to create or show an intent, open settings for all accounts so
            // the user has a chance to create an account manually.
            SigninUtils.openSettingsForAllAccounts(windowAndroid);
        });
    }

    // TODO(https://crbug.com/953765): Move this to SigninActivityMonitor.
    /**
     * Starts an activity using the provided intent. The started activity will be tracked by
     * {@link SigninActivityMonitor#hasOngoingActivity()}.
     *
     * @param windowAndroid The window to use when launching the intent.
     * @param intent The intent to launch.
     * @return Whether {@link WindowAndroid#showIntent} succeeded.
     */
    private static boolean startActivity(WindowAndroid windowAndroid, Intent intent) {
        SigninActivityMonitor signinActivityMonitor = SigninActivityMonitor.get();
        WindowAndroid.IntentCallback intentCallback =
                (window, resultCode, data) -> signinActivityMonitor.activityFinished();
        if (windowAndroid.showIntent(intent, intentCallback, null)) {
            signinActivityMonitor.activityStarted();
            return true;
        }
        return false;
    }

    /**
     * Log a UMA event for a given metric and a signin type.
     * @param metric One of ProfileAccountManagementMetrics constants.
     * @param gaiaServiceType A signin::GAIAServiceType.
     */
    public static void logEvent(int metric, int gaiaServiceType) {
        SigninUtilsJni.get().logEvent(metric, gaiaServiceType);
    }

    @NativeMethods
    interface Natives {
        void logEvent(int metric, int gaiaServiceType);
    }
}
