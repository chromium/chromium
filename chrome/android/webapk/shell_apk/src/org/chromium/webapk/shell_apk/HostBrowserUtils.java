// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Contains methods for getting information about host browser. */
public class HostBrowserUtils {
    private static final int MINIMUM_REQUIRED_CHROME_VERSION = 57;

    private static final int MINIMUM_REQUIRED_INTENT_HELPER_VERSION = 2;

    // Lowest version of Chromium which supports ShellAPK showing the splash screen.
    public static final int MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH = 78;

    private static final String VERSION_NAME_DEVELOPER_BUILD = "Developer Build";

    public static String ARC_INTENT_HELPER_BROWSER = "org.chromium.arc.intent_helper";

    public static String ARC_WEBAPK_BROWSER = "org.chromium.arc.webapk";

    /**
     * The package names of the browsers that support WebAPKs. The most preferred one comes first.
     */
    private static Set<String> sBrowsersSupportingWebApk =
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

    /** Returns whether the passed-in browser package name supports WebAPKs. */
    public static boolean doesBrowserSupportWebApks(String browserPackageName) {
        return sBrowsersSupportingWebApk.contains(browserPackageName);
    }

    /** Queries the given host browser's major version. */
    public static int queryHostBrowserMajorChromiumVersion(
            Context context, String hostBrowserPackageName) {
        if (!doesBrowserSupportWebApks(hostBrowserPackageName)) {
            return -1;
        }

        PackageInfo info;
        try {
            info = context.getPackageManager().getPackageInfo(hostBrowserPackageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return -1;
        }
        String versionName = info.versionName;

        if (TextUtils.equals(versionName, VERSION_NAME_DEVELOPER_BUILD)) {
            return Integer.MAX_VALUE;
        }

        int dotIndex = versionName.indexOf(".");
        if (dotIndex < 0) {
            return -1;
        }
        try {
            return Integer.parseInt(versionName.substring(0, dotIndex));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /** Returns whether a WebAPK should be launched as a tab. See crbug.com/772398. */
    public static boolean shouldLaunchInTab(HostBrowserLauncherParams params) {
        String hostBrowserPackageName = params.getHostBrowserPackageName();
        int hostBrowserMajorChromiumVersion = params.getHostBrowserMajorChromiumVersion();

        if (TextUtils.equals(hostBrowserPackageName, ARC_INTENT_HELPER_BROWSER)) {
            return hostBrowserMajorChromiumVersion < MINIMUM_REQUIRED_INTENT_HELPER_VERSION;
        }

        if (TextUtils.equals(hostBrowserPackageName, ARC_WEBAPK_BROWSER)) {
            return false;
        }

        return hostBrowserMajorChromiumVersion < MINIMUM_REQUIRED_CHROME_VERSION;
    }

    /**
     * Returns whether intents (android.intent.action.MAIN, android.intent.action.SEND ...) should
     * launch {@link SplashActivity} for the given host browser params.
     */
    public static boolean shouldIntentLaunchSplashActivity(HostBrowserLauncherParams params) {
        return params.isNewStyleWebApk()
                && !params.getHostBrowserPackageName().equals(ARC_INTENT_HELPER_BROWSER)
                && !params.getHostBrowserPackageName().equals(ARC_WEBAPK_BROWSER)
                && params.getHostBrowserMajorChromiumVersion()
                        >= MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH;
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
