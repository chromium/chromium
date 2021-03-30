// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

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
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
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
        if (!FirstRunUtils.canAllowSync() || !signinManager.isSignInAllowed()
                || TextUtils.isEmpty(accountName)) {
            setFirstRunFlowSignInComplete(true);
            return;
        }

        // TODO(https://crbug.com/795292): Move this to SigninFirstRunFragment.
        Account account = AccountUtils.findAccountByName(
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(), accountName);
        if (account == null) {
            setFirstRunFlowSignInComplete(true);
            return;
        }

        final boolean setUp = getFirstRunFlowSignInSetup();
        signinManager.signinAndEnableSync(
                SigninAccessPoint.START_PAGE, account, new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), true);
                        // Show sync settings if user pressed the "Settings" button.
                        if (setUp) {
                            openSignInSettings(activity);
                        } else {
                            ProfileSyncService.get().setFirstSetupComplete(
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
     * Opens sign in settings as requested in the FRE sign-in dialog.
     */
    private static void openSignInSettings(Activity activity) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            settingsLauncher.launchSettingsActivity(
                    activity, ManageSyncSettings.class, ManageSyncSettings.createArguments(true));
        } else {
            settingsLauncher.launchSettingsActivity(activity, SyncAndServicesSettings.class,
                    SyncAndServicesSettings.createArguments(true));
        }
    }

    /**
     * @return Whether there is no pending sign-in requests from the First Run Experience.
     */
    @VisibleForTesting
    public static boolean getFirstRunFlowSignInComplete() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_COMPLETE, false);
    }

    /**
     * Sets the "pending First Run Experience sign-in requests" preference.
     * @param isComplete Whether there is no pending sign-in requests from the First Run Experience.
     */
    @VisibleForTesting
    public static void setFirstRunFlowSignInComplete(boolean isComplete) {
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
    private static void setFirstRunFlowSignInAccountName(String accountName) {
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
    private static void setFirstRunFlowSignInSetup(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP, isComplete);
    }

    /**
     * Finalize the state of the FRE flow (mark is as "complete" and finalize parameters).
     * @param signInAccountName The account name for the pending sign-in request. (Or null)
     * @param showSignInSettings Whether the user selected to see the settings once signed in.
     */
    public static void finalizeFirstRunFlowState(
            String signInAccountName, boolean showSignInSettings) {
        FirstRunStatus.setFirstRunFlowComplete(true);
        setFirstRunFlowSignInAccountName(signInAccountName);
        setFirstRunFlowSignInSetup(showSignInSettings);
    }

    /**
     * Allows the user to sign-in if there are no pending FRE sign-in requests.
     */
    public static void updateSigninManagerFirstRunCheckDone() {
        SigninManager manager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        if (manager.isSignInAllowed()) return;
        if (!FirstRunStatus.getFirstRunFlowComplete()) return;
        if (!getFirstRunFlowSignInComplete()) return;
        manager.onFirstRunCheckDone();
    }
}
