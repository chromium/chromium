// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.PackageManager;

import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.components.version_info.Channel;

import java.util.Locale;

/**
 * A utility class for querying information about the current Chrome build.
 * Intentionally doesn't depend on native so that the data can be accessed before
 * libchrome.so is loaded.
 */
public class ChromeVersionInfo {
    /**
     * @return Whether this build is a local build.
     */
    public static boolean isLocalBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.DEFAULT;
    }

    /**
     * @return Whether this build is a canary build.
     */
    public static boolean isCanaryBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.CANARY;
    }

    /**
     * @return Whether this build is a dev build.
     */
    public static boolean isDevBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.DEV;
    }

    /**
     * @return Whether this build is a beta build.
     */
    public static boolean isBetaBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.BETA;
    }

    /**
     * @return Whether this build is a stable build.
     */
    public static boolean isStableBuild() {
        return ChromeVersionConstants.CHANNEL == Channel.STABLE;
    }

    /**
     * @return Whether this is an official (i.e. Google Chrome) build.
     */
    public static boolean isOfficialBuild() {
        return ChromeVersionConstants.IS_OFFICIAL_BUILD;
    }

    /**
     * @return The version number.
     */
    public static String getProductVersion() {
        return ChromeVersionConstants.PRODUCT_VERSION;
    }

    /**
     * @return The major version number.
     */
    public static int getProductMajorVersion() {
        return ChromeVersionConstants.PRODUCT_MAJOR_VERSION;
    }

    /**
     * @return The build number.
     */
    public static int getBuildVersion() {
        return ChromeVersionConstants.PRODUCT_BUILD_VERSION;
    }

    /**
     * Returns info about the Google Play services setup for Chrome and the device.
     *
     * Contains the version number of the SDK Chrome was built with and the one for the installed
     * Play Services app. It also contains whether First Party APIs are available.
     */
    @CalledByNative
    public static String getGmsInfo() {
        Context context = ContextUtils.getApplicationContext();

        final long sdkVersion = GoogleApiAvailability.GOOGLE_PLAY_SERVICES_VERSION_CODE;
        final long installedGmsVersion = getPlayServicesApkVersionNumber(context);

        final String accessType;
        if (ExternalAuthUtils.canUseFirstPartyGooglePlayServices()) {
            accessType = "1p";
        } else if (ExternalAuthUtils.canUseGooglePlayServices()) {
            accessType = "3p";
        } else {
            accessType = "none";
        }

        return String.format(Locale.US,
                "SDK=%s; Installed=%s; Access=%s", sdkVersion, installedGmsVersion, accessType);
    }

    private static long getPlayServicesApkVersionNumber(Context context) {
        try {
            return context.getPackageManager()
                    .getPackageInfo(GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE, 0)
                    .versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            return 0;
        }
    }
}
