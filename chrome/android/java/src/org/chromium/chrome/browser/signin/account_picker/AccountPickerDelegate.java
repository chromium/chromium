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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.SigninUtils;
import org.chromium.chrome.browser.signin.WebSigninBridge;
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
public class AccountPickerDelegate implements WebSigninBridge.Listener {
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
    public AccountPickerDelegate(WindowAndroid windowAndroid, Tab currentTab,
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
    public void onDismiss() {
        destroyWebSigninBridge();
        mOnSignInErrorCallback = null;
    }

    /**
     * Signs the user into the given account.
     */
    public void signIn(CoreAccountInfo coreAccountInfo,
            Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        if (mIdentityManager.getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED) != null) {
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
                        SigninUtils.openSettingsForAllAccounts(mActivity);
                    }
                });
    }

    /**
     * Updates credentials of the given account name.
     */
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

    /**
     * Records Signin.AccountConsistencyPromoAction histogram.
     */
    public static void recordAccountConsistencyPromoAction(
            @AccountConsistencyPromoAction int promoAction) {
        RecordHistogram.recordEnumeratedHistogram("Signin.AccountConsistencyPromoAction",
                promoAction, AccountConsistencyPromoAction.MAX);
    }

    /**
     * Records AccountPickerBottomSheet shown count histograms.
     */
    public static void recordAccountConsistencyPromoShownCount(String histogram) {
        RecordHistogram.recordExactLinearHistogram(histogram,
                SigninPreferencesManager.getInstance().getAccountPickerBottomSheetShownCount(),
                100);
    }

    private void destroyWebSigninBridge() {
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }
    }
}
