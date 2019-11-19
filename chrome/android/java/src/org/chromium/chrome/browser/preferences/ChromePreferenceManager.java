// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.chrome.browser.crash.MinidumpUploadService.ProcessType;

import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * ChromePreferenceManager stores and retrieves values in Android shared preferences for specific
 * features.
 *
 * TODO(crbug.com/1022102): Finish moving feature-specific methods out of this class and delete it.
 */
public class ChromePreferenceManager {
    // For new int values with a default of 0, just document the key and its usage, and call
    // #readInt and #writeInt directly.
    // For new boolean values, document the key and its usage, call #readBoolean and #writeBoolean
    // directly. While calling #readBoolean, default value is required.

    private static class LazyHolder {
        static final ChromePreferenceManager INSTANCE = new ChromePreferenceManager();
    }

    private final SharedPreferencesManager mManager;

    private ChromePreferenceManager() {
        mManager = SharedPreferencesManager.getInstance();
    }

    /**
     * Get the static instance of ChromePreferenceManager if exists else create it.
     * @return the ChromePreferenceManager singleton
     */
    public static ChromePreferenceManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * @return Number of times of successful crash upload.
     */
    public int getCrashSuccessUploadCount(@ProcessType String process) {
        // Convention to keep all the key in preference lower case.
        return mManager.readInt(successUploadKey(process));
    }

    public void setCrashSuccessUploadCount(@ProcessType String process, int count) {
        // Convention to keep all the key in preference lower case.
        mManager.writeInt(successUploadKey(process), count);
    }

    public void incrementCrashSuccessUploadCount(@ProcessType String process) {
        setCrashSuccessUploadCount(process, getCrashSuccessUploadCount(process) + 1);
    }

    private String successUploadKey(@ProcessType String process) {
        return process.toLowerCase(Locale.US) + ChromePreferenceKeys.SUCCESS_UPLOAD_SUFFIX;
    }

    /**
     * @return Number of times of failure crash upload after reaching the max number of tries.
     */
    public int getCrashFailureUploadCount(@ProcessType String process) {
        return mManager.readInt(failureUploadKey(process));
    }

    public void setCrashFailureUploadCount(@ProcessType String process, int count) {
        mManager.writeInt(failureUploadKey(process), count);
    }

    public void incrementCrashFailureUploadCount(@ProcessType String process) {
        setCrashFailureUploadCount(process, getCrashFailureUploadCount(process) + 1);
    }

    private String failureUploadKey(@ProcessType String process) {
        return process.toLowerCase(Locale.US) + ChromePreferenceKeys.FAILURE_UPLOAD_SUFFIX;
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    public int getSigninPromoLastShownVersion() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION);
    }

    /**
     * Sets Chrome major version number when signin promo was last shown.
     */
    public void setSigninPromoLastShownVersion(int majorVersion) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion);
    }

    /**
     * Returns a set of account names on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    public Set<String> getSigninPromoLastAccountNames() {
        return mManager.readStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /**
     * Stores a set of account names on the device when signin promo is shown.
     */
    public void setSigninPromoLastAccountNames(Set<String> accountNames) {
        mManager.writeStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountNames);
    }

    /**
     * Returns timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed; zero otherwise.
     * @return the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    public long getNewTabPageSigninPromoSuppressionPeriodStart() {
        return mManager.readLong(ChromePreferenceKeys.NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Sets timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed.
     * @param timeMillis the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    public void setNewTabPageSigninPromoSuppressionPeriodStart(long timeMillis) {
        mManager.writeLong(
                ChromePreferenceKeys.NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START, timeMillis);
    }

    /**
     * Removes the stored timestamp of the suppression period start when signin promos in the New
     * Tab Page are no longer suppressed.
     */
    public void clearNewTabPageSigninPromoSuppressionPeriodStart() {
        mManager.removeKey(ChromePreferenceKeys.NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Gets a set of Strings representing digital asset links that have been verified.
     * Set by {@link #setVerifiedDigitalAssetLinks(Set)}.
     */
    public Set<String> getVerifiedDigitalAssetLinks() {
        // From the official docs, modifying the result of a SharedPreferences.getStringSet can
        // cause bad things to happen including exceptions or ruining the data.
        return new HashSet<>(
                mManager.readStringSet(ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS));
    }

    /**
     * Sets a set of digital asset links (represented a strings) that have been verified.
     * Can be retrieved by {@link #getVerifiedDigitalAssetLinks()}.
     */
    public void setVerifiedDigitalAssetLinks(Set<String> links) {
        mManager.writeStringSet(ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS, links);
    }

    /** Do not modify the set returned by this method. */
    private Set<String> getTrustedWebActivityDisclosureAcceptedPackages() {
        return mManager.readStringSet(
                ChromePreferenceKeys.TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES);
    }

    /**
     * Sets that the user has accepted the Trusted Web Activity "Running in Chrome" disclosure for
     * TWAs launched by the given package.
     */
    public void setUserAcceptedTwaDisclosureForPackage(String packageName) {
        mManager.addToStringSet(
                ChromePreferenceKeys.TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES,
                packageName);
    }

    /**
     * Removes the record of accepting the Trusted Web Activity "Running in Chrome" disclosure for
     * TWAs launched by the given package.
     */
    public void removeTwaDisclosureAcceptanceForPackage(String packageName) {
        mManager.removeFromStringSet(
                ChromePreferenceKeys.TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES,
                packageName);
    }

    /**
     * Checks whether the given package was previously passed to
     * {@link #setUserAcceptedTwaDisclosureForPackage(String)}.
     */
    public boolean hasUserAcceptedTwaDisclosureForPackage(String packageName) {
        return getTrustedWebActivityDisclosureAcceptedPackages().contains(packageName);
    }
}
