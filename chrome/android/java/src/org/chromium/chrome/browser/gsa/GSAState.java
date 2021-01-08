// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.List;

/**
 * A class responsible for representing the current state of Chrome's integration with GSA.
 */
public class GSAState {
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

    /**
     * The application context to use.
     */
    private final Context mContext;
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
     * @param context The context to use.
     * @return The state object.
     */
    public static GSAState getInstance(Context context) {
        if (sGSAState == null) {
            sGSAState = new GSAState(context);
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
    GSAState(Context context) {
        mContext = context.getApplicationContext();
    }

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
        mGsaAvailable = false;
        Intent searchIntent = new Intent(SEARCH_INTENT_ACTION);
        searchIntent.setPackage(GSAState.SEARCH_INTENT_PACKAGE);
        List<ResolveInfo> resolveInfo = PackageManagerUtils.queryIntentActivities(searchIntent, 0);
        if (resolveInfo.size() == 0) {
            mGsaAvailable = false;
        } else if (!isPackageAboveVersion(SEARCH_INTENT_PACKAGE, GSA_VERSION_FOR_DOCUMENT)
                || !isPackageAboveVersion(GMS_CORE_PACKAGE, GMS_CORE_VERSION)) {
            mGsaAvailable = false;
        } else {
            mGsaAvailable = true;
        }
        return mGsaAvailable;
    }

    /**
     * Check whether the given package meets min requirements for using full document mode.
     * @param packageName The package name we are inquiring about.
     * @param minVersion The minimum version for the package to be.
     * @return Whether the package exists on the device and its version is higher than the minimum
     *         required version.
     */
    private boolean isPackageAboveVersion(String packageName, int minVersion) {
        return PackageUtils.getPackageVersion(mContext, packageName) >= minVersion;
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
        if (!intent.getPackage().equals(IntentHandler.PACKAGE_GSA)) return false;

        PackageManager packageManager = mContext.getPackageManager();
        try {
            ComponentName activity = intent.resolveActivity(packageManager);
            if (activity == null) return false;
            PackageInfo packageInfo = packageManager.getPackageInfo(activity.getPackageName(), 0);
            if (packageInfo == null) return false;
        } catch (NameNotFoundException e) {
            return false;
        }

        return true;
    }

    /**
     * Gets the version name of the Agsa package.
     *
     * @return The version name of the Agsa package or null if it can't be found.
     */
    public @Nullable String getAgsaVersionName() {
        try {
            PackageInfo packageInfo =
                    mContext.getPackageManager().getPackageInfo(IntentHandler.PACKAGE_GSA, 0);
            return packageInfo.versionName;
        } catch (NameNotFoundException e) {
            return null;
        }
    }

    /**
     * @return Whether the AGSA app installed on the device supports Assistant voice search. This
     *         reads from a content provider and shouldn't be called directly on the UI thread.
     */
    public boolean agsaSupportsAssistantVoiceSearch() {
        ThreadUtils.assertOnBackgroundThread();

        Cursor cursor = null;
        try {
            cursor = mContext.getContentResolver().query(
                    Uri.parse(ROTI_CHROME_ENABLED_PROVIDER), null, null, null, null);
            return parseAgsaAssistantCursorResult(cursor);
        } catch (Exception e) {
            Log.e(TAG, "Failed due to unexpected exception.", e);
            return false;
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    @VisibleForTesting
    boolean parseAgsaAssistantCursorResult(Cursor cursor) {
        if (cursor == null) {
            Log.e(TAG, "Failed due to cursor being null.");
            return false;
        }
        boolean isValidCursor = cursor.moveToFirst();
        if (!isValidCursor) {
            Log.e(TAG, "Failed due cursor being empty.");
            return false;
        }
        if (cursor.getType(0) != Cursor.FIELD_TYPE_STRING) {
            Log.e(TAG, "Failed due cursor having unexpected datatype (expected string).");
            return false;
        }

        return Boolean.parseBoolean(cursor.getString(0));
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
        sGSAState = gsaState;
    }
}
