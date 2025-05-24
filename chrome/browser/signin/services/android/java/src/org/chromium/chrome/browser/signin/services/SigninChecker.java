// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

import java.util.List;

/** This class regroups sign-in checks when chrome starts up and when accounts change on device */
@NullMarked
public class SigninChecker implements AccountsChangeObserver, Destroyable {
    private static final String TAG = "SigninChecker";
    private final AccountManagerFacade mAccountManagerFacade;
    private final SigninManager mSigninManager;
    // Counter to record the number of child account checks done for tests.
    private int mNumOfChildAccountChecksDone;

    /**
     * Please use {@link SigninCheckerProvider} to get {@link SigninChecker} instance instead of
     * creating it manually.
     */
    public SigninChecker(SigninManager signinManager) {
        mSigninManager = signinManager;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        if (mAccountManagerFacade.getAccounts().isFulfilled()) {
            onCoreAccountInfosChanged();
        }
        mNumOfChildAccountChecksDone = 0;
    }

    @Override
    public void destroy() {
        mAccountManagerFacade.removeObserver(this);
    }

    @Override
    public void onCoreAccountInfosChanged() {
        var accountsPromise = mAccountManagerFacade.getAccounts();
        assert accountsPromise.isFulfilled();
        var accounts = accountsPromise.getResult();
        // In the FRE, supervised accounts are signed in by the SigninManager
        if (!FirstRunStatus.isFirstRunTriggered()) {
            checkChildAccount(accounts);
        }
    }

    public int getNumOfChildAccountChecksDoneForTests() {
        return mNumOfChildAccountChecksDone;
    }

    private void checkChildAccount(List<AccountInfo> accounts) {
        AccountUtils.checkIsSubjectToParentalControls(
                mAccountManagerFacade, accounts, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable CoreAccountInfo childInfo) {
        ++mNumOfChildAccountChecksDone;
        if (!isChild) {
            return;
        }

        assert childInfo != null;
        mSigninManager.runAfterOperationInProgress(
                () -> {
                    CoreAccountInfo accountInfo =
                            mSigninManager
                                    .getIdentityManager()
                                    .getPrimaryAccountInfo(ConsentLevel.SIGNIN);

                    if (accountInfo == null || childInfo.getId().equals(accountInfo.getId())) {
                        signInSupervisedUser(childInfo);
                    } else {
                        mSigninManager.signOut(
                                SignoutReason.SIGNOUT_BEFORE_SUPERVISED_SIGNIN,
                                () -> onChildAccountStatusReady(isChild, childInfo),
                                /* forceWipeUserData= */ false);
                    }
                });
    }

    private void signInSupervisedUser(CoreAccountInfo childInfo) {
        assert childInfo != null;
        if (!mSigninManager.isSigninAllowed()) {
            return;
        }
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
        mSigninManager.signin(childInfo, SigninAccessPoint.FORCED_SIGNIN, signInCallback);
    }
}
