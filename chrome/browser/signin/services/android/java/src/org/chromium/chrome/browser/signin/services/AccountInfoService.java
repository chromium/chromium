// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.util.Pair;

import androidx.annotation.GuardedBy;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
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
public final class AccountInfoService implements IdentityManager.Observer {
    /**
     * Observes the changes of {@link AccountInfo}.
     */
    interface Observer {
        /**
         * Notifies when an {@link AccountInfo} is updated.
         */
        void onAccountInfoUpdated(AccountInfo accountInfo);
    }

    private static final String TAG = "AccountInfoService";
    private static final Object LOCK = new Object();

    @GuardedBy("LOCK")
    private static AccountInfoService sInstance;

    private final IdentityManager mIdentityManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private AccountInfoService(IdentityManager identityManager) {
        mIdentityManager = identityManager;
    }

    /**
     * Initializes the singleton object.
     */
    public static void init(IdentityManager identityManager) {
        synchronized (LOCK) {
            sInstance = new AccountInfoService(identityManager);
            identityManager.addObserver(sInstance);
        }
    }

    /**
     * Releases the resources used by {@link AccountInfoService}.
     */
    public void destroy() {
        mIdentityManager.removeObserver(this);
    }

    /**
     * Gets the singleton instance.
     */
    public static AccountInfoService get() {
        synchronized (LOCK) {
            if (sInstance == null) {
                throw new RuntimeException("The AccountInfoService is not yet initialized!");
            }
            return sInstance;
        }
    }

    @VisibleForTesting
    public static void resetForTests() {
        synchronized (LOCK) {
            sInstance = null;
        }
        PendingAccountInfoFetch.sPendingAccountInfoFetch = null;
    }

    /**
     * Gets the corresponding {@link AccountInfo} of the given account email.
     */
    AccountInfo getAccountInfoByEmail(String email) {
        return mIdentityManager.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                email);
    }

    /**
     * Adds an observer which will be invoked when an {@link AccountInfo} is updated.
     */
    void addObserver(Observer onAccountInfoUpdated) {
        mObservers.addObserver(onAccountInfoUpdated);
    }

    /**
     * Removes an observer which is invoked when an {@link AccountInfo} is updated.
     */
    void removeObserver(Observer onAccountInfoUpdated) {
        mObservers.removeObserver(onAccountInfoUpdated);
    }

    /**
     * Implements {@link IdentityManager.Observer}.
     */
    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        for (Observer observer : mObservers) {
            observer.onAccountInfoUpdated(accountInfo);
        }
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

        private final List<Pair<String, Observer>> mPendingRequests = new ArrayList<>();

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

        void pendFetch(String accountEmail, Observer observer) {
            mPendingRequests.add(Pair.create(accountEmail, observer));
        }

        @Override
        public void onSystemAccountsSeedingComplete() {
            for (Pair<String, Observer> request : mPendingRequests) {
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
     * @param observer Observer that takes the fetched {@link AccountInfo} as argument.
     */
    @MainThread
    public void startFetchingAccountInfoFor(String accountEmail, Observer observer) {
        ThreadUtils.assertOnUiThread();
        final Profile profile = Profile.getLastUsedRegularProfile();
        if (IdentityServicesProvider.get()
                        .getAccountTrackerService(profile)
                        .checkAndSeedSystemAccounts()) {
            fetchAccountInfo(accountEmail, observer);
        } else {
            PendingAccountInfoFetch.get().pendFetch(accountEmail, observer);
        }
    }

    @MainThread
    private static void fetchAccountInfo(String accountEmail, Observer observer) {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        final AccountInfo accountInfo =
                identityManager.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        accountEmail);
        if (accountInfo == null) {
            Log.i(TAG, "No AccountInfo available for email:" + accountEmail);
        } else if (accountInfo.getAccountImage() != null) {
            observer.onAccountInfoUpdated(accountInfo);
        } else {
            // Downloads the extended account information(full name, account image, etc) and saves
            // it on disk for the given Id.
            identityManager.forceRefreshOfExtendedAccountInfo(accountInfo.getId());
        }
    }
}
