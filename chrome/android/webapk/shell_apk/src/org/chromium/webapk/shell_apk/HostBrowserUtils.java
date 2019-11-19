// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Contains methods for getting information about host browser.
 */
public class HostBrowserUtils {
    private static final int MINIMUM_REQUIRED_CHROME_VERSION = 57;

    private static final int MINIMUM_REQUIRED_INTENT_HELPER_VERSION = 2;

    // Lowest version of Chromium which supports ShellAPK showing the splash screen.
    public static final int MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH = 78;

    private static final String VERSION_NAME_DEVELOPER_BUILD = "Developer Build";

    private static final String TAG = "cr_HostBrowserUtils";

    public static String ARC_INTENT_HELPER_BROWSER = "org.chromium.arc.intent_helper";

    /**
     * The package names of the browsers that support WebAPKs. The most preferred one comes first.
     */
    private static Set<String> sBrowsersSupportingWebApk =
            new HashSet<String>(Arrays.asList("com.google.android.apps.chrome",
                    "com.android.chrome", "com.chrome.beta", "com.chrome.dev", "com.chrome.canary",
                    "org.chromium.chrome", "org.chromium.chrome.tests", ARC_INTENT_HELPER_BROWSER));

    /** Caches the package name of the host browser. */
    private static String sHostPackage;

    /** For testing only. */
    public static void resetCachedHostPackageForTesting() {
        sHostPackage = null;
    }

    /** Returns whether the passed-in browser package name supports WebAPKs. */
    public static boolean doesBrowserSupportWebApks(String browserPackageName) {
        return sBrowsersSupportingWebApk.contains(browserPackageName);
    }

    /**
     * Returns the cached package name of the host browser to launch the WebAPK. Returns null if the
     * cached package name is no longer installed.
     */
    public static String getCachedHostBrowserPackage(Context context) {
        PackageManager packageManager = context.getPackageManager();
        if (WebApkUtils.isInstalled(packageManager, sHostPackage)) {
            return sHostPackage;
        }

        String hostPackage = getHostBrowserFromSharedPreference(context);
        if (!WebApkUtils.isInstalled(packageManager, hostPackage)) {
            hostPackage = null;
        }
        return hostPackage;
    }

    /**
     * Computes and returns the package name of the best host browser to launch the WebAPK. Returns
     * null if there is either no host browsers which support WebAPKs or if the user needs to
     * confirm the host browser selection. If the best host browser has changed, clears all of the
     * WebAPK's cached data.
     */
    public static String computeHostBrowserPackageClearCachedDataOnChange(Context context) {
        PackageManager packageManager = context.getPackageManager();
        if (WebApkUtils.isInstalled(packageManager, sHostPackage)) {
            return sHostPackage;
        }

        String hostInPreferences = getHostBrowserFromSharedPreference(context);
        sHostPackage = computeHostBrowserPackageNameInternal(context);
        if (!TextUtils.equals(sHostPackage, hostInPreferences)) {
            if (!TextUtils.isEmpty(hostInPreferences)) {
                WebApkSharedPreferences.clear(context);
                deleteInternalStorage(context);
            }
            writeHostBrowserToSharedPref(context, sHostPackage);
        }

        return sHostPackage;
    }

    /**
     * Writes the package name of the host browser to the SharedPreferences. If the host browser is
     * different than the previous one stored, delete the SharedPreference before storing the new
     * host browser.
     */
    public static void writeHostBrowserToSharedPref(Context context, String hostPackage) {
        if (TextUtils.isEmpty(hostPackage)) return;

        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(context).edit();
        editor.putString(WebApkSharedPreferences.PREF_RUNTIME_HOST, hostPackage);
        editor.apply();
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
        if (!doesBrowserSupportWebApks(hostBrowserPackageName)) {
            return true;
        }

        if (TextUtils.equals(hostBrowserPackageName, ARC_INTENT_HELPER_BROWSER)) {
            return hostBrowserMajorChromiumVersion < MINIMUM_REQUIRED_INTENT_HELPER_VERSION;
        }

        return hostBrowserMajorChromiumVersion < MINIMUM_REQUIRED_CHROME_VERSION;
    }

    /**
     * Returns whether intents (android.intent.action.MAIN, android.intent.action.SEND ...) should
     * launch {@link SplashActivity} for the given host browser params.
     */
    public static boolean shouldIntentLaunchSplashActivity(HostBrowserLauncherParams params) {
        return !params.getHostBrowserPackageName().equals(ARC_INTENT_HELPER_BROWSER)
                && params.getHostBrowserMajorChromiumVersion()
                >= MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK, or null if we did not find
     * one.
     */
    private static String computeHostBrowserPackageNameInternal(Context context) {
        PackageManager packageManager = context.getPackageManager();

        // Gets the package name of the host browser if it is stored in the SharedPreference.
        String cachedHostBrowser = getHostBrowserFromSharedPreference(context);
        if (!TextUtils.isEmpty(cachedHostBrowser)
                && WebApkUtils.isInstalled(packageManager, cachedHostBrowser)) {
            return cachedHostBrowser;
        }

        // Gets the package name of the host browser if it is specified in AndroidManifest.xml.
        String hostBrowserFromManifest =
                WebApkUtils.readMetaDataFromManifest(context, WebApkMetaDataKeys.RUNTIME_HOST);
        if (!TextUtils.isEmpty(hostBrowserFromManifest)) {
            if (WebApkUtils.isInstalled(packageManager, hostBrowserFromManifest)) {
                return hostBrowserFromManifest;
            }
            return null;
        }

        // Gets the package name of the default browser on the Android device.
        // TODO(hanxi): Investigate the best way to know which browser supports WebAPKs.
        String defaultBrowser = getDefaultBrowserPackageName(packageManager);
        if (!TextUtils.isEmpty(defaultBrowser) && doesBrowserSupportWebApks(defaultBrowser)
                && WebApkUtils.isInstalled(packageManager, defaultBrowser)) {
            return defaultBrowser;
        }

        Map<String, ResolveInfo> installedBrowsers =
                WebApkUtils.getInstalledBrowserResolveInfos(packageManager);
        if (installedBrowsers.size() == 1) {
            return installedBrowsers.keySet().iterator().next();
        }

        // If there is only one browser supporting WebAPK, and we can't decide which browser to use
        // by looking up cache, metadata and default browser, open with that browser.
        int numSupportedBrowsersInstalled = 0;
        String lastSupportedBrowser = null;
        for (String browserPackageName : installedBrowsers.keySet()) {
            if (numSupportedBrowsersInstalled > 1) break;
            if (doesBrowserSupportWebApks(browserPackageName)) {
                numSupportedBrowsersInstalled++;
                lastSupportedBrowser = browserPackageName;
            }
        }
        if (numSupportedBrowsersInstalled == 1) {
            return lastSupportedBrowser;
        }

        if (numSupportedBrowsersInstalled == 0 && installedBrowsers.containsKey(defaultBrowser)) {
            return defaultBrowser;
        }
        return null;
    }

    /** Returns the package name of the host browser cached in the SharedPreferences. */
    public static String getHostBrowserFromSharedPreference(Context context) {
        SharedPreferences sharedPref = WebApkSharedPreferences.getPrefs(context);
        return sharedPref.getString(WebApkSharedPreferences.PREF_RUNTIME_HOST, null);
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
