// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.auth.AccountChangeEvent;
import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * A helper for tasks like re-signin.
 *
 * This should be merged into SigninManager when it is upstreamed.
 */
public class SigninHelper implements ApplicationStatus.ApplicationStateListener {
    private static final String TAG = "SigninHelper";

    /**
     * Retrieve more detailed information from account changed intents.
     */
    public interface AccountChangeEventChecker {
        List<String> getAccountChangeEvents(Context context, int index, String accountName);
    }

    /**
     * Uses GoogleAuthUtil.getAccountChangeEvents to detect if account
     * renaming has occurred.
     */
    private static final class SystemAccountChangeEventChecker
            implements SigninHelper.AccountChangeEventChecker {
        @Override
        public List<String> getAccountChangeEvents(Context context, int index, String accountName) {
            final List<String> changedNames = new ArrayList<>();
            try {
                List<AccountChangeEvent> accountChangeEvents =
                        GoogleAuthUtil.getAccountChangeEvents(context, index, accountName);
                for (AccountChangeEvent event : accountChangeEvents) {
                    if (event.getChangeType() == GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_RENAMED_TO) {
                        changedNames.add(event.getChangeData());
                    }
                }
            } catch (IOException | GoogleAuthException e) {
                Log.w(TAG, "Failed to get change events", e);
            }
            return changedNames;
        }
    }

    private final SigninManager mSigninManager;

    private final AccountTrackerService mAccountTrackerService;

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
        ApplicationStatus.registerApplicationStateListener(this);
    }

    public void validateAccountSettings(boolean accountsChanged) {
        // validateAccountsInternal accesses account list (to check whether account exists), so
        // postpone the call until account list cache in AccountManagerFacade is ready.
        AccountManagerFacadeProvider.getInstance().runAfterCacheIsPopulated(
                () -> validateAccountsInternal(accountsChanged));
    }

    private void validateAccountsInternal(boolean accountsChanged) {
        // Ensure System accounts have been seeded.
        mAccountTrackerService.checkAndSeedSystemAccounts();
        if (mSigninManager.isOperationInProgress()) {
            // Wait for ongoing sign-in/sign-out operation to finish before validating accounts.
            mSigninManager.runAfterOperationInProgress(
                    () -> validateAccountsInternal(accountsChanged));
            return;
        }

        final CoreAccountInfo oldAccount =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SYNC);
        if (oldAccount == null) {
            return;
        }

        AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(accounts -> {
            if (AccountUtils.findAccountByName(accounts, oldAccount.getEmail()) != null) {
                // Do nothing if the signed-in account is still on device
                return;
            }
            // If the signed-in account is no longer on device, check if it is renamed to
            // another account existing on device.
            new AsyncTask<String>() {
                @Override
                protected String doInBackground() {
                    return getNewNameOfRenamedAccount(
                            new SystemAccountChangeEventChecker(), oldAccount.getEmail(), accounts);
                }

                @Override
                protected void onPostExecute(String newAccountName) {
                    if (newAccountName != null) {
                        // Sign in to the new account if the current account is renamed
                        // to a new account
                        signOutAndThenSignin(newAccountName);
                    } else {
                        mSigninManager.signOut(SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
                    }
                }
            }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        });

        if (accountsChanged) {
            // Account details have changed so inform the token service that credentials
            // should now be available.
            mSigninManager.reloadAllAccountsFromSystem();
        }
    }

    /**
     * Perform a sign-out with a callback to sign-in again.
     */
    private void signOutAndThenSignin(String accountEmail) {
        final Account account = AccountUtils.createAccountFromName(accountEmail);
        final SigninManager.SignInCallback signinCallback = new SigninManager.SignInCallback() {
            @Override
            public void onSignInComplete() {
                validateAccountsInternal(true);
            }

            @Override
            public void onSignInAborted() {}
        };
        mSigninManager.signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, () -> {
            // Clear the shared perf only after signOut is successful.
            // If Chrome dies, we can try it again on next run.
            // Otherwise, if re-sign-in fails, we'll just leave chrome
            // signed-out.
            mSigninManager.signinAndEnableSync(
                    SigninAccessPoint.ACCOUNT_RENAMED, account, signinCallback);
        }, false);
    }

    @VisibleForTesting
    static @Nullable String getNewNameOfRenamedAccount(
            AccountChangeEventChecker checker, String oldAccountName, List<Account> accounts) {
        final List<String> changedNames = checker.getAccountChangeEvents(
                ContextUtils.getApplicationContext(), 0, oldAccountName);
        for (String newAccountName : changedNames) {
            if (newAccountName != null) {
                if (AccountUtils.findAccountByName(accounts, newAccountName) != null) {
                    return newAccountName;
                }
                // When the new name does not exist on device, check if it is renamed to
                // another one existing on device
                return getNewNameOfRenamedAccount(checker, newAccountName, accounts);
            }
        }
        return null;
    }

    /**
     * Called once during initialization and then again for every start (warm-start).
     * Responsible for checking if configuration has changed since Chrome was last launched
     * and updates state accordingly.
     */
    public void onMainActivityStart() {
        try (TraceEvent ignored = TraceEvent.scoped("SigninHelper.onMainActivityStart")) {
            boolean accountsChanged =
                    SigninPreferencesManager.getInstance().checkAndClearAccountsChangedPref();
            validateAccountSettings(accountsChanged);
        }
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            onMainActivityStart();
        }
    }
}
