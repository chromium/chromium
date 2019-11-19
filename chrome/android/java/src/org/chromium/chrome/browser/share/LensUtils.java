// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.components.signin.ChromeSigninController;

/**
 * This class provides utilities for intenting into Google Lens.
 */
public class LensUtils {
    private static final String LENS_CONTRACT_URI = "googleapp://lens";
    private static final String LENS_BITMAP_URI_KEY = "LensBitmapUriKey";
    private static final String ACCOUNT_NAME_URI_KEY = "AccountNameUriKey";
    private static final String INCOGNITO_URI_KEY = "IncognitoUriKey";
    private static final String MIN_AGSA_VERSION_FEATURE_PARAM_NAME = "minAgsaVersionName";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "8.19";

    /**
     * See function for details.
     */
    private static boolean sFakePassableLensEnvironmentForTesting;

    /*
     * If true, short-circuit the version name intent check to always return a high enough version.
     * Also hardcode the device OS check to return true.
     * Used by test cases.
     * @param shouldFake Whether to fake the version check.
     */
    public static void setFakePassableLensEnvironmentForTesting(boolean shouldFake) {
        sFakePassableLensEnvironmentForTesting = shouldFake;
    }

    /**
     * Resolve the activity to verify that lens is ready to accept an intent and also
     * retrieve the version name.
     *
     * @param context The relevant application context with access to the activity.
     * @return The version name string of the AGSA app or an empty string if not available.
     */
    public static String getLensActivityVersionNameIfAvailable(Context context) {
        // Use this syntax to avoid NPE if unset.
        if (Boolean.TRUE.equals(sFakePassableLensEnvironmentForTesting)) {
            // Returns the minimum version which will meet the bar and allow future AGSA version
            // checks to succeed.
            return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
        } else {
            try {
                PackageManager pm = context.getPackageManager();
                // No data transmission occurring so safe to assume incognito is false.
                Intent lensIntent =
                        getShareWithGoogleLensIntent(Uri.EMPTY, /* isIncognito= */ false);
                ComponentName lensActivity = lensIntent.resolveActivity(pm);
                if (lensActivity == null) return "";
                PackageInfo packageInfo = pm.getPackageInfo(lensActivity.getPackageName(), 0);
                if (packageInfo == null) {
                    return "";
                } else {
                    return packageInfo.versionName;
                }
            } catch (PackageManager.NameNotFoundException e) {
                return "";
            }
        }
    }

    /**
     * Gets the minimum AGSA version required to support the Lens context menu integration
     * on this device.  Takes the value from a server provided value if a field trial is
     * active but otherwise will take the value from a client side default (unless the
     * lens feature is not enabled at all, in which case return an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    private static String getMinimumAgsaVersionForLensSupport() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)) {
            final String serverProvidedMinAgsaVersion =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            MIN_AGSA_VERSION_FEATURE_PARAM_NAME);
            if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
                // Falls into this block if the user enabled the feature using chrome://flags and
                // the param was not set by the server.
                return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
            }
            return serverProvidedMinAgsaVersion;
        }
        // The feature is disabled so no need to return a minimum version.
        return "";
    }

    /**
     * Checks if the AGSA version is below a certain {@code String} version name
     * which denotes support for the Lens postcapture experience.
     * @param installedVersionName The AGSA version installed on this device,
     * @return Whether the AGSA version on the device is high enough.
     */
    public static boolean isAgsaVersionBelowMinimum(String installedVersionName) {
        String minimumAllowedAgsaVersionName = getMinimumAgsaVersionForLensSupport();
        if (TextUtils.isEmpty(installedVersionName)
                || TextUtils.isEmpty(minimumAllowedAgsaVersionName)) {
            return true;
        }

        String[] agsaNumbers = installedVersionName.split("\\.", -1);
        String[] targetAgsaNumbers = minimumAllowedAgsaVersionName.split("\\.", -1);

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
     * Checks whether the device is below Android O.  We restrict to these versions
     * to limit to OS"s where image processing vulnerabilities can be retroactively
     * fixed if they are discovered in the future.
     * @return Whether the device is below Android O.
     */
    public static boolean isDeviceOsBelowMinimum() {
        if (sFakePassableLensEnvironmentForTesting) {
            return false;
        }

        return Build.VERSION.SDK_INT < Build.VERSION_CODES.O;
    }

    /**
     * Get a deeplink intent to Google Lens with an optional content provider image URI.
     * @param imageUri The content provider URI generated by chrome (or empty URI)
     *                 if only resolving the activity.
     * @param isIncognito Whether the current tab is in incognito mode.
     * @return The intent to Google Lens.
     */
    public static Intent getShareWithGoogleLensIntent(Uri imageUri, boolean isIncognito) {
        String signedInAccountName = ChromeSigninController.get().getSignedInAccountName();
        // If incognito do not send the account name to avoid leaking session information to Lens.
        if (signedInAccountName == null || isIncognito) signedInAccountName = "";

        Uri lensUri = Uri.parse(LENS_CONTRACT_URI);
        if (!Uri.EMPTY.equals(imageUri)) {
            lensUri =
                    lensUri.buildUpon()
                            .appendQueryParameter(LENS_BITMAP_URI_KEY, imageUri.toString())
                            .appendQueryParameter(ACCOUNT_NAME_URI_KEY, signedInAccountName)
                            .appendQueryParameter(INCOGNITO_URI_KEY, Boolean.toString(isIncognito))
                            .build();
            ContextUtils.getApplicationContext().grantUriPermission(
                    IntentHandler.PACKAGE_GSA, imageUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        Intent intent = new Intent(Intent.ACTION_VIEW).setData(lensUri);
        intent.setPackage(IntentHandler.PACKAGE_GSA);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        return intent;
    }
}
