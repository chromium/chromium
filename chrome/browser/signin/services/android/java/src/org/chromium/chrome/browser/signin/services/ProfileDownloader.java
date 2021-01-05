// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.graphics.Bitmap;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.ProfileDataSource;

import java.util.ArrayList;

/**
 * Android wrapper of the ProfileDownloader which provides access from the Java layer.
 * The native ProfileDownloader requires its access to be in the UI thread.
 * See chrome/browser/profiles/profile_downloader.h/cc for more details.
 */
class ProfileDownloader {
    private static final Object LOCK = new Object();

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
        }
        return sInstance;
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

        private final ArrayList<Profile> mProfiles;
        private final ArrayList<String> mAccountEmails;
        private final ArrayList<Integer> mImageSizes;

        private PendingProfileDownloads() {
            mProfiles = new ArrayList<>();
            mAccountEmails = new ArrayList<>();
            mImageSizes = new ArrayList<>();
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

        void pendProfileDownload(Profile profile, String accountEmail, int imageSize) {
            mProfiles.add(profile);
            mAccountEmails.add(accountEmail);
            mImageSizes.add(imageSize);
        }

        @Override
        public void onSystemAccountsSeedingComplete() {
            int numberOfPendingRequests = mAccountEmails.size();
            while (numberOfPendingRequests > 0) {
                // Pending requests here must be pre-signin request since SigninManager will wait
                // system accounts been seeded into AccountTrackerService before finishing sign in.
                ProfileDownloaderJni.get().startFetchingAccountInfoFor(
                        mProfiles.get(0), mAccountEmails.get(0), mImageSizes.get(0), true);
                mProfiles.remove(0);
                mAccountEmails.remove(0);
                mImageSizes.remove(0);
                numberOfPendingRequests--;
            }
        }

        @Override
        public void onSystemAccountsChanged() {
            mProfiles.clear();
            mAccountEmails.clear();
            mImageSizes.clear();
        }
    }

    /**
     * Starts fetching the account information for a given account.
     * @param accountEmail Account email to fetch the information for
     * @param imageSize Request image side size (in pixels)
     */
    public void startFetchingAccountInfoFor(String accountEmail, @Px int imageSize) {
        ThreadUtils.assertOnUiThread();
        final Profile profile = Profile.getLastUsedRegularProfile();
        if (!IdentityServicesProvider.get()
                        .getAccountTrackerService(profile)
                        .checkAndSeedSystemAccounts()) {
            PendingProfileDownloads.get().pendProfileDownload(profile, accountEmail, imageSize);
            return;
        }
        ProfileDownloaderJni.get().startFetchingAccountInfoFor(
                profile, accountEmail, imageSize, /* isPreSignin= */ true);
    }

    @VisibleForTesting
    @CalledByNative
    static void onProfileDownloadSuccess(
            String accountEmail, String fullName, String givenName, Bitmap avatar) {
        ThreadUtils.assertOnUiThread();
        ProfileDownloader.get().notifyObservers(
                new ProfileDataSource.ProfileData(accountEmail, avatar, fullName, givenName));
    }

    @NativeMethods
    interface Natives {
        void startFetchingAccountInfoFor(
                Profile profile, String accountEmail, int imageSize, boolean isPreSignin);
    }
}
