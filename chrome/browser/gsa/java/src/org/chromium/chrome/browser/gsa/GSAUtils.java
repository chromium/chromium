// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.content.pm.PackageInfo;
import android.text.TextUtils;

import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** This class provides utilities for the state of Google Search App. */
@NullMarked
public class GSAUtils {
    private static final Object sPackageInfoLock = new Object();
    @Nullable private static volatile PackageInfo sPackageInfo;

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
            @Nullable String installedVersionName, @Nullable String minimumVersionName) {
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
        PackageInfo info = getAgsaPackageInfo();
        return info == null ? null : info.versionName;
    }

    /**
     * Check if the AGSA is enabled
     *
     * @return Whether the AGSA is enabled
     */
    public static boolean isAgsaEnabled() {
        PackageInfo info = getAgsaPackageInfo();
        return info != null && info.applicationInfo != null && info.applicationInfo.enabled;
    }

    @Nullable
    private static PackageInfo getAgsaPackageInfo() {
        if (sPackageInfo == null) {
            synchronized (sPackageInfoLock) {
                if (sPackageInfo == null) {
                    sPackageInfo = PackageUtils.getPackageInfo(GSA_PACKAGE_NAME, 0);
                }
            }
        }
        return sPackageInfo;
    }

    /**
     * Sets the package info for AGSA for testing.
     *
     * @param packageInfo The package info to set for AGSA.
     */
    public static void setAgsaPackageInfoForTesting(@Nullable PackageInfo packageInfo) {
        sPackageInfo = packageInfo;
        ResettersForTesting.register(
                () -> {
                    sPackageInfo = null;
                });
    }
}
