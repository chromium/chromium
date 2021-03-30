// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.signin.ui.SigninUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class is used in web sign-in flow for the account picker bottom sheet.
 *
 * It is responsible for the sign-in and adding account functions needed for the
 * web sign-in flow.
 */
public class AccountPickerDelegateImpl implements WebSigninBridge.Listener, AccountPickerDelegate {
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final Tab mCurrentTab;
    private final WebSigninBridge.Factory mWebSigninBridgeFactory;
    private final String mContinueUrl;
    private final SigninManager mSigninManager;
    private final IdentityManager mIdentityManager;
    private @Nullable WebSigninBridge mWebSigninBridge;
    private Callback<GoogleServiceAuthError> mOnSignInErrorCallback;

    /**
     * @param windowAndroid The {@link WindowAndroid} instance of the activity.
     * @param currentTab The current tab where the account picker bottom sheet is displayed.
     * @param webSigninBridgeFactory A {@link WebSigninBridge.Factory} to create {@link
     *         WebSigninBridge} instances.
     * @param continueUrl The URL that the user would be redirected to after sign-in succeeds.
     */
    public AccountPickerDelegateImpl(WindowAndroid windowAndroid, Tab currentTab,
            WebSigninBridge.Factory webSigninBridgeFactory, String continueUrl) {
        mWindowAndroid = windowAndroid;
        mActivity = mWindowAndroid.getActivity().get();
        assert mActivity != null : "Activity should not be null!";
        mCurrentTab = currentTab;
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mContinueUrl = continueUrl;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
    }

    /**
     * Releases resources used by this class.
     */
    @Override
    public void onDismiss() {
        destroyWebSigninBridge();
        mOnSignInErrorCallback = null;
    }

    /**
     * Signs the user into the given account.
     */
    @Override
    public void signIn(CoreAccountInfo coreAccountInfo,
            Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        if (mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) != null) {
            // In case an error is fired because cookies are taking longer to generate than usual,
            // if user retries the sign-in from the error screen, we need to sign out the user
            // first before signing in again.
            destroyWebSigninBridge();
            // TODO(https://crbug.com/1133752): Revise sign-out reason
            mSigninManager.signOut(SignoutReason.ABORT_SIGNIN);
        }
        mOnSignInErrorCallback = onSignInErrorCallback;
        mWebSigninBridge = mWebSigninBridgeFactory.create(
                Profile.getLastUsedRegularProfile(), coreAccountInfo, this);
        mSigninManager.signin(coreAccountInfo, new SigninManager.SignInCallback() {
            @Override
            public void onSignInComplete() {
                // After the sign-in is finished in Chrome, we still need to wait for
                // WebSigninBridge to be called to redirect to the continue url.
            }

            @Override
            public void onSignInAborted() {
                AccountPickerDelegateImpl.this.destroyWebSigninBridge();
            }
        });
    }

    /**
     * Notifies when the user clicked the "add account" button.
     *
     * TODO(https//crbug.com/1117000): Change the callback argument to CoreAccountInfo
     */
    @Override
    public void addAccount(Callback<String> callback) {
        AccountManagerFacadeProvider.getInstance().createAddAccountIntent((@Nullable Intent intent)
                                                                                  -> {
            if (intent != null) {
                WindowAndroid.IntentCallback intentCallback = new WindowAndroid.IntentCallback() {
                    @Override
                    public void onIntentCompleted(
                            WindowAndroid window, int resultCode, Intent data) {
                        if (resultCode == Activity.RESULT_OK) {
                            callback.onResult(data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME));
                        }
                    }
                };
                mWindowAndroid.showIntent(intent, intentCallback, null);
            } else {
                // AccountManagerFacade couldn't create intent, use SigninUtils to open
                // settings instead.
                SigninUtils.openSettingsForAllAccounts(mActivity);
            }
        });
    }

    /**
     * Updates credentials of the given account name.
     */
    @Override
    public void updateCredentials(
            String accountName, Callback<Boolean> onUpdateCredentialsCallback) {
        AccountManagerFacadeProvider.getInstance().updateCredentials(
                AccountUtils.createAccountFromName(accountName), mActivity,
                onUpdateCredentialsCallback);
    }

    /**
     * Sign-in completed successfully and the primary account is available in the cookie jar.
     */
    @MainThread
    @Override
    public void onSigninSucceeded() {
        ThreadUtils.assertOnUiThread();
        mCurrentTab.loadUrl(new LoadUrlParams(mContinueUrl));
    }

    /**
     * Sign-in process failed.
     *
     * @param error Details about the error that occurred in the sign-in process.
     */
    @MainThread
    @Override
    public void onSigninFailed(GoogleServiceAuthError error) {
        ThreadUtils.assertOnUiThread();
        mOnSignInErrorCallback.onResult(error);
    }

    private void destroyWebSigninBridge() {
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }
    }
}
