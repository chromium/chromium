// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.GuardedBy;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.ArrayList;

/**
 * This class handles the {@link AccountInfo} fetch on Java side.
 */
final class AccountInfoService {
    private static final String TAG = "AccountInfoService";
    private static final Object LOCK = new Object();

    @GuardedBy("LOCK")
    private static AccountInfoService sInstance;

    private final ObserverList<ProfileDataSource.Observer> mObservers = new ObserverList<>();

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
     * Add an observer.
     * @param observer An observer.
     */
    public void addObserver(ProfileDataSource.Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer.
     * @param observer An observer.
     */
    public void removeObserver(ProfileDataSource.Observer observer) {
        mObservers.removeObserver(observer);
    }

    private void notifyObservers(ProfileDataSource.ProfileData profileData) {
        for (ProfileDataSource.Observer observer : mObservers) {
            observer.onProfileDataUpdated(profileData);
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

        private final ArrayList<String> mAccountEmails;

        private PendingAccountInfoFetch() {
            mAccountEmails = new ArrayList<>();
        }

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

        void pendFetch(String accountEmail) {
            mAccountEmails.add(accountEmail);
        }

        @Override
        public void onSystemAccountsSeedingComplete() {
            for (String accountEmail : mAccountEmails) {
                fetchAccountInfo(accountEmail);
            }
            mAccountEmails.clear();
        }

        @Override
        public void onSystemAccountsChanged() {
            mAccountEmails.clear();
        }
    }

    /**
     * Starts fetching the account information for a given account.
     * @param accountEmail Account email to fetch the information for
     */
    public void startFetchingAccountInfoFor(String accountEmail) {
        ThreadUtils.assertOnUiThread();
        final Profile profile = Profile.getLastUsedRegularProfile();
        if (IdentityServicesProvider.get()
                        .getAccountTrackerService(profile)
                        .checkAndSeedSystemAccounts()) {
            fetchAccountInfo(accountEmail);
        } else {
            PendingAccountInfoFetch.get().pendFetch(accountEmail);
        }
    }

    @MainThread
    private static void fetchAccountInfo(String accountEmail) {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        final AccountInfo accountInfo =
                identityManager.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        accountEmail);
        if (accountInfo == null) {
            Log.i(TAG, "No AccountInfo available for email:" + accountEmail);
        } else if (accountInfo.getAccountImage() != null) {
            AccountInfoService.get().notifyObservers(
                    new ProfileDataSource.ProfileData(accountEmail, accountInfo.getAccountImage(),
                            accountInfo.getFullName(), accountInfo.getGivenName()));
        } else {
            // Downloads the extended account information(full name, account image, etc) and saves
            // it on disk for the given Id.
            identityManager.forceRefreshOfExtendedAccountInfo(accountInfo.getId());
        }
    }
}
