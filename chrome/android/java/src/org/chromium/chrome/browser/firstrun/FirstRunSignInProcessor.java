// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * A helper to perform all necessary steps for the automatic FRE sign in.
 * The helper performs any pending request to sign in from the First Run Experience.
 * The helper calls the observer's onSignInComplete() if
 * - nothing needs to be done, or when
 * - the sign in is complete.
 * If the sign in process fails or if an interactive FRE sequence is necessary,
 * the helper starts the FRE activity, finishes the current activity and calls
 * OnSignInCancelled.
 *
 * Usage:
 * FirstRunSignInProcessor.start(activity).
 */
public final class FirstRunSignInProcessor {

    /**
     * Initiates the automatic sign-in process in background.
     *
     * @param activity The context for the FRE parameters processor.
     */
    public static void start(final Activity activity) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        signinManager.onFirstRunCheckDone();

        // Skip signin if the first run flow is not complete. Examples of cases where the user
        // would not have gone through the FRE:
        // - FRE is disabled, or
        // - FRE hasn't been completed, but the user has already seen the ToS in the Setup Wizard.
        if (!FirstRunStatus.getFirstRunFlowComplete()) {
            return;
        }

        // We are only processing signin from the FRE.
        if (getFirstRunFlowSignInComplete()) {
            return;
        }

        final String accountName = getFirstRunFlowSignInAccountName();
        if (TextUtils.isEmpty(accountName) && getFirstRunFlowSignInSetup()) {
            assert ChromeFeatureList.isEnabled(ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE);
            openAdvancedSyncSettings(activity);
            setFirstRunFlowSignInComplete(true);
        }

        if (!FirstRunUtils.canAllowSync() || !signinManager.isSyncOptInAllowed()
                || TextUtils.isEmpty(accountName)) {
            setFirstRunFlowSignInComplete(true);
            return;
        }

        // TODO(https://crbug.com/795292): Move this to SyncConsentFirstRunFragment.
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        accountManagerFacade.getAccounts().then(accounts -> {
            AccountUtils.checkChildAccountStatus(
                    accountManagerFacade, accounts, (isChild, unused) -> {
                        if (isChild) {
                            // Child account sign-ins are handled by SigninChecker.
                            setFirstRunFlowSignInComplete(true);
                            return;
                        }

                        Account account = AccountUtils.findAccountByName(accounts, accountName);
                        if (account == null) {
                            setFirstRunFlowSignInComplete(true);
                            return;
                        }

                        signinAndEnableSync(account, activity);
                    });
        });
    }

    private static void signinAndEnableSync(@NonNull Account account, Activity activity) {
        final boolean showAdvancedSyncSettings = getFirstRunFlowSignInSetup();
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .signinAndEnableSync(SigninAccessPoint.START_PAGE, account, new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), true);
                        // Show sync settings if user pressed the "Settings" button.
                        if (showAdvancedSyncSettings) {
                            openAdvancedSyncSettings(activity);
                        } else {
                            SyncService.get().setFirstSetupComplete(
                                    SyncFirstSetupCompleteSource.BASIC_FLOW);
                        }
                        setFirstRunFlowSignInComplete(true);
                    }

                    @Override
                    public void onSignInAborted() {
                        // Set FRE as complete even if signin fails because the user has already
                        // seen and accepted the terms of service.
                        setFirstRunFlowSignInComplete(true);
                    }
                });
    }

    /**
     * Opens advanced sync settings as requested in the FRE sync consent page.
     */
    private static void openAdvancedSyncSettings(Activity activity) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(
                activity, ManageSyncSettings.class, ManageSyncSettings.createArguments(true));
    }

    /**
     * @return Whether there is no pending sign-in requests from the First Run Experience.
     */
    private static boolean getFirstRunFlowSignInComplete() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_COMPLETE, false);
    }

    /**
     * Sets the "pending First Run Experience sign-in requests" preference.
     * @param isComplete Whether there is no pending sign-in requests from the First Run Experience.
     */
    private static void setFirstRunFlowSignInComplete(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_COMPLETE, isComplete);
    }

    /**
     * @return The account name selected during the First Run Experience, or null if none.
     */
    private static String getFirstRunFlowSignInAccountName() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME, null);
    }

    /**
     * Sets the account name for the pending sign-in First Run Experience request.
     * @param accountName The account name, or null.
     */
    public static void setFirstRunFlowSignInAccountName(String accountName) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME, accountName);
    }

    /**
     * @return Whether the user selected to see the settings once signed in after FRE.
     */
    private static boolean getFirstRunFlowSignInSetup() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, false);
    }

    /**
     * Sets the preference to see the settings once signed in after FRE.
     * @param isComplete Whether the user selected to see the settings once signed in.
     */
    public static void setFirstRunFlowSignInSetup(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, isComplete);
    }
}
