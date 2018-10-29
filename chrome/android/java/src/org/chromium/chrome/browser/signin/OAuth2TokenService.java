// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.content.Context;
import android.support.annotation.MainThread;
import android.util.Log;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Java instance for the native OAuth2TokenService.
 * <p/>
 * This class forwards calls to request or invalidate access tokens made by native code to
 * AccountManagerFacade and forwards callbacks to native code.
 * <p/>
 */
public final class OAuth2TokenService
        implements AccountTrackerService.OnSystemAccountsSeededListener {
    private static final String TAG = "OAuth2TokenService";

    @VisibleForTesting
    public static final String STORED_ACCOUNTS_KEY = "google.services.stored_accounts";

    /**
     * Classes that want to listen for refresh token availability should
     * implement this interface and register with {@link #addObserver}.
     */
    public interface OAuth2TokenServiceObserver {
        void onRefreshTokenAvailable(Account account);
        void onRefreshTokenRevoked(Account account);
        void onRefreshTokensLoaded();
    }

    private static final String OAUTH2_SCOPE_PREFIX = "oauth2:";

    private boolean mPendingValidation;
    private boolean mPendingValidationForceNotifications;

    private final long mNativeOAuth2TokenServiceDelegateAndroid;
    private final ObserverList<OAuth2TokenServiceObserver> mObservers;

    private OAuth2TokenService(long nativeOAuth2Service) {
        mNativeOAuth2TokenServiceDelegateAndroid = nativeOAuth2Service;
        mObservers = new ObserverList<>();
        AccountTrackerService.get().addSystemAccountsSeededListener(this);
    }

    public static OAuth2TokenService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return (OAuth2TokenService) nativeGetForProfile(profile);
    }

    @CalledByNative
    private static OAuth2TokenService create(long nativeOAuth2Service) {
        ThreadUtils.assertOnUiThread();
        return new OAuth2TokenService(nativeOAuth2Service);
    }

    @VisibleForTesting
    public void addObserver(OAuth2TokenServiceObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.addObserver(observer);
    }

    @VisibleForTesting
    public void removeObserver(OAuth2TokenServiceObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.removeObserver(observer);
    }

    private static Account getAccountOrNullFromUsername(String username) {
        if (username == null) {
            Log.e(TAG, "Username is null");
            return null;
        }

        AccountManagerFacade accountManagerFacade = AccountManagerFacade.get();
        Account account = accountManagerFacade.getAccountFromName(username);
        if (account == null) {
            Log.e(TAG, "Account not found for provided username.");
            return null;
        }
        return account;
    }

    /**
     * Called by native to list the active account names in the OS.
     */
    @VisibleForTesting
    @CalledByNative
    public static String[] getSystemAccountNames() {
        // TODO(https://crbug.com/768366): Remove this after adding cache to account manager facade.
        // This function is called by native code on UI thread.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            List<String> accountNames = AccountManagerFacade.get().tryGetGoogleAccountNames();
            return accountNames.toArray(new String[accountNames.size()]);
        }
    }

    /**
     * Called by native to list the accounts Id with OAuth2 refresh tokens.
     * This can differ from getSystemAccountNames as the user add/remove accounts
     * from the OS. validateAccounts should be called to keep these two
     * in sync.
     */
    @CalledByNative
    public static String[] getAccounts() {
        return getStoredAccounts();
    }

    /**
     * Called by native to retrieve OAuth2 tokens.
     *  @param username The native username (full address).
     * @param scope The scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param nativeCallback The pointer to the native callback that should be run upon completion.
     */
    @MainThread
    @CalledByNative
    public static void getOAuth2AuthToken(
            String username, String scope, final long nativeCallback) {
        Account account = getAccountOrNullFromUsername(username);
        if (account == null) {
            ThreadUtils.postOnUiThread(() -> nativeOAuth2TokenFetched(null, false, nativeCallback));
            return;
        }
        String oauth2Scope = OAUTH2_SCOPE_PREFIX + scope;

        AccountManagerFacade accountManagerFacade = AccountManagerFacade.get();
        accountManagerFacade.getAuthToken(
                account, oauth2Scope, new AccountManagerFacade.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String token) {
                        nativeOAuth2TokenFetched(token, false, nativeCallback);
                    }

                    @Override
                    public void tokenUnavailable(boolean isTransientError) {
                        nativeOAuth2TokenFetched(null, isTransientError, nativeCallback);
                    }
                });
    }

    /**
     * Call this method to retrieve an OAuth2 access token for the given account and scope.
     *
     * @param account the account to get the access token for.
     * @param scope The scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param callback called on successful and unsuccessful fetching of auth token.
     */
    @MainThread
    public static void getOAuth2AccessToken(Context context, Account account, String scope,
            AccountManagerFacade.GetAuthTokenCallback callback) {
        String oauth2Scope = OAUTH2_SCOPE_PREFIX + scope;
        AccountManagerFacade.get().getAuthToken(account, oauth2Scope, callback);
    }

    /**
     * Call this method to retrieve an OAuth2 access token for the given account and scope. This
     * method times out after the specified timeout, and will return null if that happens.
     *
     * Given that this is a blocking method call, this should never be called from the UI thread.
     *
     * @param account the account to get the access token for.
     * @param scope The scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param timeout the timeout.
     * @param unit the unit for |timeout|.
     */
    @VisibleForTesting
    public static String getOAuth2AccessTokenWithTimeout(
            Context context, Account account, String scope, long timeout, TimeUnit unit) {
        assert !ThreadUtils.runningOnUiThread();
        final AtomicReference<String> result = new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        getOAuth2AccessToken(
                context, account, scope, new AccountManagerFacade.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String token) {
                        result.set(token);
                        semaphore.release();
                    }

                    @Override
                    public void tokenUnavailable(boolean isTransientError) {
                        result.set(null);
                        semaphore.release();
                    }
                });
        try {
            if (semaphore.tryAcquire(timeout, unit)) {
                return result.get();
            } else {
                Log.d(TAG, "Failed to retrieve auth token within timeout ("
                        + timeout + " + " + unit.name() + ")");
                return null;
            }
        } catch (InterruptedException e) {
            Log.w(TAG, "Got interrupted while waiting for auth token");
            return null;
        }
    }

    /**
     * Called by native to check whether the account has an OAuth2 refresh token.
     */
    @CalledByNative
    public static boolean hasOAuth2RefreshToken(String accountName) {
        if (!AccountManagerFacade.get().isCachePopulated()) {
            return false;
        }

        // Temporarily allowing disk read while fixing. TODO: http://crbug.com/618096.
        // This function is called in RefreshTokenIsAvailable of OAuth2TokenService which is
        // expected to be called in the UI thread synchronously.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            return AccountManagerFacade.get().hasAccountForName(accountName);
        }
    }

    /**
    * Called by native to invalidate an OAuth2 token.
    */
    @CalledByNative
    public static void invalidateOAuth2AuthToken(String accessToken) {
        if (accessToken != null) {
            AccountManagerFacade.get().invalidateAuthToken(accessToken);
        }
    }

    /**
    * Continue pending accounts validation after system accounts have been seeded into
    * AccountTrackerService.
    */
    @Override
    public void onSystemAccountsSeedingComplete() {
        if (mPendingValidation) {
            validateAccountsWithSignedInAccountName(mPendingValidationForceNotifications);
            mPendingValidation = false;
            mPendingValidationForceNotifications = false;
        }
    }

    /**
    * Clear pending accounts validation when system accounts in AccountTrackerService were
    * refreshed.
    */
    @Override
    public void onSystemAccountsChanged() {
        mPendingValidationForceNotifications = false;
    }

    @CalledByNative
    public void validateAccounts(boolean forceNotifications) {
        ThreadUtils.assertOnUiThread();
        if (!AccountTrackerService.get().checkAndSeedSystemAccounts()) {
            mPendingValidation = true;
            mPendingValidationForceNotifications = forceNotifications;
            return;
        }

        validateAccountsWithSignedInAccountName(forceNotifications);
    }

    private void validateAccountsWithSignedInAccountName(boolean forceNotifications) {
        String currentlySignedInAccount = ChromeSigninController.get().getSignedInAccountName();
        if (currentlySignedInAccount != null
                && isSignedInAccountChanged(currentlySignedInAccount)) {
            // Set currentlySignedInAccount to null for validation if signed-in account was changed
            // (renamed or removed from the device), this will cause all credentials in token
            // service be revoked.
            // Could only get here during Chrome cold startup.
            // After chrome started, SigninHelper and AccountsChangedReceiver will handle account
            // change (re-signin or sign out signed-in account).
            currentlySignedInAccount = null;
        }
        nativeValidateAccounts(mNativeOAuth2TokenServiceDelegateAndroid, currentlySignedInAccount,
                forceNotifications);
    }

    private boolean isSignedInAccountChanged(String signedInAccountName) {
        String[] accountNames = getSystemAccountNames();
        for (String accountName : accountNames) {
            if (accountName.equals(signedInAccountName)) return false;
        }
        return true;
    }

    @CalledByNative
    private void notifyRefreshTokenAvailable(String accountName) {
        assert accountName != null;
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        for (OAuth2TokenServiceObserver observer : mObservers) {
            observer.onRefreshTokenAvailable(account);
        }
    }

    @CalledByNative
    public void notifyRefreshTokenRevoked(String accountName) {
        assert accountName != null;
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        for (OAuth2TokenServiceObserver observer : mObservers) {
            observer.onRefreshTokenRevoked(account);
        }
    }

    @CalledByNative
    public void notifyRefreshTokensLoaded() {
        for (OAuth2TokenServiceObserver observer : mObservers) {
            observer.onRefreshTokensLoaded();
        }
    }

    private static String[] getStoredAccounts() {
        Set<String> accounts =
                ContextUtils.getAppSharedPreferences().getStringSet(STORED_ACCOUNTS_KEY, null);
        return accounts == null ? new String[]{} : accounts.toArray(new String[accounts.size()]);
    }

    @CalledByNative
    private static void saveStoredAccounts(String[] accounts) {
        Set<String> set = new HashSet<>(Arrays.asList(accounts));
        ContextUtils.getAppSharedPreferences().edit()
                .putStringSet(STORED_ACCOUNTS_KEY, set).apply();
    }

    private static native Object nativeGetForProfile(Profile profile);
    private static native void nativeOAuth2TokenFetched(
            String authToken, boolean isTransientError, long nativeCallback);
    private native void nativeValidateAccounts(long nativeOAuth2TokenServiceDelegateAndroid,
            String currentlySignedInAccount, boolean forceNotifications);
}
