// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Contains methods for getting information about host browser. */
public class HostBrowserUtils {
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
     * Contains a host browser's package name, and, if applicable, also its component name. The
     * component name is expected to be null if the WebAPK is bound, and valid if the WebAPK is
     * unbound or effectively unbound.
     */
    public static class PackageNameAndComponentName {
        @NonNull private String mPackageName;
        @Nullable private ComponentName mComponentName;

        public PackageNameAndComponentName(@NonNull String packageName) {
            mPackageName = packageName;
            mComponentName = null;
        }

        public PackageNameAndComponentName(@NonNull ComponentName componentName) {
            mPackageName = componentName.getPackageName();
            mComponentName = componentName;
        }

        /** Returns the package name of the host browser. */
        public @NonNull String getPackageName() {
            return mPackageName;
        }

        /**
         * Returns the component name of the host browser if the WebAPK is unbound, or null if it is
         * bound.
         */
        public @Nullable ComponentName getComponentName() {
            return mComponentName;
        }
    }

    /**
     * Returns the package name and (if the WebAPK is unbound) component name of the host browser to
     * launch the WebAPK, or null if we did not find a host browser.
     */
    public static @Nullable PackageNameAndComponentName
            computeHostBrowserPackageNameAndComponentName(Context context) {
        // Gets the package name of the host browser if it is specified in AndroidManifest.xml.
        String hostBrowserFromManifest = getHostBrowserFromManifest(context);
        if (!TextUtils.isEmpty(hostBrowserFromManifest)) {
            return new PackageNameAndComponentName(hostBrowserFromManifest);
        }

        PackageManager packageManager = context.getPackageManager();

        // Gets the package name of the default browser on the Android device.
        ComponentName defaultBrowser = getDefaultBrowserComponentName(packageManager);
        if (defaultBrowser != null
                && !TextUtils.isEmpty(defaultBrowser.getPackageName())
                && WebApkUtils.isInstalled(packageManager, defaultBrowser.getPackageName())) {
            return new PackageNameAndComponentName(defaultBrowser);
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

    /** Returns the component name of the default browser on the Android device. */
    private static @Nullable ComponentName getDefaultBrowserComponentName(
            PackageManager packageManager) {
        Intent browserIntent = WebApkUtils.getQueryInstalledBrowsersIntent();
        ResolveInfo resolveInfo =
                packageManager.resolveActivity(browserIntent, PackageManager.MATCH_DEFAULT_ONLY);
        return WebApkUtils.getComponentNameFromResolveInfo(resolveInfo);
    }

    /** Deletes the internal storage for the given context. */
    private static void deleteInternalStorage(Context context) {
        DexLoader.deletePath(context.getCacheDir());
        DexLoader.deletePath(context.getFilesDir());
        DexLoader.deletePath(
                context.getDir(HostBrowserClassLoader.DEX_DIR_NAME, Context.MODE_PRIVATE));
    }
}
