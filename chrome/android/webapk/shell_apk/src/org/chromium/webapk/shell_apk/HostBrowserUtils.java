// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Contains methods for getting information about host browser. */
public class HostBrowserUtils {
    private static final String VERSION_NAME_DEVELOPER_BUILD = "Developer Build";

    public static String ARC_INTENT_HELPER_BROWSER = "org.chromium.arc.intent_helper";

    public static String ARC_WEBAPK_BROWSER = "org.chromium.arc.webapk";

    /** The package names of the browsers that support WebAPK notification delegation. */
    private static Set<String> sBrowsersSupportingNotificationDelegation =
            new HashSet<String>(
                    Arrays.asList(
                            "com.google.android.apps.chrome",
                            "com.android.chrome",
                            "com.chrome.beta",
                            "com.chrome.dev",
                            "com.chrome.canary",
                            "org.chromium.chrome",
                            "org.chromium.chrome.tests",
                            ARC_INTENT_HELPER_BROWSER,
                            ARC_WEBAPK_BROWSER));

    /**
     * Returns whether the passed-in browser package name supports WebAPK notification delegation.
     */
    public static boolean doesBrowserSupportNotificationDelegation(String browserPackageName) {
        return sBrowsersSupportingNotificationDelegation.contains(browserPackageName);
    }

    /**
     * Returns whether intents (android.intent.action.MAIN, android.intent.action.SEND ...) should
     * launch {@link SplashActivity} for the given host browser params.
     */
    public static boolean shouldIntentLaunchSplashActivity(HostBrowserLauncherParams params) {
        return params.isNewStyleWebApk()
                && !params.getHostBrowserPackageName().equals(ARC_INTENT_HELPER_BROWSER)
                && !params.getHostBrowserPackageName().equals(ARC_WEBAPK_BROWSER);
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK, or null if we did not find
     * one.
     */
    public static String computeHostBrowserPackageName(Context context) {
        // Gets the package name of the host browser if it is specified in AndroidManifest.xml.
        String hostBrowserFromManifest = getHostBrowserFromManifest(context);
        if (!TextUtils.isEmpty(hostBrowserFromManifest)) {
            return hostBrowserFromManifest;
        }

        PackageManager packageManager = context.getPackageManager();

        // Gets the package name of the default browser on the Android device.
        // TODO(hanxi): Investigate the best way to know which browser supports WebAPKs.
        String defaultBrowser = getDefaultBrowserPackageName(packageManager);
        if (!TextUtils.isEmpty(defaultBrowser)
                && WebApkUtils.isInstalled(packageManager, defaultBrowser)) {
            return defaultBrowser;
        }

        return null;
    }

    private static String getHostBrowserFromManifest(Context context) {
        String hostBrowserFromManifest =
                WebApkUtils.readMetaDataFromManifest(context, WebApkMetaDataKeys.RUNTIME_HOST);
        if (!TextUtils.isEmpty(hostBrowserFromManifest)
                && WebApkUtils.isInstalled(context.getPackageManager(), hostBrowserFromManifest)) {
            return hostBrowserFromManifest;
        }
        return null;
    }

    static boolean isHostBrowserFromManifest(Context context, String hostBrowser) {
        if (TextUtils.isEmpty(hostBrowser)) {
            return false;
        }
        return TextUtils.equals(hostBrowser, getHostBrowserFromManifest(context));
    }

    /** Returns the package name of the default browser on the Android device. */
    private static String getDefaultBrowserPackageName(PackageManager packageManager) {
        Intent browserIntent = WebApkUtils.getQueryInstalledBrowsersIntent();
        ResolveInfo resolveInfo =
                packageManager.resolveActivity(browserIntent, PackageManager.MATCH_DEFAULT_ONLY);
        return WebApkUtils.getPackageNameFromResolveInfo(resolveInfo);
    }

    /** Deletes the internal storage for the given context. */
    private static void deleteInternalStorage(Context context) {
        DexLoader.deletePath(context.getCacheDir());
        DexLoader.deletePath(context.getFilesDir());
        DexLoader.deletePath(
                context.getDir(HostBrowserClassLoader.DEX_DIR_NAME, Context.MODE_PRIVATE));
    }
}
