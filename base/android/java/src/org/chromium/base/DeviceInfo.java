// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static android.content.Context.UI_MODE_SERVICE;

import android.app.UiModeManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Process;
import android.provider.Settings;
import android.util.DisplayMetrics;

import androidx.annotation.GuardedBy;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Caches device info during app start-up. For values that might change during the lifetime of the
 * app, refer to @see org.chromium.ui.base.DeviceFormFactor.java
 */
@JNINamespace("base::android::device_info")
@NullMarked
public final class DeviceInfo {
    private static final String TAG = "DeviceInfo";

    private static @Nullable String sGmsVersionCodeForTesting;
    private static @Nullable Boolean sIsAutomotiveForTesting;
    private static boolean sInitialized;
    private static @Nullable Boolean sIsXrForTesting;
    private static @Nullable Boolean sIsRetailDemoModeForTesting;
    private final IDeviceInfo mIDeviceInfo;
    private @Nullable Boolean mIsRetailDemoMode;
    private @Nullable ApplicationInfo mGmsAppInfo;

    // This is the minimum width in DP that defines a large display device
    public static final int LARGE_DISPLAY_MIN_SCREEN_WIDTH_600_DP = 600;

    @GuardedBy("CREATION_LOCK")
    private static @Nullable DeviceInfo sInstance;

    private static final Object CREATION_LOCK = new Object();

    private static boolean sIsNativeLoaded;

    // Called by the native code to retrieve field values. There is no easy way to
    // return several fields from Java to native, so instead this calls back into
    // native, passing the fields as parameters to a native function.
    // The native code expects native `fillFields()` to be called inline from this
    // function.
    @CalledByNative
    private static void nativeReadyForFields() {
        sendToNative(getInstance().mIDeviceInfo);
        sIsNativeLoaded = true;
    }

    public static void sendToNative(IDeviceInfo info) {
        DeviceInfoJni.get()
                .fillFields(
                        /* gmsVersionCode= */ info.gmsVersionCode,
                        /* isTV= */ info.isTv,
                        /* isAutomotive= */ info.isAutomotive,
                        /* isFoldable= */ info.isFoldable,
                        /* isDesktop= */ info.isDesktop,
                        /* vulkanDeqpLevel= */ info.vulkanDeqpLevel,
                        /* isXr= */ (sIsXrForTesting != null) ? sIsXrForTesting : info.isXr,
                        /* wasLaunchedOnLargeDisplay= */ info.wasLaunchedOnLargeDisplay);
    }

    public static IDeviceInfo getAidlInfo() {
        return getInstance().mIDeviceInfo;
    }

    public static String getGmsVersionCode() {
        return getInstance().mIDeviceInfo.gmsVersionCode;
    }

    public static @Nullable ApplicationInfo getGmsAppInfo() {
        return getInstance().mGmsAppInfo;
    }

    @CalledByNativeForTesting
    public static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        sGmsVersionCodeForTesting = gmsVersionCode;
        // Every time we call getInstance in a test we reconstruct the mIDeviceInfo object, so we
        // don't need to set mIDeviceInfo's copy here as it'll just get reconstructed.
        ResettersForTesting.register(() -> sGmsVersionCodeForTesting = null);
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
    }

    public static void setIsAutomotiveForTesting(boolean isAutomotive) {
        sIsAutomotiveForTesting = isAutomotive;
        ResettersForTesting.register(() -> sIsAutomotiveForTesting = null);
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
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

    public static boolean isXr() {
        return (sIsXrForTesting != null) ? sIsXrForTesting : getInstance().mIDeviceInfo.isXr;
    }

    public static boolean isRetailDemoMode() {
        if (sIsRetailDemoModeForTesting != null) {
            return sIsRetailDemoModeForTesting;
        }
        // Always assume false for tests, unless specifically overridden by a test.
        if (BuildConfig.IS_FOR_TEST) {
            return false;
        }
        DeviceInfo instance = getInstance();
        boolean ret;
        if (instance.mIsRetailDemoMode != null) {
            ret = instance.mIsRetailDemoMode;
        } else {
            ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
            // Android demo mode (Settings.Global.DEVICE_DEMO_MODE is @hide).
            ret = Settings.Global.getInt(resolver, "device_demo_mode", 0) != 0;
            instance.mIsRetailDemoMode = ret;
        }
        return ret;
    }

    @CalledByNative
    public static String getDeviceName() {
        return Settings.Global.getString(
                ContextUtils.getApplicationContext().getContentResolver(), "device_name");
    }

    public static boolean isInitializedForTesting() {
        return sInitialized;
    }

    @CalledByNativeForTesting
    public static void setIsXrForTesting(boolean value) {
        sIsXrForTesting = value;
        ResettersForTesting.register(() -> sIsXrForTesting = null);
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
    }

    @CalledByNativeForTesting
    public static void resetIsXrForTesting() {
        sIsXrForTesting = null;
    }

    public static void setIsRetailDemoModeForTesting(boolean value) {
        sIsRetailDemoModeForTesting = value;
        ResettersForTesting.register(() -> sIsRetailDemoModeForTesting = null);
    }

    private static DeviceInfo getInstance() {
        // Some tests mock out things DeviceInfo is based on, so disable caching in tests to ensure
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

    /**
     * @return CPU architecture name, see "arch:" in:
     *     https://chromium.googlesource.com/chromium/src.git/+/master/docs/updater/protocol_3_1.md
     */
    public static String getArch() {
        boolean is64Bit = Process.is64Bit();
        if (NativeLibraries.sCpuFamily == NativeLibraries.CPU_FAMILY_ARM) {
            return is64Bit ? "arm64" : "arm";
        } else if (NativeLibraries.sCpuFamily == NativeLibraries.CPU_FAMILY_X86) {
            return is64Bit ? "x86_64" : "x86";
        }
        return "";
    }

    /**
     * @return The device's screen width in density-independent pixels (dp).
     */
    private static int getDeviceWidthInDp() {
        DisplayMetrics displayMetrics =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        return (int) (displayMetrics.widthPixels / displayMetrics.density);
    }

    private DeviceInfo() {
        mIDeviceInfo = new IDeviceInfo();
        sInitialized = true;
        PackageInfo gmsPackageInfo = PackageUtils.getPackageInfo("com.google.android.gms", 0);
        String gmsVersionCode;
        if (gmsPackageInfo != null) {
            mGmsAppInfo = gmsPackageInfo.applicationInfo;
            gmsVersionCode = String.valueOf(packageVersionCode(gmsPackageInfo));
        } else {
            gmsVersionCode = "gms versionCode not available.";
        }
        if (sGmsVersionCodeForTesting != null) {
            gmsVersionCode = sGmsVersionCodeForTesting;
        }
        mIDeviceInfo.gmsVersionCode = gmsVersionCode;

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

        if (sIsAutomotiveForTesting != null) {
            mIDeviceInfo.isAutomotive = sIsAutomotiveForTesting;
        }

        mIDeviceInfo.isDesktop =
                (BuildConfig.IS_DESKTOP_ANDROID && pm.hasSystemFeature(PackageManager.FEATURE_PC))
                        || CommandLine.getInstance().hasSwitch(BaseSwitches.FORCE_DESKTOP_ANDROID);

        // Detect whether device is foldable.
        mIDeviceInfo.isFoldable =
                !mIDeviceInfo.isDesktop
                        && Build.VERSION.SDK_INT >= VERSION_CODES.R
                        && pm.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE);

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

        mIDeviceInfo.wasLaunchedOnLargeDisplay =
                getDeviceWidthInDp() >= LARGE_DISPLAY_MIN_SCREEN_WIDTH_600_DP;

        mIDeviceInfo.isXr = pm.hasSystemFeature("android.software.xr.api.openxr");
    }

    @NativeMethods
    interface Natives {
        void fillFields(
                @JniType("std::string") String gmsVersionCode,
                boolean isTV,
                boolean isAutomotive,
                boolean isFoldable,
                boolean isDesktop,
                int vulkanDeqpLevel,
                boolean isXr,
                boolean wasLaunchedOnLargeDisplay);
    }
}
