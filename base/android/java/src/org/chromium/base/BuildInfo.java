// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static android.content.Context.UI_MODE_SERVICE;

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Build.VERSION;
import android.text.TextUtils;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.compat.ApiHelperForP;
import org.chromium.build.BuildConfig;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo} information. This is
 * primarily of use for accessing package information from native code.
 */
public class BuildInfo {
    private static final String TAG = "BuildInfo";
    private static final int MAX_FINGERPRINT_LENGTH = 128;

    private static PackageInfo sBrowserPackageInfo;
    private static boolean sInitialized;

    /** Not a member variable to avoid creating the instance early (it is set early in start up). */
    private static String sFirebaseAppId = "";

    /** The application name (e.g. "Chrome"). For WebView, this is name of the embedding app. */
    public final String hostPackageLabel;
    /** By default: same as versionCode. For WebView: versionCode of the embedding app. */
    public final long hostVersionCode;
    /** The packageName of Chrome/WebView. Use application context for host app packageName. */
    public final String packageName;
    /** The versionCode of the apk. */
    public final long versionCode;
    /** The versionName of Chrome/WebView. Use application context for host app versionName. */
    public final String versionName;
    /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
    public final String installerPackageName;
    /** The versionCode of Play Services (for crash reporting). */
    public final String gmsVersionCode;
    /** Formatted ABI string (for crash reporting). */
    public final String abiString;
    /** Truncated version of Build.FINGERPRINT (for crash reporting). */
    public final String androidBuildFingerprint;
    /** Whether or not the device has apps installed for using custom themes. */
    public final String customThemes;
    /** Product version as stored in Android resources. */
    public final String resourcesVersion;
    /** Whether we're running on Android TV or not */
    public final boolean isTV;

    private static class Holder { private static BuildInfo sInstance = new BuildInfo(); }

    @CalledByNative
    private static String[] getAll() {
        BuildInfo buildInfo = getInstance();
        String hostPackageName = ContextUtils.getApplicationContext().getPackageName();
        return new String[] {
                Build.BRAND,
                Build.DEVICE,
                Build.ID,
                Build.MANUFACTURER,
                Build.MODEL,
                String.valueOf(Build.VERSION.SDK_INT),
                Build.TYPE,
                Build.BOARD,
                hostPackageName,
                String.valueOf(buildInfo.hostVersionCode),
                buildInfo.hostPackageLabel,
                buildInfo.packageName,
                String.valueOf(buildInfo.versionCode),
                buildInfo.versionName,
                buildInfo.androidBuildFingerprint,
                buildInfo.gmsVersionCode,
                buildInfo.installerPackageName,
                buildInfo.abiString,
                sFirebaseAppId,
                buildInfo.customThemes,
                buildInfo.resourcesVersion,
                String.valueOf(
                        ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion),
                isDebugAndroid() ? "1" : "0",
                buildInfo.isTV ? "1" : "0",
                Build.VERSION.INCREMENTAL,
        };
    }

    private static String nullToEmpty(CharSequence seq) {
        return seq == null ? "" : seq.toString();
    }

    private static long packageVersionCode(PackageInfo pi) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return ApiHelperForP.getLongVersionCode(pi);
        } else {
            return pi.versionCode;
        }
    }

    /**
     * @param packageInfo Package for Chrome/WebView (as opposed to host app).
     */
    public static void setBrowserPackageInfo(PackageInfo packageInfo) {
        assert !sInitialized;
        sBrowserPackageInfo = packageInfo;
    }

    public static BuildInfo getInstance() {
        return Holder.sInstance;
    }

    private BuildInfo() {
        sInitialized = true;
        try {
            Context appContext = ContextUtils.getApplicationContext();
            String hostPackageName = appContext.getPackageName();
            PackageManager pm = appContext.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(hostPackageName, 0);
            hostVersionCode = packageVersionCode(pi);
            if (sBrowserPackageInfo != null) {
                packageName = sBrowserPackageInfo.packageName;
                versionCode = packageVersionCode(sBrowserPackageInfo);
                versionName = nullToEmpty(sBrowserPackageInfo.versionName);
                sBrowserPackageInfo = null;
            } else {
                packageName = hostPackageName;
                versionCode = hostVersionCode;
                versionName = nullToEmpty(pi.versionName);
            }

            hostPackageLabel = nullToEmpty(pm.getApplicationLabel(pi.applicationInfo));
            installerPackageName = nullToEmpty(pm.getInstallerPackageName(packageName));

            PackageInfo gmsPackageInfo = null;
            try {
                gmsPackageInfo = pm.getPackageInfo("com.google.android.gms", 0);
            } catch (NameNotFoundException e) {
                Log.d(TAG, "GMS package is not found.");
            }
            gmsVersionCode = gmsPackageInfo != null
                    ? String.valueOf(packageVersionCode(gmsPackageInfo))
                    : "gms versionCode not available.";

            String hasCustomThemes = "true";
            try {
                // Substratum is a theme engine that enables users to use custom themes provided
                // by theme apps. Sometimes these can cause crashs if not installed correctly.
                // These crashes can be difficult to debug, so knowing if the theme manager is
                // present on the device is useful (http://crbug.com/820591).
                pm.getPackageInfo("projekt.substratum", 0);
            } catch (NameNotFoundException e) {
                hasCustomThemes = "false";
            }
            customThemes = hasCustomThemes;

            String currentResourcesVersion = "Not Enabled";
            // Controlled by target specific build flags.
            if (BuildConfig.R_STRING_PRODUCT_VERSION != 0) {
                try {
                    // This value can be compared with the actual product version to determine if
                    // corrupted resources were the cause of a crash. This can happen if the app
                    // loads resources from the outdated package  during an update
                    // (http://crbug.com/820591).
                    currentResourcesVersion = ContextUtils.getApplicationContext().getString(
                            BuildConfig.R_STRING_PRODUCT_VERSION);
                } catch (Exception e) {
                    currentResourcesVersion = "Not found";
                }
            }
            resourcesVersion = currentResourcesVersion;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                abiString = TextUtils.join(", ", Build.SUPPORTED_ABIS);
            } else {
                abiString = String.format("ABI1: %s, ABI2: %s", Build.CPU_ABI, Build.CPU_ABI2);
            }

            // The value is truncated, as this is used for crash and UMA reporting.
            androidBuildFingerprint = Build.FINGERPRINT.substring(
                    0, Math.min(Build.FINGERPRINT.length(), MAX_FINGERPRINT_LENGTH));

            // See https://developer.android.com/training/tv/start/hardware.html#runtime-check.
            UiModeManager uiModeManager =
                    (UiModeManager) appContext.getSystemService(UI_MODE_SERVICE);
            isTV = uiModeManager != null
                    && uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;

        } catch (NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Check if this is a debuggable build of Android. Use this to enable developer-only features.
     * This is a rough approximation of the hidden API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    /**
     * Checks if the codename is a matching or higher version than the given build value.
     * @param codename the requested build codename, e.g. {@code "O"} or {@code "OMR1"}
     * @param buildCodename the value of {@link Build.VERSION#CODENAME}
     *
     * @return {@code true} if APIs from the requested codename are available in the build.
     */
    private static boolean isAtLeastPreReleaseCodename(
            @NonNull String codename, @NonNull String buildCodename) {
        // Special case "REL", which means the build is not a pre-release build.
        if ("REL".equals(buildCodename)) {
            return false;
        }
        // Otherwise lexically compare them.  Return true if the build codename is equal to or
        // greater than the requested codename.
        return buildCodename.compareTo(codename) >= 0;
    }

    // The markers Begin:BuildCompat and End:BuildCompat delimit code
    // that is autogenerated from Android sources.
    // Begin:BuildCompat S

    /**
     * Checks if the device is running on a pre-release version of Android S or a release version of
     * Android S or newer.
     *
     * @return {@code true} if S APIs are available for use, {@code false} otherwise
     */
    @ChecksSdkIntAtLeast(api = 31, codename = "S")
    public static boolean isAtLeastS() {
        return VERSION.SDK_INT >= 31 || isAtLeastPreReleaseCodename("S", VERSION.CODENAME);
    }

    /**
     * Checks if the application targets at least released SDK S
     */
    public static boolean targetsAtLeastS() {
        return ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion >= 31;
    }

    // End:BuildCompat

    public static void setFirebaseAppId(String id) {
        assert sFirebaseAppId.equals("");
        sFirebaseAppId = id;
    }

    public static String getFirebaseAppId() {
        return sFirebaseAppId;
    }

}
