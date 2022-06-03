// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.PackageManager;

import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.util.Locale;

/**
 * A utility class for querying information about Play Services Version.
 */
public class PlayServicesVersionInfo {
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
        final long installedGmsVersion = getApkVersionNumber(context);

        final String accessType;
        ExternalAuthUtils externalAuthUtils = ExternalAuthUtils.getInstance();
        if (externalAuthUtils.canUseFirstPartyGooglePlayServices()) {
            accessType = "1p";
        } else if (externalAuthUtils.canUseGooglePlayServices()) {
            accessType = "3p";
        } else {
            accessType = "none";
        }

        return String.format(Locale.US, "SDK=%s; Installed=%s; Access=%s", sdkVersion,
                installedGmsVersion, accessType);
    }

    /**
     *
     * @param context A Context with which to retrieve the PackageManager.
     * @return The version code for the Google Play Services installed on the device or 0 if the
     *         package is not found.
     */
    public static int getApkVersionNumber(Context context) {
        try {
            return context.getPackageManager()
                    .getPackageInfo(GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE, 0)
                    .versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            return 0;
        }
    }
}
