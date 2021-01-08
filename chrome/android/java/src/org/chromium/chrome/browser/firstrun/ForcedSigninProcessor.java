// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * A helper to perform all necessary steps for forced sign in.
 * The helper performs:
 * - necessary child account checks;
 * - automatic non-interactive forced sign in for child accounts; and
 * The helper calls the observer's onSignInComplete() if
 * - nothing needs to be done, or when
 * - the sign in is complete.
 *
 * Usage:
 * ForcedSigninProcessor.start(appContext).
 */
public final class ForcedSigninProcessor {
    private static final String TAG = "ForcedSignin";

    /*
     * Only for static usage.
     */
    private ForcedSigninProcessor() {}

    /**
     * Check whether a forced automatic signin is required and process it if it is.
     * This is triggered once per Chrome Application lifetime and every time the Account state
     * changes with early exit if an account has already been signed in.
     */
    public static void start(@Nullable final Runnable onComplete) {
        ChildAccountService.checkChildAccountStatus(status -> {
            boolean hasChildAccount = ChildAccountStatus.isChild(status);
            AccountManagementFragment.setSignOutAllowedPreferenceValue(!hasChildAccount);
            if (hasChildAccount) {
                processForcedSignIn(onComplete);
            }
        });
    }

    /**
     * Processes the fully automatic non-FRE-related forced sign-in.
     * This is used to enforce the environment for Android EDU and child accounts.
     */
    private static void processForcedSignIn(@Nullable final Runnable onComplete) {
        if (FirstRunUtils.canAllowSync()
                && IdentityServicesProvider.get()
                           .getIdentityManager(Profile.getLastUsedRegularProfile())
                           .hasPrimaryAccount()) {
            // TODO(https://crbug.com/1044206): Remove this.
            ProfileSyncService.get().setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
        }

        final SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        // By definition we have finished all the checks for first run.
        signinManager.onFirstRunCheckDone();
        if (!FirstRunUtils.canAllowSync() || !signinManager.isSignInAllowed()) {
            Log.d(TAG, "Sign in disallowed");
            return;
        }
        AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(accounts -> {
            if (accounts.size() != 1) {
                Log.d(TAG, "Incorrect number of accounts (%d)", accounts.size());
                return;
            }
            signinManager.signinAndEnableSync(SigninAccessPoint.FORCED_SIGNIN, accounts.get(0),
                    new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            // TODO(https://crbug.com/1044206): Remove this.
                            ProfileSyncService.get().setFirstSetupComplete(
                                    SyncFirstSetupCompleteSource.BASIC_FLOW);
                            if (onComplete != null) {
                                onComplete.run();
                            }
                        }

                        @Override
                        public void onSignInAborted() {
                            if (onComplete != null) {
                                onComplete.run();
                            }
                        }
                    });
        });
    }

    /**
     * If forced signin is required by policy, check that Google Play Services is available, and
     * show a non-cancelable dialog otherwise.
     * @param activity The activity for which to show the dialog.
     */
    // TODO(bauerb): Once external dependencies reliably use policy to force sign-in,
    // consider removing the child account.
    public static void checkCanSignIn(final Activity activity) {
        if (IdentityServicesProvider.get()
                        .getSigninManager(Profile.getLastUsedRegularProfile())
                        .isForceSigninEnabled()) {
            ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                    new UserRecoverableErrorHandler.ModalDialog(activity, false));
        }
    }
}
