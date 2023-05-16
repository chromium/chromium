// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

import java.util.List;

/**
 * This class regroups sign-in checks when chrome starts up and when accounts change on device
 */
public class SigninChecker implements AccountTrackerService.Observer {
    private static final String TAG = "SigninChecker";
    private final SigninManager mSigninManager;
    private final AccountTrackerService mAccountTrackerService;
    private final AccountManagerFacade mAccountManagerFacade;
    // Counter to record the number of child account checks done for tests.
    private int mNumOfChildAccountChecksDone;

    /**
     * Please use {@link SigninCheckerProvider} to get {@link SigninChecker} instance instead of
     * creating it manually.
     */
    public SigninChecker(SigninManager signinManager, AccountTrackerService accountTrackerService) {
        mSigninManager = signinManager;
        mAccountTrackerService = accountTrackerService;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mNumOfChildAccountChecksDone = 0;

        mAccountTrackerService.addObserver(this);
    }

    private void validateAccountSettings() {
        mAccountManagerFacade.getAccounts().then(accounts -> {
            mAccountTrackerService.seedAccountsIfNeeded(() -> {
                mSigninManager.runAfterOperationInProgress(() -> {
                    validatePrimaryAccountExists(accounts, /*accountsChanged=*/false);
                    checkChildAccount(accounts);
                });
            });
        });
    }

    /**
     * This method is invoked every time the accounts on device are seeded.
     */
    @Override
    public void onAccountsSeeded(List<CoreAccountInfo> accountInfos, boolean accountsChanged) {
        final List<Account> accounts = AccountUtils.toAndroidAccounts(accountInfos);
        mSigninManager.runAfterOperationInProgress(() -> {
            validatePrimaryAccountExists(accounts, accountsChanged);
            checkChildAccount(accounts);
        });
    }

    @VisibleForTesting
    public int getNumOfChildAccountChecksDoneForTests() {
        return mNumOfChildAccountChecksDone;
    }

    /**
     * Validates that the primary account exists on device.
     */
    private void validatePrimaryAccountExists(List<Account> accounts, boolean accountsChanged) {
        final CoreAccountInfo oldAccount =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        boolean oldSyncConsent =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SYNC)
                != null;
        if (oldAccount == null) {
            // Do nothing if user is not signed in
            return;
        }
        if (AccountUtils.findAccountByName(accounts, oldAccount.getEmail()) != null) {
            // Reload the accounts if the primary account is still on device and this is triggered
            // by an accounts change event.
            if (accountsChanged) {
                mSigninManager.reloadAllAccountsFromSystem(oldAccount.getId());
            }
            return;
        }
        // Check whether the primary account is renamed to another account when it is not on device
        AccountRenameChecker.get()
                .getNewNameOfRenamedAccountAsync(oldAccount.getEmail(), accounts)
                .then(newAccountName -> {
                    if (newAccountName != null) {
                        // Sign in to the new account if the current primary account is renamed to
                        // a new account.
                        resigninAfterAccountRename(newAccountName, oldSyncConsent);
                    } else {
                        // Sign out if the current primary account is not renamed
                        mSigninManager.runAfterOperationInProgress(() -> {
                            mSigninManager.signOut(SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
                        });
                    }
                });
    }

    private void resigninAfterAccountRename(String newAccountName, boolean shouldEnableSync) {
        mSigninManager.signOut(SignoutReason.ACCOUNT_EMAIL_UPDATED, () -> {
            if (shouldEnableSync) {
                mSigninManager.signinAndEnableSync(
                        AccountUtils.createAccountFromName(newAccountName),
                        SigninAccessPoint.ACCOUNT_RENAMED, new SignInCallback() {
                            @Override
                            public void onSignInComplete() {
                                SyncService.get().setInitialSyncFeatureSetupComplete(
                                        SyncFirstSetupCompleteSource.BASIC_FLOW);
                            }

                            @Override
                            public void onSignInAborted() {}
                        });
            } else {
                mSigninManager.signin(AccountUtils.createAccountFromName(newAccountName),
                        SigninAccessPoint.ACCOUNT_RENAMED, null);
            }
        }, false);
    }

    private void checkChildAccount(List<Account> accounts) {
        AccountUtils.checkChildAccountStatus(
                mAccountManagerFacade, accounts, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable Account childAccount) {
        if (isChild) {
            assert childAccount != null;
            mSigninManager.runAfterOperationInProgress(() -> {
                if (mSigninManager.isSigninAllowed()) {
                    Log.d(TAG, "The child account sign-in starts.");

                    final SignInCallback signInCallback = new SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            ++mNumOfChildAccountChecksDone;
                        }

                        @Override
                        public void onSignInAborted() {}
                    };
                    mSigninManager.wipeSyncUserData(() -> {
                        RecordUserAction.record("Signin_Signin_WipeDataOnChildAccountSignin2");
                        mSigninManager.signin(
                                childAccount, SigninAccessPoint.FORCED_SIGNIN, signInCallback);
                    });
                    return;
                }
            });
        }
        ++mNumOfChildAccountChecksDone;
    }

    /**
     * Called once when Chrome starts.
     * Responsible for checking if configuration has changed since Chrome was last launched
     * and updates state accordingly.
     */
    public void onMainActivityStart() {
        try (TraceEvent ignored = TraceEvent.scoped("SigninChecker.onMainActivityStart")) {
            validateAccountSettings();
        }
    }
}
