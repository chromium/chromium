// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.SigninManager.DataWipeOption;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.util.List;

/** This class regroups sign-in checks when chrome starts up and when accounts change on device */
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
        if (mAccountManagerFacade.getCoreAccountInfos().isFulfilled()) {
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
        Promise<List<CoreAccountInfo>> coreAccountInfosPromise =
                mAccountManagerFacade.getCoreAccountInfos();
        assert coreAccountInfosPromise.isFulfilled();
        List<CoreAccountInfo> coreAccountInfos = coreAccountInfosPromise.getResult();
        // In the FRE, supervised accounts are signed in by the SigninManager
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                || !FirstRunStatus.isFirstRunTriggered()) {
            checkChildAccount(coreAccountInfos);
        }
    }

    public int getNumOfChildAccountChecksDoneForTests() {
        return mNumOfChildAccountChecksDone;
    }

    private void checkChildAccount(List<CoreAccountInfo> coreAccountInfos) {
        AccountUtils.checkChildAccountStatus(
                mAccountManagerFacade, coreAccountInfos, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable CoreAccountInfo childInfo) {
        ++mNumOfChildAccountChecksDone;
        if (!isChild) {
            return;
        }

        assert childInfo != null;
        mSigninManager.runAfterOperationInProgress(
                () -> {
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
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                        mSigninManager.signin(
                                childInfo, SigninAccessPoint.FORCED_SIGNIN, signInCallback);
                    } else {
                        mSigninManager.wipeSyncUserData(
                                () -> {
                                    RecordUserAction.record(
                                            "Signin_Signin_WipeDataOnChildAccountSignin2");
                                    mSigninManager.signin(
                                            childInfo,
                                            SigninAccessPoint.FORCED_SIGNIN,
                                            signInCallback);
                                },
                                DataWipeOption.WIPE_SYNC_DATA);
                    }
                });
    }
}
