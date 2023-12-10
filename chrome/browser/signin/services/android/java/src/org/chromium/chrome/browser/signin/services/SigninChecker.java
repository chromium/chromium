// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.signin.services.SigninManager.DataWipeOption;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;

import java.util.List;

/** This class regroups sign-in checks when chrome starts up and when accounts change on device */
public class SigninChecker
        implements AccountTrackerService.Observer, AccountsChangeObserver, Destroyable {
    private static final String TAG = "SigninChecker";
    private final AccountTrackerService mAccountTrackerService;
    private final SyncService mSyncService;
    private final AccountManagerFacade mAccountManagerFacade;
    // TODO(crbug/1491005): Delete this once SeedAccountsRevamp is fully enabled.
    @Nullable private SigninManager mSigninManager;
    // Counter to record the number of child account checks done for tests.
    private int mNumOfChildAccountChecksDone;

    /**
     * Please use {@link SigninCheckerProvider} to get {@link SigninChecker} instance instead of
     * creating it manually.
     */
    public SigninChecker(
            SigninManager signinManager,
            AccountTrackerService accountTrackerService,
            SyncService syncService) {
        if (!SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            mSigninManager = signinManager;
        }
        mAccountTrackerService = accountTrackerService;
        mSyncService = syncService;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mNumOfChildAccountChecksDone = 0;
        mAccountTrackerService.addObserver(this);
    }

    @Override
    public void destroy() {
        mAccountTrackerService.removeObserver(this);
    }

    private void validateAccountSettings() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            throw new IllegalStateException(
                    "This method should never be called when SEED_ACCOUNTS_REVAMP is enabled");
        }
        mAccountManagerFacade
                .getCoreAccountInfos()
                .then(
                        coreAccountInfos -> {
                            mAccountTrackerService.legacySeedAccountsIfNeeded(
                                    () -> {
                                        mSigninManager.runAfterOperationInProgress(
                                                () -> {
                                                    validatePrimaryAccountExists(
                                                            coreAccountInfos,
                                                            /* accountsChanged= */ false);
                                                    checkChildAccount(coreAccountInfos);
                                                });
                                    });
                        });
    }

    @Override
    public void onCoreAccountInfosChanged() {
        Promise<List<CoreAccountInfo>> coreAccountInfosPromise =
                mAccountManagerFacade.getCoreAccountInfos();
        assert coreAccountInfosPromise.isFulfilled();
        List<CoreAccountInfo> coreAccountInfos = coreAccountInfosPromise.getResult();
        checkChildAccount(coreAccountInfos);
    }

    /** This method is invoked every time the accounts on device are seeded. */
    @Override
    public void legacyOnAccountsSeeded(
            List<CoreAccountInfo> coreAccountInfos, boolean accountsChanged) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            throw new IllegalStateException(
                    "This method should never be called when SEED_ACCOUNTS_REVAMP is enabled");
        }
        mSigninManager.runAfterOperationInProgress(
                () -> {
                    validatePrimaryAccountExists(coreAccountInfos, accountsChanged);
                    checkChildAccount(coreAccountInfos);
                });
    }

    public int getNumOfChildAccountChecksDoneForTests() {
        return mNumOfChildAccountChecksDone;
    }

    /** Validates that the primary account exists on device. */
    private void validatePrimaryAccountExists(
            List<CoreAccountInfo> coreAccountInfos, boolean accountsChanged) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            throw new IllegalStateException(
                    "This method should never be called when SEED_ACCOUNTS_REVAMP is enabled");
        }
        final CoreAccountInfo oldAccount =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        boolean oldSyncConsent =
                mSigninManager.getIdentityManager().getPrimaryAccountInfo(ConsentLevel.SYNC)
                        != null;
        if (oldAccount == null) {
            // Do nothing if user is not signed in
            return;
        }
        if (coreAccountInfos.contains(oldAccount)) {
            // Reload the coreAccountInfos if the primary account is still on device and this is
            // triggered by an coreAccountInfos change event.
            if (accountsChanged) {
                mSigninManager.reloadAllAccountsFromSystem(oldAccount.getId());
            }
            return;
        }
        // Check whether the primary account is renamed to another account when it is not on device
        AccountRenameChecker.get()
                .getNewNameOfRenamedAccountAsync(oldAccount.getEmail(), coreAccountInfos)
                .then(
                        newAccountName -> {
                            if (newAccountName != null) {
                                // Sign in to the new account if the current primary account is
                                // renamed to a new account.
                                resigninAfterAccountRename(newAccountName, oldSyncConsent);
                            } else {
                                // Sign out if the current primary account is not renamed
                                mSigninManager.runAfterOperationInProgress(
                                        () -> {
                                            mSigninManager.signOut(
                                                    SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
                                        });
                            }
                        });
    }

    private void resigninAfterAccountRename(String newAccountName, boolean shouldEnableSync) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            throw new IllegalStateException(
                    "This method should never be called when SEED_ACCOUNTS_REVAMP is enabled");
        }
        mSigninManager.signOut(
                SignoutReason.ACCOUNT_EMAIL_UPDATED,
                () -> {
                    if (shouldEnableSync) {
                        mSigninManager.signinAndEnableSync(
                                AccountUtils.createAccountFromName(newAccountName),
                                SigninAccessPoint.ACCOUNT_RENAMED,
                                new SignInCallback() {
                                    @Override
                                    public void onSignInComplete() {
                                        mSyncService.setInitialSyncFeatureSetupComplete(
                                                SyncFirstSetupCompleteSource.BASIC_FLOW);
                                    }

                                    @Override
                                    public void onSignInAborted() {}
                                });
                    } else {
                        mSigninManager.signin(
                                AccountUtils.createAccountFromName(newAccountName),
                                SigninAccessPoint.ACCOUNT_RENAMED,
                                null);
                    }
                },
                false);
    }

    private void checkChildAccount(List<CoreAccountInfo> coreAccountInfos) {
        AccountUtils.checkChildAccountStatus(
                mAccountManagerFacade, coreAccountInfos, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable Account childAccount) {
        if (isChild) {
            assert childAccount != null;
            mSigninManager.runAfterOperationInProgress(
                    () -> {
                        if (mSigninManager.isSigninAllowed()) {
                            Log.d(TAG, "The child account sign-in starts.");

                            final SignInCallback signInCallback =
                                    new SignInCallback() {
                                        @Override
                                        public void onSignInComplete() {
                                            ++mNumOfChildAccountChecksDone;
                                        }

                                        @Override
                                        public void onSignInAborted() {}
                                    };
                            mSigninManager.wipeSyncUserData(
                                    () -> {
                                        RecordUserAction.record(
                                                "Signin_Signin_WipeDataOnChildAccountSignin2");
                                        mSigninManager.signin(
                                                childAccount,
                                                SigninAccessPoint.FORCED_SIGNIN,
                                                signInCallback);
                                    },
                                    DataWipeOption.WIPE_SYNC_DATA);
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
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            return;
        }
        try (TraceEvent ignored = TraceEvent.scoped("SigninChecker.onMainActivityStart")) {
            validateAccountSettings();
        }
    }
}
