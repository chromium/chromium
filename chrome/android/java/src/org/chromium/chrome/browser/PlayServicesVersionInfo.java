// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import com.google.android.gms.common.GoogleApiAvailability;

import org.jni_zero.CalledByNative;

import org.chromium.base.PackageUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.util.Locale;

/** A utility class for querying information about Play Services Version. */
public class PlayServicesVersionInfo {
    /**
     * Returns info about the Google Play services setup for Chrome and the device.
     *
     * Contains the version number of the SDK Chrome was built with and the one for the installed
     * Play Services app. It also contains whether First Party APIs are available.
     */
    @CalledByNative
    public static String getGmsInfo() {
        final long sdkVersion = GoogleApiAvailability.GOOGLE_PLAY_SERVICES_VERSION_CODE;
        final long installedGmsVersion = getApkVersionNumber();

        final String accessType;
        ExternalAuthUtils externalAuthUtils = ExternalAuthUtils.getInstance();
        if (externalAuthUtils.canUseFirstPartyGooglePlayServices()) {
            accessType = "1p";
        } else if (externalAuthUtils.canUseGooglePlayServices()) {
            accessType = "3p";
        } else {
            accessType = "none";
        }

        return String.format(
                Locale.US,
                "SDK=%s; Installed=%s; Access=%s",
                sdkVersion,
                installedGmsVersion,
                accessType);
    }

    /**
     * @return The version code for the Google Play Services installed on the device or -1 if the
     *     package is not found.
     */
    public static int getApkVersionNumber() {
        int ret =
                PackageUtils.getPackageVersion(GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE);
        if (ret < 0) {
            ret = 0;
        }
        return ret;
    }
}
