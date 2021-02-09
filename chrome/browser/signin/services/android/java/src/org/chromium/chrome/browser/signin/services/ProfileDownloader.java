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
 * Android wrapper of the ProfileDownloader which provides access from the Java layer.
 * The native ProfileDownloader requires its access to be in the UI thread.
 * See chrome/browser/profiles/profile_downloader.h/cc for more details.
 */
class ProfileDownloader {
    private static final String TAG = "ProfileDownloader";
    private static final Object LOCK = new Object();

    @GuardedBy("LOCK")
    private static ProfileDownloader sInstance;

    private final ObserverList<ProfileDataSource.Observer> mObservers = new ObserverList<>();

    /**
     * Get the instance of ProfileDownloader.
     */
    static ProfileDownloader get() {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new ProfileDownloader();
            }
            return sInstance;
        }
    }

    @VisibleForTesting
    static void resetForTests() {
        synchronized (LOCK) {
            sInstance = null;
        }
        PendingProfileDownloads.sPendingProfileDownloads = null;
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
     * Private class (package private for tests) to pend profile download requests when system
     * accounts have not been seeded into AccountTrackerService.
     * It listens onSystemAccountsSeedingComplete to finish pending
     * requests and onSystemAccountsChanged to clear outdated pending requests.
     */
    @VisibleForTesting
    static class PendingProfileDownloads
            implements AccountTrackerService.OnSystemAccountsSeededListener {
        private static PendingProfileDownloads sPendingProfileDownloads;

        private final ArrayList<String> mAccountEmails;

        private PendingProfileDownloads() {
            mAccountEmails = new ArrayList<>();
        }

        static PendingProfileDownloads get() {
            ThreadUtils.assertOnUiThread();
            if (sPendingProfileDownloads == null) {
                sPendingProfileDownloads = new PendingProfileDownloads();
                IdentityServicesProvider.get()
                        .getAccountTrackerService(Profile.getLastUsedRegularProfile())
                        .addSystemAccountsSeededListener(sPendingProfileDownloads);
            }
            return sPendingProfileDownloads;
        }

        void pendProfileDownload(String accountEmail) {
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
            PendingProfileDownloads.get().pendProfileDownload(accountEmail);
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
            ProfileDownloader.get().notifyObservers(
                    new ProfileDataSource.ProfileData(accountEmail, accountInfo.getAccountImage(),
                            accountInfo.getFullName(), accountInfo.getGivenName()));
        } else {
            // Downloads the extended account information(full name, account image, etc) and saves
            // it on disk for the given Id.
            identityManager.forceRefreshOfExtendedAccountInfo(accountInfo.getId());
        }
    }
}
