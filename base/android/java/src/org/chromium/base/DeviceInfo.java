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
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** DeviceInfo is a utility class to access the device-related information. */
@JNINamespace("base::android::device_info")
@NullMarked
public final class DeviceInfo {
    private static final String TAG = "DeviceInfo";

    private static @Nullable String sGmsVersionCodeForTesting;
    private static boolean sInitialized;
    private final IDeviceInfo mIDeviceInfo;

    @GuardedBy("CREATION_LOCK")
    private static @Nullable DeviceInfo sInstance;

    private static final Object CREATION_LOCK = new Object();

    // Called by the native code to retrieve field values. There is no easy way to
    // return several fields from Java to native, so instead this calls back into
    // native, passing the fields as parameters to a native function.
    // The native code expects native `fillFields()` to be called inline from this
    // function.
    @CalledByNative
    private static void nativeReadyForFields() {
        sendToNative(getInstance().mIDeviceInfo);
    }

    public static void sendToNative(IDeviceInfo info) {
        DeviceInfoJni.get()
                .fillFields(
                        /* gmsVersionCode= */ info.gmsVersionCode,
                        /* isTV= */ info.isTv,
                        /* isAutomotive= */ info.isAutomotive,
                        /* isFoldable= */ info.isFoldable,
                        /* isDesktop= */ info.isDesktop,
                        /* vulkanDeqpLevel= */ info.vulkanDeqpLevel);
    }

    public static IDeviceInfo getAidlInfo() {
        return getInstance().mIDeviceInfo;
    }

    public static String getGmsVersionCode() {
        return getInstance().mIDeviceInfo.gmsVersionCode;
    }

    @CalledByNativeForTesting
    public static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        sGmsVersionCodeForTesting = gmsVersionCode;
        // Every time we call getInstance in a test we reconstruct the mIDeviceInfo object, so we
        // don't need to set mIDeviceInfo's copy here as it'll just get reconstructed.
        ResettersForTesting.register(() -> sGmsVersionCodeForTesting = null);
    }

    public static boolean isTV() {
        return getInstance().mIDeviceInfo.isTv;
    }

    public static boolean isAutomotive() {
        return getInstance().mIDeviceInfo.isAutomotive;
    }

    public static boolean isFoldable() {
        return getInstance().mIDeviceInfo.isFoldable;
    }

    public static boolean isDesktop() {
        return getInstance().mIDeviceInfo.isDesktop;
    }

    public static int getVulkanDeqpLevel() {
        return getInstance().mIDeviceInfo.vulkanDeqpLevel;
    }

    public static boolean isInitializedForTesting() {
        return sInitialized;
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
        mIDeviceInfo = new IDeviceInfo();
        sInitialized = true;
        PackageInfo gmsPackageInfo = PackageUtils.getPackageInfo("com.google.android.gms", 0);
        mIDeviceInfo.gmsVersionCode =
                gmsPackageInfo != null
                        ? String.valueOf(packageVersionCode(gmsPackageInfo))
                        : "gms versionCode not available.";
        if (sGmsVersionCodeForTesting != null) {
            mIDeviceInfo.gmsVersionCode = sGmsVersionCodeForTesting;
        }

        Context appContext = ContextUtils.getApplicationContext();
        PackageManager pm = appContext.getPackageManager();
        // See https://developer.android.com/training/tv/start/hardware.html#runtime-check.
        UiModeManager uiModeManager = (UiModeManager) appContext.getSystemService(UI_MODE_SERVICE);
        mIDeviceInfo.isTv =
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
        mIDeviceInfo.isAutomotive = isAutomotive;

        // Detect whether device is foldable.
        mIDeviceInfo.isFoldable =
                Build.VERSION.SDK_INT >= VERSION_CODES.R
                        && pm.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE);

        mIDeviceInfo.isDesktop = pm.hasSystemFeature(PackageManager.FEATURE_PC);

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
        mIDeviceInfo.vulkanDeqpLevel = vulkanLevel;
    }

    @NativeMethods
    interface Natives {
        void fillFields(
                @JniType("std::string") String gmsVersionCode,
                boolean isTV,
                boolean isAutomotive,
                boolean isFoldable,
                boolean isDesktop,
                int vulkanDeqpLevel);
    }
}
