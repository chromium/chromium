// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.content.Context;
import android.graphics.Bitmap;

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
     * Private class to pend profile download requests when system accounts have not been seeded
     * into AccountTrackerService. It listens onSystemAccountsSeedingComplete to finish pending
     * requests and onSystemAccountsChanged to clear outdated pending requests.
     */
    private static class PendingProfileDownloads
            implements AccountTrackerService.OnSystemAccountsSeededListener {
        private static PendingProfileDownloads sPendingProfileDownloads;

        private final ArrayList<Profile> mProfiles;
        private final ArrayList<String> mAccountEmails;
        private final ArrayList<Integer> mImageSidePixels;

        private PendingProfileDownloads() {
            mProfiles = new ArrayList<>();
            mAccountEmails = new ArrayList<>();
            mImageSidePixels = new ArrayList<>();
        }

        static PendingProfileDownloads get(Context context) {
            ThreadUtils.assertOnUiThread();
            if (sPendingProfileDownloads == null) {
                sPendingProfileDownloads = new PendingProfileDownloads();
                IdentityServicesProvider.get()
                        .getAccountTrackerService(Profile.getLastUsedRegularProfile())
                        .addSystemAccountsSeededListener(sPendingProfileDownloads);
            }
            return sPendingProfileDownloads;
        }

        public void pendProfileDownload(Profile profile, String accountEmail, int imageSidePixels) {
            mProfiles.add(profile);
            mAccountEmails.add(accountEmail);
            mImageSidePixels.add(imageSidePixels);
        }

        @Override
        public void onSystemAccountsSeedingComplete() {
            int numberOfPendingRequests = mAccountEmails.size();
            while (numberOfPendingRequests > 0) {
                // Pending requests here must be pre-signin request since SigninManager will wait
                // system accounts been seeded into AccountTrackerService before finishing sign in.
                ProfileDownloaderJni.get().startFetchingAccountInfoFor(
                        mProfiles.get(0), mAccountEmails.get(0), mImageSidePixels.get(0), true);
                mProfiles.remove(0);
                mAccountEmails.remove(0);
                mImageSidePixels.remove(0);
                numberOfPendingRequests--;
            }
        }

        @Override
        public void onSystemAccountsChanged() {
            mProfiles.clear();
            mAccountEmails.clear();
            mImageSidePixels.clear();
        }
    }

    /**
     * Starts fetching the account information for a given account.
     * @param context context associated with the request
     * @param accountEmail Account email to fetch the information for
     * @param imageSidePixels Request image side (in pixels)
     */
    public void startFetchingAccountInfoFor(
            Context context, String accountEmail, int imageSidePixels, boolean isPreSignin) {
        ThreadUtils.assertOnUiThread();
        Profile profile = Profile.getLastUsedRegularProfile();
        if (!IdentityServicesProvider.get()
                        .getAccountTrackerService(profile)
                        .checkAndSeedSystemAccounts()) {
            PendingProfileDownloads.get(context).pendProfileDownload(
                    profile, accountEmail, imageSidePixels);
            return;
        }
        ProfileDownloaderJni.get().startFetchingAccountInfoFor(
                profile, accountEmail, imageSidePixels, isPreSignin);
    }

    @CalledByNative
    private static void onProfileDownloadSuccess(
            String accountEmail, String fullName, String givenName, Bitmap avatar) {
        ThreadUtils.assertOnUiThread();
        ProfileDownloader.get().notifyObservers(
                new ProfileDataSource.ProfileData(accountEmail, avatar, fullName, givenName));
    }

    @NativeMethods
    interface Natives {
        void startFetchingAccountInfoFor(
                Profile profile, String accountEmail, int imageSidePixels, boolean isPreSignin);
    }
}
