// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static android.content.Context.UI_MODE_SERVICE;

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Build.VERSION_CODES;

import androidx.annotation.GuardedBy;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JniType;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Nullable;

/** DeviceInfo is a utility class to access the device-related information. */
public class DeviceInfo {
    private static final String TAG = "DeviceInfo";

    private @Nullable String mGmsVersionCodeForTesting;

    /** The versionCode of Play Services. Can be overridden in tests. */
    private String mGmsVersionCode;

    /** Whether we're running on Android TV or not */
    private final boolean mIsTv;

    /** Whether we're running on an Android Automotive OS device or not. */
    private final boolean mIsAutomotive;

    /** Whether we're running on an Android Foldable OS device or not. */
    private final boolean mIsFoldable;

    /** Whether we're running on an Android Desktop OS device or not. */
    private final boolean mIsDesktop;

    /** Whether or not the device has apps installed for using custom themes. */
    private final String mCustomThemes;

    /**
     * version of the FEATURE_VULKAN_DEQP_LEVEL, if available. Queried only on Android T or above
     */
    private final int mVulkanDeqpLevel;

    @GuardedBy("CREATION_LOCK")
    private static DeviceInfo sInstance;

    private static final Object CREATION_LOCK = new Object();

    // Returns Android info fields in a specific order that the caller is expected to follow.
    // This is awkward and error-prone, but the alternative of having one JNI method per
    // field was deemed too inefficient in terms of binary size.
    @CalledByNative
    private static String[] getStringDeviceInfo() {
        return new String[] {getGmsVersionCode(), getCustomThemes()};
    }

    // Returns Android info fields in a specific order that the caller is expected to follow.
    // This is awkward and error-prone, but the alternative of having one JNI method per
    // field was deemed too inefficient in terms of binary size.
    @CalledByNative
    private static int[] getIntDeviceInfo() {
        return new int[] {
            isTV() ? 1 : 0,
            isAutomotive() ? 1 : 0,
            isFoldable() ? 1 : 0,
            isDesktop() ? 1 : 0,
            getVulkanDeqpLevel()
        };
    }

    public static String getGmsVersionCode() {
        return getInstance().mGmsVersionCodeForTesting == null
                ? getInstance().mGmsVersionCode
                : getInstance().mGmsVersionCodeForTesting;
    }

    @CalledByNativeForTesting
    public static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        getInstance().mGmsVersionCodeForTesting = gmsVersionCode;
    }

    public static boolean isTV() {
        return getInstance().mIsTv;
    }

    public static boolean isAutomotive() {
        return getInstance().mIsAutomotive;
    }

    public static boolean isFoldable() {
        return getInstance().mIsFoldable;
    }

    public static boolean isDesktop() {
        return getInstance().mIsDesktop;
    }

    public static String getCustomThemes() {
        return getInstance().mCustomThemes;
    }

    public static int getVulkanDeqpLevel() {
        return getInstance().mVulkanDeqpLevel;
    }

    private static DeviceInfo getInstance() {
        // Some tests mock out things BuildInfo is based on, so disable caching in tests to ensure
        // such mocking is not defeated by caching.
        if (BuildConfig.IS_FOR_TEST) {
            return new DeviceInfo();
        }

        synchronized (CREATION_LOCK) {
            if (sInstance == null) {
                sInstance = new DeviceInfo();
            }
            return sInstance;
        }
    }

    /**
     * Return the "long" version code of the given PackageInfo. Does the right thing for
     * before/after Android P when this got wider.
     */
    public static long packageVersionCode(PackageInfo pi) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return pi.getLongVersionCode();
        } else {
            return pi.versionCode;
        }
    }

    private DeviceInfo() {
        PackageInfo gmsPackageInfo = PackageUtils.getPackageInfo("com.google.android.gms", 0);
        mGmsVersionCode =
                gmsPackageInfo != null
                        ? String.valueOf(packageVersionCode(gmsPackageInfo))
                        : "gms versionCode not available.";

        Context appContext = ContextUtils.getApplicationContext();
        PackageManager pm = appContext.getPackageManager();
        // See https://developer.android.com/training/tv/start/hardware.html#runtime-check.
        UiModeManager uiModeManager = (UiModeManager) appContext.getSystemService(UI_MODE_SERVICE);
        mIsTv =
                uiModeManager != null
                        && uiModeManager.getCurrentModeType()
                                == Configuration.UI_MODE_TYPE_TELEVISION;

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
        mIsAutomotive = isAutomotive;

        // Detect whether device is foldable.
        mIsFoldable =
                Build.VERSION.SDK_INT >= VERSION_CODES.R
                        && pm.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE);

        mIsDesktop = pm.hasSystemFeature(PackageManager.FEATURE_PC);

        int vulkanLevel = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            FeatureInfo[] features = pm.getSystemAvailableFeatures();
            if (features != null) {
                for (FeatureInfo feature : features) {
                    if (PackageManager.FEATURE_VULKAN_DEQP_LEVEL.equals(feature.name)) {
                        vulkanLevel = feature.version;
                        break;
                    }
                }
            }
        }
        mVulkanDeqpLevel = vulkanLevel;

        // Substratum is a theme engine that enables users to use custom themes provided
        // by theme apps. Sometimes these can cause crashs if not installed correctly.
        // These crashes can be difficult to debug, so knowing if the theme manager is
        // present on the device is useful (http://crbug.com/820591).
        mCustomThemes = String.valueOf(PackageUtils.isPackageInstalled("projekt.substratum"));
    }
}
