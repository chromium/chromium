// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.content.pm.PackageInfo;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.PackageUtils;

/** This class provides utilities for the state of Google Search App. */
public class GSAUtils {
    public static final String GSA_PACKAGE_NAME = "com.google.android.googlequicksearchbox";
    public static final String GSA_CLASS_NAME =
            "com.google.android.apps.search.googleapp.activity.GoogleAppActivity";
    public static final String VOICE_SEARCH_INTENT_ACTION = "android.intent.action.VOICE_ASSIST";

    /**
     * @return Whether the given package name is the package name for Google Search App.
     */
    public static boolean isGsaPackageName(String packageName) {
        return GSA_PACKAGE_NAME.equals(packageName);
    }

    /**
     * Checks if the AGSA version is below a certain {@code String} version name.
     *
     * @param installedVersionName The AGSA version installed on this device,
     * @param minimumVersionName The minimum AGSA version allowed.
     * @return Whether the AGSA version on the device is below the given minimum
     */
    public static boolean isAgsaVersionBelowMinimum(
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
     * Gets the version name of the Agsa package.
     *
     * @return The version name of the Agsa package or null if it can't be found.
     */
    public static @Nullable String getAgsaVersionName() {
        PackageInfo packageInfo = PackageUtils.getPackageInfo(GSA_PACKAGE_NAME, 0);
        return packageInfo == null ? null : packageInfo.versionName;
    }
}
