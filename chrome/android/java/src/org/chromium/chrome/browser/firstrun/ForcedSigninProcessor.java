// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.chrome.browser.preferences.sync.AccountManagementFragment;
import org.chromium.chrome.browser.services.AndroidEduAndChildAccountHelper;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChildAccountStatus;

/**
 * A helper to perform all necessary steps for forced sign in.
 * The helper performs:
 * - necessary Android EDU and child account checks;
 * - automatic non-interactive forced sign in for Android EDU and child accounts; and
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
     * This is triggered once per Chrome Application lifetime and everytime the Account state
     * changes with early exit if an account has already been signed in.
     */
    public static void start(@Nullable final Runnable onComplete) {
        new AndroidEduAndChildAccountHelper() {
            @Override
            public void onParametersReady() {
                boolean isAndroidEduDevice = isAndroidEduDevice();
                boolean hasChildAccount = ChildAccountStatus.isChild(getChildAccountStatus());
                // Child account and EDU device at the same time is not supported.
                assert !(isAndroidEduDevice && hasChildAccount);

                boolean forceSignin = isAndroidEduDevice || hasChildAccount;
                AccountManagementFragment.setSignOutAllowedPreferenceValue(!forceSignin);
                if (forceSignin) {
                    processForcedSignIn(onComplete);
                }
            }
        }.start();
    }

    /**
     * Processes the fully automatic non-FRE-related forced sign-in.
     * This is used to enforce the environment for Android EDU and child accounts.
     */
    private static void processForcedSignIn(@Nullable final Runnable onComplete) {
        final SigninManager signinManager = IdentityServicesProvider.getSigninManager();
        // By definition we have finished all the checks for first run.
        signinManager.onFirstRunCheckDone();
        if (!FirstRunUtils.canAllowSync() || !signinManager.isSignInAllowed()) {
            Log.d(TAG, "Sign in disallowed");
            return;
        }
        AccountManagerFacade.get().tryGetGoogleAccounts(accounts -> {
            if (accounts.size() != 1) {
                Log.d(TAG, "Incorrect number of accounts (%d)", accounts.size());
                return;
            }
            signinManager.signIn(accounts.get(0), new SigninManager.SignInCallback() {
                @Override
                public void onSignInComplete() {
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
    // consider removing the child account / EDU checks.
    public static void checkCanSignIn(final ChromeActivity activity) {
        if (IdentityServicesProvider.getSigninManager().isForceSigninEnabled()) {
            ExternalAuthUtils.getInstance().canUseGooglePlayServices(
                    new UserRecoverableErrorHandler.ModalDialog(activity, false));
        }
    }
}
