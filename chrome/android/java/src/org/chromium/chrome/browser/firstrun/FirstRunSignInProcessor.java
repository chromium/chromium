// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.sync.SyncAndServicesPreferences;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.ProfileSyncService;

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
     * SharedPreferences preference names to keep the state of the First Run Experience.
     */
    private static final String FIRST_RUN_FLOW_SIGNIN_COMPLETE = "first_run_signin_complete";

    // Needed by ChromeBackupAgent
    public static final String FIRST_RUN_FLOW_SIGNIN_SETUP = "first_run_signin_setup";
    public static final String FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME =
            "first_run_signin_account_name";

    /**
     * Initiates the automatic sign-in process in background.
     *
     * @param activity The context for the FRE parameters processor.
     */
    public static void start(final Activity activity) {
        SigninManager signinManager = IdentityServicesProvider.getSigninManager();
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

        final boolean setUp = getFirstRunFlowSignInSetup();
        signinManager.signIn(accountName, new SignInCallback() {
            @Override
            public void onSignInComplete() {
                UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);
                // Show sync settings if user pressed the "Settings" button.
                if (setUp) {
                    openSignInSettings(activity);
                } else if (ChromeFeatureList.isEnabled(
                                   ChromeFeatureList.SYNC_MANUAL_START_ANDROID)) {
                    ProfileSyncService.get().setFirstSetupComplete(
                            SyncFirstSetupCompleteSource.BASIC_FLOW);
                }
                setFirstRunFlowSignInComplete(true);
            }

            @Override
            public void onSignInAborted() {
                // Set FRE as complete even if signin fails because the user has already seen and
                // accepted the terms of service.
                setFirstRunFlowSignInComplete(true);
            }
        });
    }

    /**
     * Opens sign in settings as requested in the FRE sign-in dialog.
     */
    private static void openSignInSettings(Activity activity) {
        final Class<? extends Fragment> fragment = SyncAndServicesPreferences.class;
        final Bundle arguments = SyncAndServicesPreferences.createArguments(true);
        PreferencesLauncher.launchSettingsPage(activity, fragment, arguments);
    }

    /**
     * @return Whether there is no pending sign-in requests from the First Run Experience.
     */
    @VisibleForTesting
    public static boolean getFirstRunFlowSignInComplete() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(FIRST_RUN_FLOW_SIGNIN_COMPLETE, false);
    }

    /**
     * Sets the "pending First Run Experience sign-in requests" preference.
     * @param isComplete Whether there is no pending sign-in requests from the First Run Experience.
     */
    @VisibleForTesting
    public static void setFirstRunFlowSignInComplete(boolean isComplete) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(FIRST_RUN_FLOW_SIGNIN_COMPLETE, isComplete)
                .apply();
    }

    /**
     * @return The account name selected during the First Run Experience, or null if none.
     */
    private static String getFirstRunFlowSignInAccountName() {
        return ContextUtils.getAppSharedPreferences()
                .getString(FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME, null);
    }

    /**
     * Sets the account name for the pending sign-in First Run Experience request.
     * @param accountName The account name, or null.
     */
    private static void setFirstRunFlowSignInAccountName(String accountName) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME, accountName)
                .apply();
    }

    /**
     * @return Whether the user selected to see the settings once signed in after FRE.
     */
    private static boolean getFirstRunFlowSignInSetup() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                FIRST_RUN_FLOW_SIGNIN_SETUP, false);
    }

    /**
     * Sets the preference to see the settings once signed in after FRE.
     * @param isComplete Whether the user selected to see the settings once signed in.
     */
    private static void setFirstRunFlowSignInSetup(boolean isComplete) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(FIRST_RUN_FLOW_SIGNIN_SETUP, isComplete)
                .apply();
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
        SigninManager manager = IdentityServicesProvider.getSigninManager();
        if (manager.isSignInAllowed()) return;
        if (!FirstRunStatus.getFirstRunFlowComplete()) return;
        if (!getFirstRunFlowSignInComplete()) return;
        manager.onFirstRunCheckDone();
    }
}
