// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static android.content.Context.UI_MODE_SERVICE;

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.OptIn;
import androidx.annotation.VisibleForTesting;
import androidx.core.os.BuildCompat;

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
    private static ApplicationInfo sBrowserApplicationInfo;
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
    /** Whether we're running on an Android Automotive OS device or not. */
    public final boolean isAutomotive;

    private static class Holder { private static BuildInfo sInstance = new BuildInfo(); }

    @CalledByNative
    private static String[] getAll() {
        return BuildInfo.getInstance().getAllProperties();
    }

    /** Returns a serialized string array of all properties of this class. */
    @VisibleForTesting
    String[] getAllProperties() {
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
                String.valueOf(hostVersionCode),
                hostPackageLabel,
                packageName,
                String.valueOf(versionCode),
                versionName,
                androidBuildFingerprint,
                gmsVersionCode,
                installerPackageName,
                abiString,
                sFirebaseAppId,
                customThemes,
                resourcesVersion,
                String.valueOf(
                        ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion),
                isDebugAndroid() ? "1" : "0",
                isTV ? "1" : "0",
                Build.VERSION.INCREMENTAL,
                Build.HARDWARE,
                isAtLeastT() ? "1" : "0",
                isAutomotive ? "1" : "0",
        };
    }

    private static String nullToEmpty(CharSequence seq) {
        return seq == null ? "" : seq.toString();
    }

    /**
     * Return the "long" version code of the given PackageInfo.
     * Does the right thing for before/after Android P when this got wider.
     */
    public static long packageVersionCode(PackageInfo pi) {
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

    /**
     * @return ApplicationInfo for Chrome/WebView (as opposed to host app).
     */
    public ApplicationInfo getBrowserApplicationInfo() {
        return sBrowserApplicationInfo;
    }

    public static BuildInfo getInstance() {
        return Holder.sInstance;
    }

    @VisibleForTesting
    BuildInfo() {
        sInitialized = true;

        Context appContext = ContextUtils.getApplicationContext();
        String hostPackageName = appContext.getPackageName();
        PackageManager pm = appContext.getPackageManager();
        PackageInfo pi = PackageUtils.getPackageInfo(hostPackageName, 0);
        hostVersionCode = packageVersionCode(pi);
        if (sBrowserPackageInfo != null) {
            packageName = sBrowserPackageInfo.packageName;
            versionCode = packageVersionCode(sBrowserPackageInfo);
            versionName = nullToEmpty(sBrowserPackageInfo.versionName);
            sBrowserApplicationInfo = sBrowserPackageInfo.applicationInfo;
            sBrowserPackageInfo = null;
        } else {
            packageName = hostPackageName;
            versionCode = hostVersionCode;
            versionName = nullToEmpty(pi.versionName);
            sBrowserApplicationInfo = appContext.getApplicationInfo();
        }

        hostPackageLabel = nullToEmpty(pm.getApplicationLabel(pi.applicationInfo));
        installerPackageName = nullToEmpty(pm.getInstallerPackageName(packageName));

        PackageInfo gmsPackageInfo = PackageUtils.getPackageInfo("com.google.android.gms", 0);
        gmsVersionCode = gmsPackageInfo != null ? String.valueOf(packageVersionCode(gmsPackageInfo))
                                                : "gms versionCode not available.";

        // Substratum is a theme engine that enables users to use custom themes provided
        // by theme apps. Sometimes these can cause crashs if not installed correctly.
        // These crashes can be difficult to debug, so knowing if the theme manager is
        // present on the device is useful (http://crbug.com/820591).
        customThemes = String.valueOf(PackageUtils.isPackageInstalled("projekt.substratum"));

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
        UiModeManager uiModeManager = (UiModeManager) appContext.getSystemService(UI_MODE_SERVICE);
        isTV = uiModeManager != null
                && uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;

        boolean isAutomotive;
        try {
            isAutomotive = pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
        } catch (SecurityException e) {
            Log.e(TAG, "Unable to query for Automotive system feature", e);

            // `hasSystemFeature` can possibly throw an exception on modified instances of
            // Android. In this case, assume the device is not a car since automotive vehicles
            // should not have such a modification.
            isAutomotive = false;
        }
        this.isAutomotive = isAutomotive;
    }

    /**
     * Check if this is a debuggable build of Android. Use this to enable developer-only features.
     * This is a rough approximation of the hidden API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    /**
     * Wrap BuildCompat.isAtLeastT. This enables it to be shadowed in Robolectric tests.
     */
    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    public static boolean isAtLeastT() {
        return BuildCompat.isAtLeastT();
    }

    /**
     * Checks if the application targets pre-release SDK T.
     * This must be manually maintained as the SDK goes through finalization!
     * Avoid depending on this if possible; this is only intended for WebView.
     */
    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    public static boolean targetsAtLeastT() {
        int target = ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;

        // Logic for pre-API-finalization:
        // return BuildCompat.isAtLeastT() && target == Build.VERSION_CODES.CUR_DEVELOPMENT;

        // Logic for after API finalization but before public SDK release has to
        // just hardcode the appropriate SDK integer. This will include Android
        // builds with the finalized SDK, and also pre-API-finalization builds
        // (because CUR_DEVELOPMENT == 10000).
        // return target >= 33;

        // Once the public SDK is upstreamed we can use the defined constant,
        // deprecate this, then eventually inline this at all callsites and
        // remove it.
        return target >= Build.VERSION_CODES.TIRAMISU;
    }

    public static void setFirebaseAppId(String id) {
        assert sFirebaseAppId.equals("");
        sFirebaseAppId = id;
    }

    public static String getFirebaseAppId() {
        return sFirebaseAppId;
    }

    /**
     * This operation is not thread-safe. Construction of the new BuildInfo object will
     * happen synchronously and result in a consistent BuildInfo, but references to the static
     * BuildInfo instance may be out of date in some threads.
     */
    @VisibleForTesting
    public static void resetForTesting() {
        Holder.sInstance = new BuildInfo();
    }
}
