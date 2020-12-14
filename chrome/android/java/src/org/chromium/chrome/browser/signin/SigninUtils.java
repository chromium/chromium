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
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerDelegateImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
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

    @CalledByNative
    private static void openAccountManagementScreen(WindowAndroid windowAndroid,
            @GAIAServiceType int gaiaServiceType, @Nullable String email) {
        ThreadUtils.assertOnUiThread();
        AccountManagementFragment.openAccountManagementScreen(gaiaServiceType);
    }

    @CalledByNative
    @VisibleForTesting
    static void openAccountPickerBottomSheet(WindowAndroid windowAndroid, String continueUrl) {
        ThreadUtils.assertOnUiThread();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        if (!signinManager.isSignInAllowed()) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED);
            return;
        }
        if (AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts().isEmpty()) {
            // TODO(https://crbug.com/1119720): Show the bottom sheet when no accounts on device
            //  in the future. This disabling is only temporary.
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS);
            return;
        }
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            // The bottomSheetController can be null when google.com is just opened inside a
            // bottom sheet for example. In this case, it's better to disable the account picker
            // bottom sheet.
            return;
        }

        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        // To close the current regular tab after the user clicks on "Continue" in the incognito
        // interstitial.
        TabModel regularTabModel = activity.getTabModelSelector().getModel(/*incognito=*/false);
        // To create a new incognito tab after after the user clicks on "Continue" in the incognito
        // interstitial.
        TabCreator incognitoTabCreator = activity.getTabCreator(/*incognito=*/true);
        AccountPickerBottomSheetCoordinator coordinator = new AccountPickerBottomSheetCoordinator(
                activity, bottomSheetController,
                new AccountPickerDelegateImpl(windowAndroid, activity.getActivityTab(),
                        new WebSigninBridge.Factory(), continueUrl),
                regularTabModel, incognitoTabCreator, HelpAndFeedbackLauncherImpl.getInstance());
    }

    /**
     * Launches the {@link SigninActivity} if signin is allowed.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @return a boolean indicating if the SigninActivity is launched.
     */
    public static boolean startSigninActivityIfAllowed(
            Context context, @SigninAccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        if (signinManager.isSignInAllowed()) {
            SigninActivityLauncherImpl.get().launchActivity(context, accessPoint);
            return true;
        }
        if (signinManager.isSigninDisabledByPolicy()) {
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        }
        return false;
    }
}
