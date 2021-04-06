// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.util.List;

/**
 * A helper to perform all necessary steps for forced sign in.
 * The helper performs:
 * - necessary child account checks;
 * - automatic non-interactive sign in for child accounts; and
 * The helper calls the observer's onSignInComplete() if
 * - nothing needs to be done, or when
 * - the sign in is complete.
 *
 * Usage:
 * ForcedSigninProcessor.start().
 */
public final class ForcedSigninProcessor {
    /*
     * Only for static usage.
     */
    private ForcedSigninProcessor() {}

    /**
     * Check whether an automatic signin is required and process it if it is.
     * This is triggered once per Chrome Application lifetime and every time the Account state
     * changes with early exit if an account has already been signed in.
     */
    public static void start() {
        ChildAccountService.checkChildAccountStatus(status -> {
            if (ChildAccountStatus.isChild(status)) {
                final AccountManagerFacade accountManagerFacade =
                        AccountManagerFacadeProvider.getInstance();
                // Account cache is already available when child account status is ready.
                final List<Account> accounts = accountManagerFacade.tryGetGoogleAccounts();
                assert accounts.size() == 1 : "Child account should be the only account on device!";
                new AsyncTask<String>() {
                    @Override
                    protected String doInBackground() {
                        return accountManagerFacade.getAccountGaiaId(accounts.get(0).name);
                    }

                    @Override
                    protected void onPostExecute(String accountGaiaId) {
                        final CoreAccountInfo coreAccountInfo =
                                CoreAccountInfo.createFromEmailAndGaiaId(
                                        accounts.get(0).name, accountGaiaId);
                        signinAndEnableSync(coreAccountInfo);
                    }
                }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        });
    }

    /**
     * Processes the fully automatic non-FRE-related forced sign-in.
     * This is used to enforce the environment for child accounts.
     */
    private static void signinAndEnableSync(final CoreAccountInfo childAccount) {
        final Profile profile = Profile.getLastUsedRegularProfile();
        if (IdentityServicesProvider.get().getIdentityManager(profile).hasPrimaryAccount()) {
            // TODO(https://crbug.com/1044206): Remove this.
            ProfileSyncService.get().setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
        }
        final SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(profile);
        // By definition we have finished all the checks for first run.
        signinManager.onFirstRunCheckDone();
        if (signinManager.isSignInAllowed()) {
            signinManager.signinAndEnableSync(SigninAccessPoint.FORCED_SIGNIN, childAccount,
                    new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            // TODO(https://crbug.com/1044206): Remove this.
                            ProfileSyncService syncService = ProfileSyncService.get();
                            if (syncService == null) {
                                // Sync was disabled with a command-line flag, skip sign-in.
                                return;
                            }
                            syncService.setFirstSetupComplete(
                                    SyncFirstSetupCompleteSource.BASIC_FLOW);
                        }

                        @Override
                        public void onSignInAborted() {}
                    });
        }
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
