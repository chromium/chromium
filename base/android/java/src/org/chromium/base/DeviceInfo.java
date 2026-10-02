// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Process;
import android.provider.Settings;
import android.util.DisplayMetrics;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Caches device info for the lifetime of the process. Most fields are initialized during app
 * start-up, while GMS info is initialized on first use. For values that might change during the
 * lifetime of the app, refer to @see org.chromium.ui.base.DeviceFormFactor.java
 */
@JNINamespace("base::android::device_info")
@NullMarked
public final class DeviceInfo {
    private static final String TAG = "DeviceInfo";

    @VisibleForTesting
    static final String XR_OPENXR_FEATURE_NAME = "android.software.xr.api.openxr";

    @GuardedBy("GMS_INFO_LOCK")
    private static @Nullable String sGmsVersionCodeForTesting;

    private static @Nullable Boolean sIsAutomotiveForTesting;
    private static @Nullable Boolean sIsTVForTesting;
    private static boolean sInitialized;
    private static @Nullable Boolean sIsXrForTesting;
    private static @Nullable Boolean sIsRetailDemoModeForTesting;
    private static @Nullable Boolean sIsDesktopForTesting;
    private static @Nullable Boolean sIsFoldableForTesting;
    private final IDeviceInfo mIDeviceInfo;
    private @Nullable Boolean mIsRetailDemoMode;

    // This is the minimum width in DP that defines a large display device
    public static final int LARGE_DISPLAY_MIN_SCREEN_WIDTH_600_DP = 600;

    @GuardedBy("CREATION_LOCK")
    private static @Nullable DeviceInfo sInstance;

    private static final Object CREATION_LOCK = new Object();

    private static final Object GMS_INFO_LOCK = new Object();

    @GuardedBy("GMS_INFO_LOCK")
    private static @Nullable GmsInfo sGmsInfo;

    @IntDef({FormFactor.TV, FormFactor.AUTOMOTIVE, FormFactor.DESKTOP, FormFactor.XR})
    @Retention(RetentionPolicy.SOURCE)
    private @interface FormFactor {
        int TV = 0;
        int AUTOMOTIVE = 1;
        int DESKTOP = 2;
        int XR = 3;
    }

    private static boolean sIsNativeLoaded;
    private static volatile boolean sIsGmsVersionNativeLoaded;

    private static final class GmsInfo {
        final String mVersionCode;
        final @Nullable ApplicationInfo mApplicationInfo;

        GmsInfo(String versionCode, @Nullable ApplicationInfo applicationInfo) {
            mVersionCode = versionCode;
            mApplicationInfo = applicationInfo;
        }
    }

    @VisibleForTesting
    static final class SystemFeatureSnapshot {
        final boolean mHasAutomotive;
        final boolean mHasPc;
        final boolean mHasHingeAngle;
        final boolean mHasXr;
        final int mVulkanDeqpLevel;

        SystemFeatureSnapshot(FeatureInfo[] features) {
            boolean hasAutomotive = false;
            boolean hasPc = false;
            boolean hasHingeAngle = false;
            boolean hasXr = false;
            boolean hasVulkanDeqpLevel = false;
            int vulkanDeqpLevel = 0;

            for (FeatureInfo feature : features) {
                if (feature == null || feature.name == null) {
                    continue;
                }
                String name = feature.name;
                if (PackageManager.FEATURE_AUTOMOTIVE.equals(name)) {
                    hasAutomotive = true;
                } else if (PackageManager.FEATURE_PC.equals(name)) { // nocheck
                    hasPc = true;
                } else if (PackageManager.FEATURE_SENSOR_HINGE_ANGLE.equals(name)) {
                    hasHingeAngle = true;
                } else if (XR_OPENXR_FEATURE_NAME.equals(name)) {
                    hasXr = true;
                } else if (!hasVulkanDeqpLevel
                        && PackageManager.FEATURE_VULKAN_DEQP_LEVEL.equals(name)) {
                    vulkanDeqpLevel = feature.version;
                    hasVulkanDeqpLevel = true;
                }
            }

            mHasAutomotive = hasAutomotive;
            mHasPc = hasPc;
            mHasHingeAngle = hasHingeAngle;
            mHasXr = hasXr;
            mVulkanDeqpLevel = vulkanDeqpLevel;
        }
    }

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
                        /* isTV= */ info.isTv,
                        /* isAutomotive= */ info.isAutomotive,
                        /* isFoldable= */ (sIsFoldableForTesting != null)
                                ? sIsFoldableForTesting
                                : info.isFoldable,
                        /* isDesktop= */ (sIsDesktopForTesting != null)
                                ? sIsDesktopForTesting
                                : info.isDesktop,
                        /* vulkanDeqpLevel= */ info.vulkanDeqpLevel,
                        /* isXr= */ (sIsXrForTesting != null) ? sIsXrForTesting : info.isXr,
                        /* wasLaunchedOnLargeDisplay= */ info.wasLaunchedOnLargeDisplay);
        // Child processes receive GMS through AIDL. The browser's early capability snapshot leaves
        // this null so that initializing the other fields does not trigger the package query.
        if (info.gmsVersionCode != null) {
            DeviceInfoJni.get().setGmsVersionCode(info.gmsVersionCode);
        }
    }

    public static IDeviceInfo getAidlInfo() {
        IDeviceInfo info = getInstance().mIDeviceInfo;
        // Native-only child processes cannot query Java, so materialize the lazy value before
        // parceling this snapshot.
        info.gmsVersionCode = getGmsVersionCode();
        return info;
    }

    public static String getGmsVersionCode() {
        synchronized (GMS_INFO_LOCK) {
            return sGmsVersionCodeForTesting != null
                    ? sGmsVersionCodeForTesting
                    : getGmsInfoLocked().mVersionCode;
        }
    }

    @CalledByNative
    private static @JniType("std::string") String getGmsVersionCodeForNative() {
        sIsGmsVersionNativeLoaded = true;
        return getGmsVersionCode();
    }

    public static @Nullable ApplicationInfo getGmsAppInfo() {
        synchronized (GMS_INFO_LOCK) {
            return getGmsInfoLocked().mApplicationInfo;
        }
    }

    @CalledByNativeForTesting
    public static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        synchronized (GMS_INFO_LOCK) {
            sGmsVersionCodeForTesting = gmsVersionCode;
        }
        ResettersForTesting.register(
                () -> {
                    synchronized (GMS_INFO_LOCK) {
                        sGmsVersionCodeForTesting = null;
                    }
                });
        if (sIsNativeLoaded || sIsGmsVersionNativeLoaded) {
            DeviceInfoJni.get().setGmsVersionCode(gmsVersionCode);
        }
    }

    public static void setIsAutomotiveForTesting(boolean isAutomotive) {
        sIsAutomotiveForTesting = isAutomotive;
        ResettersForTesting.register(() -> sIsAutomotiveForTesting = null);
        if (isAutomotive) {
            setExclusiveFormFactorForTesting(FormFactor.AUTOMOTIVE);
        }
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
    }

    public static void setIsTVForTesting(boolean isTV) {
        sIsTVForTesting = isTV;
        ResettersForTesting.register(() -> sIsTVForTesting = null);
        if (isTV) {
            setExclusiveFormFactorForTesting(FormFactor.TV);
        }
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

    /**
     * Checks whether the current device is a foldable device.
     *
     * <p>A device is considered foldable if it both declares the {@code
     * PackageManager.FEATURE_SENSOR_HINGE_ANGLE} system feature and exposes a real {@code
     * Sensor.TYPE_HINGE_ANGLE} sensor. The sensor is required because some system images (notably
     * emulators) declare the feature statically even when no hinge exists.
     *
     * <p><b>Limitation:</b> The hinge angle sensor was officially introduced in Android 11 (API
     * level 30), so early foldable devices that launched on Android 9 or 10 (such as the original
     * Samsung Galaxy Fold, Z Fold2, and Z Flip) use proprietary implementations instead of the
     * standard AOSP hinge sensor. Consequently, this method will incorrectly return {@code false}
     * for those specific legacy devices.
     *
     * @return {@code true} if the device is recognized by the OS as having a hinge angle sensor,
     *     {@code false} otherwise (including on legacy Samsung foldables).
     */
    public static boolean isFoldable() {
        return (sIsFoldableForTesting != null)
                ? sIsFoldableForTesting
                : getInstance().mIDeviceInfo.isFoldable;
    }

    public static boolean isDesktop() {
        return (sIsDesktopForTesting != null)
                ? sIsDesktopForTesting
                : getInstance().mIDeviceInfo.isDesktop;
    }

    public static int getVulkanDeqpLevel() {
        return getInstance().mIDeviceInfo.vulkanDeqpLevel;
    }

    public static boolean isXr() {
        return (sIsXrForTesting != null) ? sIsXrForTesting : getInstance().mIDeviceInfo.isXr;
    }

    @CalledByNative
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
    public static @JniType("std::string") @Nullable String getDeviceName() {
        return Settings.Global.getString(
                ContextUtils.getApplicationContext().getContentResolver(), "device_name");
    }

    public static boolean isInitializedForTesting() {
        return sInitialized;
    }

    static boolean isGmsInfoInitializedForTesting() {
        synchronized (GMS_INFO_LOCK) {
            return sGmsInfo != null;
        }
    }

    static void resetGmsInfoForTesting() {
        synchronized (GMS_INFO_LOCK) {
            sGmsInfo = null;
            sGmsVersionCodeForTesting = null;
        }
        sIsGmsVersionNativeLoaded = false;
    }

    @CalledByNativeForTesting
    public static void setIsXrForTesting(boolean value) {
        sIsXrForTesting = value;
        ResettersForTesting.register(() -> sIsXrForTesting = null);
        if (value) {
            setExclusiveFormFactorForTesting(FormFactor.XR);
        }
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

    @CalledByNativeForTesting
    public static void setIsDesktopForTesting(boolean isDesktop) {
        sIsDesktopForTesting = isDesktop;
        ResettersForTesting.register(() -> sIsDesktopForTesting = null);
        if (isDesktop) {
            setExclusiveFormFactorForTesting(FormFactor.DESKTOP);
        }
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
    }

    @CalledByNativeForTesting
    public static void resetIsDesktopForTesting() {
        sIsDesktopForTesting = null;
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
    }

    @CalledByNativeForTesting
    public static void setIsFoldableForTesting(boolean value) {
        sIsFoldableForTesting = value;
        ResettersForTesting.register(() -> sIsFoldableForTesting = null);
        if (sIsNativeLoaded) {
            sendToNative(getInstance().mIDeviceInfo);
        }
    }

    @CalledByNativeForTesting
    public static void resetIsFoldableForTesting() {
        sIsFoldableForTesting = null;
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

    @GuardedBy("GMS_INFO_LOCK")
    private static GmsInfo getGmsInfoLocked() {
        if (sGmsInfo == null) {
            PackageInfo packageInfo = PackageUtils.getPackageInfo("com.google.android.gms", 0);
            String versionCode = "gms versionCode not available.";
            ApplicationInfo applicationInfo = null;
            if (packageInfo != null) {
                versionCode = String.valueOf(packageVersionCode(packageInfo));
                applicationInfo = packageInfo.applicationInfo;
            }
            sGmsInfo = new GmsInfo(versionCode, applicationInfo);
        }
        return sGmsInfo;
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

    @VisibleForTesting
    static @Nullable SystemFeatureSnapshot getSystemFeatureSnapshot(PackageManager pm) {
        try {
            FeatureInfo[] features = pm.getSystemAvailableFeatures();
            return features == null ? null : new SystemFeatureSnapshot(features);
        } catch (SecurityException e) {
            Log.e(TAG, "Unable to query available system features", e);
            return null;
        }
    }

    /**
     * Returns whether the device actually has a hinge angle sensor. Devices with a real hinge are
     * required to expose a {@link Sensor#TYPE_HINGE_ANGLE} sensor, but some system images (notably
     * emulators) declare {@code PackageManager.FEATURE_SENSOR_HINGE_ANGLE} without having one.
     */
    private static boolean hasHingeAngleSensor(Context context) {
        // TYPE_HINGE_ANGLE was added in Android 11 (API 30).
        if (Build.VERSION.SDK_INT < VERSION_CODES.R) return false;

        var sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        return sensorManager != null
                && sensorManager.getDefaultSensor(Sensor.TYPE_HINGE_ANGLE) != null;
    }

    private DeviceInfo() {
        mIDeviceInfo = new IDeviceInfo();
        sInitialized = true;
        Context appContext = ContextUtils.getApplicationContext();
        PackageManager pm = appContext.getPackageManager();
        // See https://developer.android.com/training/tv/start/hardware.html#runtime-check.
        int uiMode = appContext.getResources().getConfiguration().uiMode;
        mIDeviceInfo.isTv =
                (uiMode & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_TELEVISION;
        if (sIsTVForTesting != null) {
            mIDeviceInfo.isTv = sIsTVForTesting;
        }

        // On Android T+, reuse the feature list that is already needed for the Vulkan deQP level
        // rather than making separate PackageManager calls for each form-factor feature.
        SystemFeatureSnapshot systemFeatures =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                        ? getSystemFeatureSnapshot(pm)
                        : null;

        boolean isAutomotive;
        if (systemFeatures != null) {
            isAutomotive = systemFeatures.mHasAutomotive;
        } else {
            try {
                isAutomotive = pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
            } catch (SecurityException e) {
                Log.e(TAG, "Unable to query for Automotive system feature", e);

                // `hasSystemFeature` can possibly throw an exception on modified instances of
                // Android. In this case, assume the device is not a car since automotive vehicles
                // should not have such a modification.
                isAutomotive = false;
            }
        }
        mIDeviceInfo.isAutomotive = isAutomotive;

        if (sIsAutomotiveForTesting != null) {
            mIDeviceInfo.isAutomotive = sIsAutomotiveForTesting;
        }

        mIDeviceInfo.isDesktop =
                (sIsDesktopForTesting != null)
                        ? sIsDesktopForTesting
                        : (BuildConfig.IS_DESKTOP_ANDROID
                                        && (systemFeatures != null
                                                ? systemFeatures.mHasPc
                                                : pm.hasSystemFeature(
                                                        PackageManager.FEATURE_PC))) // nocheck
                                || CommandLine.getInstance()
                                        .hasSwitch(BaseSwitches.FORCE_DESKTOP_ANDROID);

        // Detect whether device is foldable. The system feature alone is not sufficient: emulator
        // system images declare FEATURE_SENSOR_HINGE_ANGLE in
        // /vendor/etc/permissions/handheld_core_hardware.xml even for AVDs configured without a
        // hinge (hw.sensor.hinge = no), so phone-sized emulators look like foldables. Devices with
        // a real hinge must also expose a TYPE_HINGE_ANGLE sensor (CDD 7.3.12), so require the
        // sensor itself. The sensor lookup is short-circuited by the feature check, so it is only
        // performed on the few devices that declare the feature. See crbug.com/555859584.
        mIDeviceInfo.isFoldable =
                !mIDeviceInfo.isDesktop
                        && Build.VERSION.SDK_INT >= VERSION_CODES.R
                        && (systemFeatures != null
                                ? systemFeatures.mHasHingeAngle
                                : pm.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE))
                        && hasHingeAngleSensor(appContext);
        if (sIsFoldableForTesting != null) {
            mIDeviceInfo.isFoldable = sIsFoldableForTesting;
        }

        mIDeviceInfo.vulkanDeqpLevel = systemFeatures == null ? 0 : systemFeatures.mVulkanDeqpLevel;

        mIDeviceInfo.wasLaunchedOnLargeDisplay =
                getDeviceWidthInDp() >= LARGE_DISPLAY_MIN_SCREEN_WIDTH_600_DP;

        mIDeviceInfo.isXr =
                systemFeatures != null
                        ? systemFeatures.mHasXr
                        : pm.hasSystemFeature(XR_OPENXR_FEATURE_NAME);
        if (sIsXrForTesting != null) {
            mIDeviceInfo.isXr = sIsXrForTesting;
        }
    }

    private static void setExclusiveFormFactorForTesting(@FormFactor int activeFormFactor) {
        if (activeFormFactor != FormFactor.TV) {
            sIsTVForTesting = false;
            ResettersForTesting.register(() -> sIsTVForTesting = null);
        }
        if (activeFormFactor != FormFactor.AUTOMOTIVE) {
            sIsAutomotiveForTesting = false;
            ResettersForTesting.register(() -> sIsAutomotiveForTesting = null);
        }
        if (activeFormFactor != FormFactor.DESKTOP) {
            sIsDesktopForTesting = false;
            ResettersForTesting.register(() -> sIsDesktopForTesting = null);
        }
        if (activeFormFactor != FormFactor.XR) {
            sIsXrForTesting = false;
            ResettersForTesting.register(() -> sIsXrForTesting = null);
        }
    }

    @NativeMethods
    interface Natives {
        void fillFields(
                boolean isTV,
                boolean isAutomotive,
                boolean isFoldable,
                boolean isDesktop,
                int vulkanDeqpLevel,
                boolean isXr,
                boolean wasLaunchedOnLargeDisplay);

        void setGmsVersionCode(@JniType("std::string") String gmsVersionCode);
    }
}
