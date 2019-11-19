// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;
import org.chromium.components.signin.ChromeSigninController;

import java.util.List;

/**
 * A class responsible fore representing the current state of Chrome's integration with GSA.
 */
public class GSAState {
    private static final int GSA_VERSION_FOR_DOCUMENT = 300401021;
    private static final int GMS_CORE_VERSION = 6577010;

    static final String SEARCH_INTENT_PACKAGE = "com.google.android.googlequicksearchbox";
    private static final String GMS_CORE_PACKAGE = "com.google.android.gms";

    static final String SEARCH_INTENT_ACTION =
            "com.google.android.googlequicksearchbox.TEXT_ASSIST";

    /**
     * An instance of GSAState class encapsulating knowledge about the current status.
     */
    @SuppressLint("StaticFieldLeak")
    private static GSAState sGSAState;

    /**
     * The application context to use.
     */
    private final Context mContext;

    /**
     * Caches the result of a computation on whether GSA is available.
     */
    private Boolean mGsaAvailable;

    /**
     * The Google account being used by GSA according to the latest update we have received.
     * This may be null.
     */
    private String mGsaAccount;

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
    private GSAState(Context context) {
        mContext = context.getApplicationContext();
    }

    /**
     * Update the GSA logged in account name and whether we are in GSA holdback.
     * @param gsaAccount The email address of the logged in account.
     */
    public void setGsaAccount(String gsaAccount) {
        mGsaAccount = gsaAccount;
    }

    /**
     * @return Whether GSA and Chrome are logged in with the same account. The situation where
     * both are logged out is not considered a match.
     */
    public boolean doesGsaAccountMatchChrome() {
        Account chromeUser = ChromeSigninController.get().getSignedInUser();
        return chromeUser != null && !TextUtils.isEmpty(mGsaAccount) && TextUtils.equals(
                chromeUser.name, mGsaAccount);
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
}
