// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.util.Pair;

import androidx.annotation.GuardedBy;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.ArrayList;
import java.util.List;

/**
 * This class handles the {@link AccountInfo} fetch on Java side.
 */
final class AccountInfoService {
    private static final String TAG = "AccountInfoService";
    private static final Object LOCK = new Object();

    @GuardedBy("LOCK")
    private static AccountInfoService sInstance;

    /**
     * Get the instance of ProfileDownloader.
     */
    static AccountInfoService get() {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new AccountInfoService();
            }
            return sInstance;
        }
    }

    @VisibleForTesting
    static void resetForTests() {
        synchronized (LOCK) {
            sInstance = null;
        }
        PendingAccountInfoFetch.sPendingAccountInfoFetch = null;
    }

    /**
     * Private class (package private for tests) to pend account info fetch requests when system
     * accounts have not been seeded into AccountTrackerService.
     * It listens onSystemAccountsSeedingComplete to finish pending
     * requests and onSystemAccountsChanged to clear outdated pending requests.
     */
    @VisibleForTesting
    static class PendingAccountInfoFetch
            implements AccountTrackerService.OnSystemAccountsSeededListener {
        private static PendingAccountInfoFetch sPendingAccountInfoFetch;

        private final List<Pair<String, Callback<AccountInfo>>> mPendingRequests =
                new ArrayList<>();

        private PendingAccountInfoFetch() {}

        static PendingAccountInfoFetch get() {
            ThreadUtils.assertOnUiThread();
            if (sPendingAccountInfoFetch == null) {
                sPendingAccountInfoFetch = new PendingAccountInfoFetch();
                IdentityServicesProvider.get()
                        .getAccountTrackerService(Profile.getLastUsedRegularProfile())
                        .addSystemAccountsSeededListener(sPendingAccountInfoFetch);
            }
            return sPendingAccountInfoFetch;
        }

        void pendFetch(String accountEmail, Callback<AccountInfo> callback) {
            mPendingRequests.add(Pair.create(accountEmail, callback));
        }

        @Override
        public void onSystemAccountsSeedingComplete() {
            for (Pair<String, Callback<AccountInfo>> request : mPendingRequests) {
                fetchAccountInfo(request.first, request.second);
            }
            mPendingRequests.clear();
        }

        @Override
        public void onSystemAccountsChanged() {
            mPendingRequests.clear();
        }
    }

    /**
     * Starts fetching the account information for a given account.
     * @param accountEmail Account email to fetch the information for
     * @param callback Callback that takes the fetched {@link AccountInfo} as argument.
     */
    @MainThread
    public void startFetchingAccountInfoFor(String accountEmail, Callback<AccountInfo> callback) {
        ThreadUtils.assertOnUiThread();
        final Profile profile = Profile.getLastUsedRegularProfile();
        if (IdentityServicesProvider.get()
                        .getAccountTrackerService(profile)
                        .checkAndSeedSystemAccounts()) {
            fetchAccountInfo(accountEmail, callback);
        } else {
            PendingAccountInfoFetch.get().pendFetch(accountEmail, callback);
        }
    }

    @MainThread
    private static void fetchAccountInfo(String accountEmail, Callback<AccountInfo> callback) {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        final AccountInfo accountInfo =
                identityManager.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        accountEmail);
        if (accountInfo == null) {
            Log.i(TAG, "No AccountInfo available for email:" + accountEmail);
        } else if (accountInfo.getAccountImage() != null) {
            callback.onResult(accountInfo);
        } else {
            // Downloads the extended account information(full name, account image, etc) and saves
            // it on disk for the given Id.
            identityManager.forceRefreshOfExtendedAccountInfo(accountInfo.getId());
        }
    }
}
