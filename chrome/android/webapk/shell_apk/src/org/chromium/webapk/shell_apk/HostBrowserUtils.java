// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Contains methods for getting information about host browser.
 */
public class HostBrowserUtils {
    public static final String SHARED_PREF_RUNTIME_HOST = "runtime_host";

    private static final int MINIMUM_REQUIRED_CHROME_VERSION = 57;

    private static final int MINIMUM_REQUIRED_INTENT_HELPER_VERSION = 2;

    private static final String TAG = "cr_HostBrowserUtils";

    /**
     * The package names of the browsers that support WebAPKs. The most preferred one comes first.
     */
    private static List<String> sBrowsersSupportingWebApk =
            new ArrayList<String>(Arrays.asList("com.google.android.apps.chrome",
                    "com.android.chrome", "com.chrome.beta", "com.chrome.dev", "com.chrome.canary",
                    "org.chromium.chrome", "org.chromium.arc.intent_helper"));

    /** Caches the package name of the host browser. */
    private static String sHostPackage;

    /** For testing only. */
    public static void resetCachedHostPackageForTesting() {
        sHostPackage = null;
    }

    /**
     * Returns a list of browsers that support WebAPKs. TODO(hanxi): Replace this function once we
     * figure out a better way to know which browser supports WebAPKs.
     */
    public static List<String> getBrowsersSupportingWebApk() {
        return sBrowsersSupportingWebApk;
    }

    /**
     * Returns a Context for the host browser that was specified when building the WebAPK.
     * @param context A context.
     * @return The remote context. Returns null on an error.
     */
    public static Context getHostBrowserContext(Context context) {
        try {
            String hostPackage = getHostBrowserPackageName(context);
            return context.getApplicationContext().createPackageContext(hostPackage, 0);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK. Also caches the package
     * name in the SharedPreference if it is not null.
     * @param context A context.
     * @return The package name. Returns null on an error.
     */
    public static String getHostBrowserPackageName(Context context) {
        if (sHostPackage == null
                || !WebApkUtils.isInstalled(context.getPackageManager(), sHostPackage)) {
            sHostPackage = getHostBrowserPackageNameInternal(context);
            if (sHostPackage != null) {
                writeHostBrowserToSharedPref(context, sHostPackage);
            }
        }

        return sHostPackage;
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK, or null if we did not find
     * one.
     */
    private static String getHostBrowserPackageNameInternal(Context context) {
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
        String defaultBrowser = getDefaultBrowserPackageName(context.getPackageManager());
        if (!TextUtils.isEmpty(defaultBrowser) && sBrowsersSupportingWebApk.contains(defaultBrowser)
                && WebApkUtils.isInstalled(packageManager, defaultBrowser)) {
            return defaultBrowser;
        }

        // If there is only one browser supporting WebAPK, and we can't decide which browser to use
        // by looking up cache, metadata and default browser, open with that browser.
        int availableBrowserCounter = 0;
        String lastSupportedBrowser = null;
        for (String packageName : sBrowsersSupportingWebApk) {
            if (availableBrowserCounter > 1) break;
            if (WebApkUtils.isInstalled(packageManager, packageName)) {
                availableBrowserCounter++;
                lastSupportedBrowser = packageName;
            }
        }
        if (availableBrowserCounter == 1) {
            return lastSupportedBrowser;
        }
        return null;
    }

    /**
     * Returns the uid for the host browser that was specified when building the WebAPK.
     * @param context A context.
     * @return The application uid. Returns -1 on an error.
     */
    public static int getHostBrowserUid(Context context) {
        String hostPackageName = getHostBrowserPackageName(context);
        if (hostPackageName == null) {
            return -1;
        }
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo = packageManager.getApplicationInfo(
                    hostPackageName, PackageManager.GET_META_DATA);
            return appInfo.uid;
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }
        return -1;
    }

    /** Returns the package name of the host browser cached in the SharedPreferences. */
    public static String getHostBrowserFromSharedPreference(Context context) {
        SharedPreferences sharedPref =
                context.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        return sharedPref.getString(SHARED_PREF_RUNTIME_HOST, null);
    }

    /** Returns the package name of the default browser on the Android device. */
    private static String getDefaultBrowserPackageName(PackageManager packageManager) {
        Intent browserIntent = WebApkUtils.getQueryInstalledBrowsersIntent();
        ResolveInfo resolveInfo =
                packageManager.resolveActivity(browserIntent, PackageManager.MATCH_DEFAULT_ONLY);
        if (resolveInfo == null || resolveInfo.activityInfo == null) return null;

        return resolveInfo.activityInfo.packageName;
    }

    /**
     * Writes the package name of the host browser to the SharedPreferences. If the host browser is
     * different than the previous one stored, delete the SharedPreference before storing the new
     * host browser.
     */
    public static void writeHostBrowserToSharedPref(Context context, String hostPackage) {
        if (TextUtils.isEmpty(hostPackage)) return;

        SharedPreferences sharedPref =
                context.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPref.edit();
        editor.putString(SHARED_PREF_RUNTIME_HOST, hostPackage);
        editor.apply();
    }

    /** Queries the given host browser's major version. */
    public static int queryHostBrowserMajorChromiumVersion(
            Context context, String hostBrowserPackageName) {
        PackageInfo info;
        try {
            info = context.getPackageManager().getPackageInfo(hostBrowserPackageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return -1;
        }
        String versionName = info.versionName;
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
    public static boolean shouldLaunchInTab(
            String hostBrowserPackageName, int hostBrowserChromiumMajorVersion) {
        if (!sBrowsersSupportingWebApk.contains(hostBrowserPackageName)) {
            return true;
        }

        if (TextUtils.equals(hostBrowserPackageName, "org.chromium.arc.intent_helper")) {
            return hostBrowserChromiumMajorVersion < MINIMUM_REQUIRED_INTENT_HELPER_VERSION;
        }

        return hostBrowserChromiumMajorVersion < MINIMUM_REQUIRED_CHROME_VERSION;
    }
}
