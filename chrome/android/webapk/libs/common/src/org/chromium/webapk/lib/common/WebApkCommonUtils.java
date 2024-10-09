// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common;

/** Contains utility methods which are used both by ShellAPK and by Chrome. */
public class WebApkCommonUtils {
    /**
     * Returns name of "Runtime Dex" asset in Chrome APK based on version.
     *
     * @return Dex asset name.
     */
    public static String getRuntimeDexName(int version) {
        return "webapk" + version + ".dex";
    }

    /**
     * Returns authority of the content provider which provides the splash screen for the given
     * WebAPK.
     */
    public static String generateSplashContentProviderAuthority(String webApkPackageName) {
        return webApkPackageName + ".SplashContentProvider";
    }

    /**
     * Returns the URI of the content provider which provides the splash screen for the given
     * WebAPK.
     */
    public static String generateSplashContentProviderUri(String webApkPackageName) {
        return "content://"
                + generateSplashContentProviderAuthority(webApkPackageName)
                + "/cached_splash_image";
    }
}
