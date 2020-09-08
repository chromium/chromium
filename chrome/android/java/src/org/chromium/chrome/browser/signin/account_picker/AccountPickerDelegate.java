// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninUtils;
import org.chromium.chrome.browser.signin.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class is used in web sign-in flow for the account picker bottom sheet.
 *
 * It is responsible for the sign-in and adding account functions needed for the
 * web sign-in flow.
 */
public class AccountPickerDelegate implements WebSigninBridge.Listener {
    private final WindowAndroid mWindowAndroid;
    private final Tab mCurrentTab;
    private final TabCreator mIncognitoTabCreator;
    private final WebSigninBridge.Factory mWebSigninBridgeFactory;
    private final String mContinueUrl;
    private final SigninManager mSigninManager;
    private @Nullable WebSigninBridge mWebSigninBridge;
    private Callback<GoogleServiceAuthError> mOnSignInErrorCallback;

    public AccountPickerDelegate(WindowAndroid windowAndroid, Tab currentTab,
            TabCreator incognitoTabCreator, WebSigninBridge.Factory webSigninBridgeFactory,
            String continueUrl) {
        mWindowAndroid = windowAndroid;
        mCurrentTab = currentTab;
        mIncognitoTabCreator = incognitoTabCreator;
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mContinueUrl = continueUrl;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
    }

    /**
     * Creates and return the {@link IncognitoInterstitialDelegate}.
     */
    public IncognitoInterstitialDelegate getIncognitoInterstitialDelegate() {
        IncognitoInterstitialDelegate incognitoInterstitialDelegate =
                new IncognitoInterstitialDelegate(mWindowAndroid.getActivity().get(),
                        mIncognitoTabCreator, HelpAndFeedback.getInstance(),
                        mCurrentTab.getUrlString());
        return incognitoInterstitialDelegate;
    }

    /**
     * Releases resources used by this class.
     */
    public void onDismiss() {
        destroyWebSigninBridge();
        mOnSignInErrorCallback = null;
    }

    /**
     * Signs the user into the given account.
     */
    public void signIn(CoreAccountInfo coreAccountInfo,
            Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        mOnSignInErrorCallback = onSignInErrorCallback;
        mWebSigninBridge = mWebSigninBridgeFactory.create(
                Profile.getLastUsedRegularProfile(), coreAccountInfo, this);
        mSigninManager.signin(
                coreAccountInfo, new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // After the sign-in is finished in Chrome, we still need to wait for
                        // WebSigninBridge to be called to redirect to the continue url.
                    }

                    @Override
                    public void onSignInAborted() {
                        AccountPickerDelegate.this.destroyWebSigninBridge();
                    }
                });
    }

    /**
     * Notifies when the user clicked the "add account" button.
     *
     * TODO(https//crbug.com/1117000): Change the callback argument to CoreAccountInfo
     */
    public void addAccount(Callback<String> callback) {
        AccountManagerFacadeProvider.getInstance().createAddAccountIntent(
                (@Nullable Intent intent) -> {
                    if (intent != null) {
                        WindowAndroid.IntentCallback intentCallback =
                                new WindowAndroid.IntentCallback() {
                                    @Override
                                    public void onIntentCompleted(
                                            WindowAndroid window, int resultCode, Intent data) {
                                        if (resultCode == Activity.RESULT_OK) {
                                            callback.onResult(data.getStringExtra(
                                                    AccountManager.KEY_ACCOUNT_NAME));
                                        }
                                    }
                                };
                        mWindowAndroid.showIntent(intent, intentCallback, null);
                    } else {
                        // AccountManagerFacade couldn't create intent, use SigninUtils to open
                        // settings instead.
                        SigninUtils.openSettingsForAllAccounts(mWindowAndroid.getContext().get());
                    }
                });
    }

    /**
     * Sign-in completed successfully and the primary account is available in the cookie jar.
     */
    @MainThread
    @Override
    public void onSigninSucceded() {
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
        destroyWebSigninBridge();
    }

    private void destroyWebSigninBridge() {
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }
    }
}
