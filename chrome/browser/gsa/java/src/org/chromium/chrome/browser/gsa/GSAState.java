// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A class responsible for representing the current state of Chrome's integration with GSA.
 */
public class GSAState {
    public static final String PACKAGE_NAME = "com.google.android.googlequicksearchbox";

    /** Used to observe state changes in the class. */
    public interface Observer {
        /** Called when the GSA account name is set. */
        void onSetGsaAccount();
    }

    private static final String TAG = "GSAState";

    private static final int GSA_VERSION_FOR_DOCUMENT = 300401021;
    private static final int GMS_CORE_VERSION = 6577010;

    static final String SEARCH_INTENT_PACKAGE = "com.google.android.googlequicksearchbox";
    private static final String GMS_CORE_PACKAGE = "com.google.android.gms";

    static final String SEARCH_INTENT_ACTION =
            "com.google.android.googlequicksearchbox.TEXT_ASSIST";

    // AGSA's public content provider, used to expose public properties to other apps.
    static final String GSA_PUBLIC_CONTENT_PROVIDER =
            String.format("%s.GsaPublicContentProvider", SEARCH_INTENT_PACKAGE);
    // AGSA-side checks for if Chrome should use Assistant for voice transcription.
    // This value is a boolean stored as a string.
    static final String ROTI_CHROME_ENABLED_PROVIDER = String.format(
            "content://%s/publicvalue/roti_for_chrome_enabled", GSA_PUBLIC_CONTENT_PROVIDER);

    /**
     * An instance of GSAState class encapsulating knowledge about the current status.
     */
    @SuppressLint("StaticFieldLeak")
    private static GSAState sGSAState;

    private static final Pattern MAJOR_MINOR_VERSION_PATTERN =
            Pattern.compile("^(\\d+)\\.(\\d+)(\\.\\d+)*$");

    private final ObserverList<Observer> mObserverList = new ObserverList<>();

    /**
     * Caches the result of a computation on whether GSA is available.
     */
    private Boolean mGsaAvailable;

    /**
     * The Google account email address being used by GSA according to the latest update we have
     * received.
     */
    private @Nullable String mGsaAccount;

    /**
     * Returns the singleton instance of GSAState and creates one if necessary.
     */
    public static GSAState getInstance() {
        if (sGSAState == null) {
            sGSAState = new GSAState();
        }
        return sGSAState;
    }

    /**
     * @return Whether the given package name is the package name for Google Search App.
     */
    public static boolean isGsaPackageName(String packageName) {
        return SEARCH_INTENT_PACKAGE.equals(packageName);
    }

    /* Private constructor, since this is a singleton */
    @VisibleForTesting
    GSAState() {}

    /**
     * Update the GSA logged in account name and whether we are in GSA holdback.
     * @param gsaAccount The email address of the logged in account.
     */
    public void setGsaAccount(String gsaAccount) {
        mGsaAccount = gsaAccount;

        for (Observer observer : mObserverList) {
            observer.onSetGsaAccount();
        }
    }

    /**
     * @return Whether GSA and Chrome are logged in with the same account. The situation where
     * both are logged out is not considered a match.
     */
    public boolean doesGsaAccountMatchChrome() {
        if (!ProfileManager.isInitialized()) return false;
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        CoreAccountInfo chromeAccountInfo =
                identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC);
        return chromeAccountInfo != null && !TextUtils.isEmpty(mGsaAccount)
                && TextUtils.equals(chromeAccountInfo.getEmail(), mGsaAccount);
    }

    /**
     * @return The current GSA account. May return null if GSA hasn't replied yet.
     */
    public String getGsaAccount() {
        return mGsaAccount;
    }

    /**
     * This is used to check whether GSA package is available to handle search requests and if
     * the Chrome experiment to do so is enabled.
     * @return Whether the search intent this class creates will resolve to an activity.
     */
    public boolean isGsaAvailable() {
        if (mGsaAvailable != null) return mGsaAvailable;

        Intent searchIntent = new Intent(SEARCH_INTENT_ACTION);
        searchIntent.setPackage(GSAState.SEARCH_INTENT_PACKAGE);
        mGsaAvailable = PackageManagerUtils.canResolveActivity(searchIntent)
                && isPackageAboveVersion(SEARCH_INTENT_PACKAGE, GSA_VERSION_FOR_DOCUMENT)
                && isPackageAboveVersion(GMS_CORE_PACKAGE, GMS_CORE_VERSION);

        return mGsaAvailable;
    }

    /** Returns whether the GSA package is installed on device. */
    public boolean isGsaInstalled() {
        return PackageUtils.isPackageInstalled(PACKAGE_NAME);
    }

    /**
     * Check whether the given package meets min requirements for using full document mode.
     * @param packageName The package name we are inquiring about.
     * @param minVersion The minimum version for the package to be.
     * @return Whether the package exists on the device and its version is higher than the minimum
     *         required version.
     */
    private boolean isPackageAboveVersion(String packageName, int minVersion) {
        return PackageUtils.getPackageVersion(packageName) >= minVersion;
    }

    /**
     * Checks if the AGSA version is below a certain {@code String} version name.
     *
     * @param installedVersionName The AGSA version installed on this device,
     * @param minimumVersionName The minimum AGSA version allowed.
     * @return Whether the AGSA version on the device is below the given minimum
     */
    public boolean isAgsaVersionBelowMinimum(
            String installedVersionName, String minimumVersionName) {
        if (TextUtils.isEmpty(installedVersionName) || TextUtils.isEmpty(minimumVersionName)) {
            return true;
        }

        String[] agsaNumbers = installedVersionName.split("\\.", -1);
        String[] targetAgsaNumbers = minimumVersionName.split("\\.", -1);

        // To avoid IndexOutOfBounds
        int maxIndex = Math.min(agsaNumbers.length, targetAgsaNumbers.length);
        for (int i = 0; i < maxIndex; ++i) {
            int agsaNumber = Integer.parseInt(agsaNumbers[i]);
            int targetAgsaNumber = Integer.parseInt(targetAgsaNumbers[i]);

            if (agsaNumber < targetAgsaNumber) {
                return true;
            } else if (agsaNumber > targetAgsaNumber) {
                return false;
            }
        }

        // If versions are the same so far, but they have different length...
        return agsaNumbers.length < targetAgsaNumbers.length;
    }

    /**
     * Determines if the given intent can be handled by Agsa.
     *
     * @param intent Given to the system to find the Activity available to handle it.
     * @return Whether the given intent can be handled by Agsa.
     */
    public boolean canAgsaHandleIntent(@NonNull Intent intent) {
        if (!intent.getPackage().equals(PACKAGE_NAME)) return false;

        ComponentName activity =
                intent.resolveActivity(ContextUtils.getApplicationContext().getPackageManager());
        if (activity == null) return false;
        return PackageUtils.isPackageInstalled(activity.getPackageName());
    }

    /**
     * Gets the version name of the Agsa package.
     *
     * @return The version name of the Agsa package or null if it can't be found.
     */
    public @Nullable String getAgsaVersionName() {
        PackageInfo packageInfo = PackageUtils.getPackageInfo(PACKAGE_NAME, 0);
        return packageInfo == null ? null : packageInfo.versionName;
    }

    /**
     * Converts the given version name into a reportable integer which contains the major and minor
     * version.
     * - The returned integer ranges between 1,000 - 999,999.
     * - The major version is represented by the numbers in the hundred/ten/thousanths places.
     * - The minor version is represented by the numbers in the tens/hundredths places.
     * - The max for both major and minor versions is 999. If either exceeds the maximum, null is
     *   returned.
     *
     * @param versionName The version name as a string (eg 11.9).
     * @return The version name as an integer between 1,000 - 999,999 as described above or null if
     *         the above conditions aren't satisfied.
     */
    public @Nullable Integer parseAgsaMajorMinorVersionAsInteger(String versionName) {
        if (versionName == null) return null;

        Matcher matcher = MAJOR_MINOR_VERSION_PATTERN.matcher(versionName);
        if (!matcher.find() || matcher.groupCount() < 2) return null;

        try {
            int major = Integer.parseInt(matcher.group(1));
            if (major > 999) {
                Log.e(TAG, "Major verison exceeded maximum of 999.");
                return null;
            }

            int minor = Integer.parseInt(matcher.group(2));
            if (minor > 999) {
                Log.e(TAG, "Minor verison exceeded maximum of 999.");
                return null;
            }
            return major * 1000 + minor;
        } catch (NumberFormatException e) {
            Log.e(TAG, "Version was incorrectly formatted.");
            return null;
        }
    }

    /**
     * Adds an observer.
     * @param observer The observer to add.
     */
    public void addObserver(@NonNull Observer observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Removes an observer.
     * @param observer The observer to remove.
     */
    public void removeObserver(@NonNull Observer observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * Sets an instance for testing.
     * @param gsaState The instance to set for testing.
     */
    public static void setInstanceForTesting(GSAState gsaState) {
        var oldValue = sGSAState;
        sGSAState = gsaState;
        ResettersForTesting.register(() -> sGSAState = oldValue);
    }
}
