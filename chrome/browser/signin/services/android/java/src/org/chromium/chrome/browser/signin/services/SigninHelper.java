// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;
import android.content.Context;
import android.util.Pair;

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
            try {
                List<AccountChangeEvent> accountChangeEvents =
                        GoogleAuthUtil.getAccountChangeEvents(context, index, accountName);
                List<String> changedNames = new ArrayList<>(accountChangeEvents.size());
                for (AccountChangeEvent event : accountChangeEvents) {
                    if (event.getChangeType() == GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_RENAMED_TO) {
                        changedNames.add(event.getChangeData());
                    } else {
                        changedNames.add(null);
                    }
                }
                return changedNames;
            } catch (IOException | GoogleAuthException e) {
                Log.w(TAG, "Failed to get change events", e);
            }
            return new ArrayList<>(0);
        }
    }

    private final SigninManager mSigninManager;

    private final AccountTrackerService mAccountTrackerService;

    private final SigninPreferencesManager mPrefsManager;

    /**
     * Please use SigninHelperProvider to get SigninHelper instance instead of creating it
     * manually.
     */
    public SigninHelper(SigninManager signinManager, AccountTrackerService accountTrackerService,
            SigninPreferencesManager signinPreferencesManager) {
        mSigninManager = signinManager;
        mAccountTrackerService = accountTrackerService;
        mPrefsManager = signinPreferencesManager;
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
        if (!accountsChanged) {
            mAccountTrackerService.validateSystemAccounts();
        }
        if (mSigninManager.isOperationInProgress()) {
            // Wait for ongoing sign-in/sign-out operation to finish before validating accounts.
            mSigninManager.runAfterOperationInProgress(
                    () -> validateAccountsInternal(accountsChanged));
            return;
        }

        final CoreAccountInfo syncAccount =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SYNC);
        if (syncAccount == null) {
            return;
        }

        String renamedAccount = mPrefsManager.getNewSignedInAccountName();
        if (accountsChanged && renamedAccount != null) {
            Log.i(TAG,
                    "handleAccountRename from: " + syncAccount.getEmail() + " to "
                            + renamedAccount);
            signOutAndThenSignin(renamedAccount);
            return;
        }

        final List<Account> accounts =
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
        // Always check for account deleted.
        if (AccountUtils.findAccountByName(accounts, syncAccount.getEmail()) == null) {
            // It is possible that Chrome got to this point without account
            // rename notification. Let us signout before doing a rename.
            AsyncTask<Void> task = new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    updateAccountRenameData(
                            new SystemAccountChangeEventChecker(), syncAccount.getEmail());
                    return null;
                }

                @Override
                protected void onPostExecute(Void result) {
                    if (mPrefsManager.getNewSignedInAccountName() != null
                            || mSigninManager.isOperationInProgress()) {
                        // Found account rename event or there's a sign-in/sign-out operation in
                        // progress. Restart validation process.
                        validateAccountsInternal(true);
                    } else {
                        mSigninManager.signOut(SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
                    }
                }
            };
            task.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
            return;
        }

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
            mPrefsManager.clearNewSignedInAccountName();
            mSigninManager.signinAndEnableSync(
                    SigninAccessPoint.ACCOUNT_RENAMED, account, signinCallback);
        }, false);
    }

    @VisibleForTesting
    public static void updateAccountRenameData(
            AccountChangeEventChecker checker, String currentAccountName) {
        final SigninPreferencesManager prefsManager = SigninPreferencesManager.getInstance();
        final int currentEventIndex = prefsManager.getLastAccountChangedEventIndex();
        final Pair<Integer, String> newEventIndexAndAccountName =
                findAccountRenameEvent(checker, currentEventIndex, currentAccountName);
        if (currentEventIndex != newEventIndexAndAccountName.first) {
            prefsManager.setLastAccountChangedEventIndex(newEventIndexAndAccountName.first);
        }
        if (!currentAccountName.equals(newEventIndexAndAccountName.second)) {
            prefsManager.setNewSignedInAccountName(newEventIndexAndAccountName.second);
        }
    }

    /**
     * Finds the account rename event.
     * @return A pair including the account change event index and the changed account name.
     *         When there's no pending rename event, the event index is still updated to the
     *         last read index to avoid reading the same data again in the future.
     */
    private static Pair<Integer, String> findAccountRenameEvent(
            AccountChangeEventChecker checker, int eventIndex, String accountName) {
        final List<Account> accounts =
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
        final List<String> changedNames = checker.getAccountChangeEvents(
                ContextUtils.getApplicationContext(), eventIndex, accountName);
        final int newEventIndex = changedNames.size();
        for (String changedName : changedNames) {
            if (changedName != null) {
                // We have found a rename event of the current account.
                // We need to check if that account is further renamed.
                if (AccountUtils.findAccountByName(accounts, changedName) == null) {
                    // Start from the beginning of the new account if account does not exist on
                    // device
                    return findAccountRenameEvent(checker, 0, changedName);
                } else {
                    return new Pair<>(newEventIndex, changedName);
                }
            }
        }
        return new Pair<>(newEventIndex, accountName);
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
