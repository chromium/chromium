// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

import java.util.List;

/**
 * A helper for tasks like re-signin.
 *
 * This should be merged into SigninManager when it is upstreamed.
 */
public class SigninHelper implements ApplicationStatus.ApplicationStateListener {
    private final SigninManager mSigninManager;
    private final AccountTrackerService mAccountTrackerService;
    private final AccountRenameChecker mAccountRenameChecker;

    /**
     * Please use SigninHelperProvider to get SigninHelper instance instead of creating it
     * manually.
     *
     * TODO(crbug/1152460): Add integration tests for the rename flow to check that if
     * a signed-in account A is renamed to the account B, the signed-in account will be
     * account B.
     */
    public SigninHelper(SigninManager signinManager, AccountTrackerService accountTrackerService) {
        mSigninManager = signinManager;
        mAccountTrackerService = accountTrackerService;
        mAccountRenameChecker = new AccountRenameChecker();
        ApplicationStatus.registerApplicationStateListener(this);
    }

    public void validateAccountSettings() {
        AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(accounts -> {
            mAccountTrackerService.seedAccountsIfNeeded(() -> {
                mSigninManager.runAfterOperationInProgress(
                        () -> { validateSignedInAccountExists(accounts); });
            });
        });
    }

    private void validateSignedInAccountExists(List<Account> accounts) {
        final CoreAccountInfo oldAccount =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SYNC);
        if (oldAccount == null) {
            return;
        }

        if (AccountUtils.findAccountByName(accounts, oldAccount.getEmail()) != null) {
            // Do nothing if the signed-in account is still on device
            return;
        }

        // If the signed-in account is no longer on device, check if it is renamed to
        // another account existing on device.
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
                return mAccountRenameChecker.getNewNameOfRenamedAccount(
                        oldAccount.getEmail(), accounts);
            }

            @Override
            protected void onPostExecute(String newAccountName) {
                if (newAccountName != null) {
                    // Sign in to the new account if the current account is renamed
                    // to a new account
                    mSigninManager.signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, () -> {
                        mSigninManager.signinAndEnableSync(SigninAccessPoint.ACCOUNT_RENAMED,
                                AccountUtils.createAccountFromName(newAccountName), null);
                    }, false);
                } else {
                    mSigninManager.signOut(SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
                }
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Called once during initialization and then again for every start (warm-start).
     * Responsible for checking if configuration has changed since Chrome was last launched
     * and updates state accordingly.
     */
    public void onMainActivityStart() {
        try (TraceEvent ignored = TraceEvent.scoped("SigninHelper.onMainActivityStart")) {
            validateAccountSettings();
        }
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            onMainActivityStart();
        }
    }
}
