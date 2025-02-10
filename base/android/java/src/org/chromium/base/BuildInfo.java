// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Process;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import javax.annotation.concurrent.GuardedBy;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo} information. This is
 * primarily of use for accessing package information from native code.
 *
 * <p>NOTE: This class is deprecated, You can find the appropriate utilities that lived here in
 * AndroidInfo, ApkInfo or DeviceInfo.
 */
@NullMarked
@Deprecated
public class BuildInfo {
    private static final String TAG = "BuildInfo";

    /**
     * The package name of the host app which has loaded WebView, retrieved from the application
     * context. In the context of the SDK Runtime, the package name of the app that owns this
     * particular instance of the SDK Runtime will also be included. e.g.
     * com.google.android.sdksandbox:com:com.example.myappwithads
     */
    public final String hostPackageName;

    /**
     * The application name (e.g. "Chrome"). For WebView, this is name of the embedding app.
     * In the context of the SDK Runtime, this is the name of the app that owns this particular
     * instance of the SDK Runtime.
     */
    public final String hostPackageLabel;

    /**
     * By default: same as versionCode. For WebView: versionCode of the embedding app. In the
     * context of the SDK Runtime, this is the versionCode of the app that owns this particular
     * instance of the SDK Runtime.
     */
    public final long hostVersionCode;

    /**
     * The packageName of Chrome/WebView. Use application context for host app packageName. Same as
     * the host information within any child process.
     */
    public final String packageName;

    /** The versionCode of the apk. */
    public final long versionCode = BuildConfig.VERSION_CODE;

    /** The versionName of Chrome/WebView. Use application context for host app versionName. */
    public final String versionName;

    /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
    public final String installerPackageName;

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

    /** Whether we're running on an Android Foldable OS device or not. */
    public final boolean isFoldable;

    /** Whether we're running on an Android Desktop OS device or not. */
    public final boolean isDesktop;

    /**
     * version of the FEATURE_VULKAN_DEQP_LEVEL, if available. Queried only on Android T or above
     */
    public final int vulkanDeqpLevel;

    /**
     * The SHA256 of the public certificate used to sign the host application. This will default to
     * an empty string if we were unable to retrieve it.
     */
    @GuardedBy("mCertLock")
    private @Nullable String mHostSigningCertSha256;

    private final Object mCertLock = new Object();

    private static class Holder {
        private static final BuildInfo INSTANCE = new BuildInfo();
    }

    @CalledByNative
    private static @JniType("std::string") String lazyGetHostSigningCertSha256() {
        return BuildInfo.getInstance().getHostSigningCertSha256();
    }

    public static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        DeviceInfo.setGmsVersionCodeForTest(gmsVersionCode);
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
     * @param packageInfo Package for Chrome/WebView (as opposed to host app).
     */
    public static void setBrowserPackageInfo(PackageInfo packageInfo) {
        ApkInfo.setBrowserPackageInfo(packageInfo);
    }

    /**
     * @return ApplicationInfo for Chrome/WebView (as opposed to host app).
     */
    public ApplicationInfo getBrowserApplicationInfo() {
        return ApkInfo.getInstance().getBrowserApplicationInfo();
    }

    public static BuildInfo getInstance() {
        // Some tests mock out things BuildInfo is based on, so disable caching in tests to ensure
        // such mocking is not defeated by caching.
        if (BuildConfig.IS_FOR_TEST) {
            return new BuildInfo();
        }
        return Holder.INSTANCE;
    }

    /** The versionCode of Play Services. */
    public String getGmsVersionCode() {
        return DeviceInfo.getGmsVersionCode();
    }

    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/98
    private BuildInfo() {
        hostPackageName = ApkInfo.getHostPackageName();
        hostPackageLabel = ApkInfo.getHostPackageLabel();
        hostVersionCode = ApkInfo.getHostVersionCode();
        packageName = ApkInfo.getPackageName();
        versionName = ApkInfo.getPackageVersionName();
        installerPackageName = ApkInfo.getInstallerPackageName();
        abiString = AndroidInfo.getAndroidSupportedAbis();
        androidBuildFingerprint = AndroidInfo.getAndroidBuildFingerprint();
        customThemes = DeviceInfo.getCustomThemes();
        resourcesVersion = ApkInfo.getResourcesVersion();
        isTV = DeviceInfo.isTV();
        isAutomotive = DeviceInfo.isAutomotive();
        isFoldable = DeviceInfo.isFoldable();
        isDesktop = DeviceInfo.isDesktop();
        vulkanDeqpLevel = DeviceInfo.getVulkanDeqpLevel();
    }

    /**
     * Check if this is a debuggable build of Android. This is a rough approximation of the hidden
     * API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return AndroidInfo.isDebugAndroid();
    }

    /*
     * Check if the app is declared debuggable in its manifest.
     * In WebView, this refers to the host app.
     */
    public static boolean isDebugApp() {
        return ApkInfo.isDebugApp();
    }

    /**
     * Check if this is either a debuggable build of Android or of the host app. Use this to enable
     * developer-only features.
     */
    public static boolean isDebugAndroidOrApp() {
        return isDebugAndroid() || isDebugApp();
    }

    /**
     * Checks if the application targets the T SDK or later.
     * @deprecated Chrome callers should just remove this test - Chrome targets T or later now.
     * WebView callers should just inline the logic below to check the target level of the embedding
     * App when necessary.
     */
    @Deprecated
    public static boolean targetsAtLeastT() {
        int target = ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;

        // Now that the public SDK is upstreamed we can use the defined constant.
        return target >= VERSION_CODES.TIRAMISU;
    }

    /**
     * Checks if the application targets pre-release SDK U. This must be manually maintained as the
     * SDK goes through finalization! Avoid depending on this if possible; this is only intended for
     * WebView.
     */
    public static boolean targetsAtLeastU() {
        return ApkInfo.targetsAtLeastU();
    }

    @NullUnmarked
    public String getHostSigningCertSha256() {
        // We currently only make use of this certificate for calls from the storage access API
        // within WebView. So we rather lazy load this value to avoid impacting app startup.
        synchronized (mCertLock) {
            if (mHostSigningCertSha256 == null) {
                String certificate = "";

                PackageInfo pi =
                        PackageUtils.getPackageInfo(
                                ContextUtils.getApplicationContext().getPackageName(),
                                getPackageInfoFlags());

                Signature[] signatures = getPackageSignatures(pi);
                if (signatures != null) {
                    try {
                        MessageDigest messageDigest = MessageDigest.getInstance("SHA-256");
                        // The current signing certificate is always the last one in the list.
                        byte[] digest =
                                messageDigest.digest(
                                        signatures[signatures.length - 1].toByteArray());
                        certificate = PackageUtils.byteArrayToHexString(digest);
                    } catch (NoSuchAlgorithmException e) {
                        Log.w(TAG, "Unable to hash host app signature", e);
                    }
                }

                mHostSigningCertSha256 = certificate;
            }

            return mHostSigningCertSha256;
        }
    }

    private int getPackageInfoFlags() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return PackageManager.GET_SIGNING_CERTIFICATES;
        }
        return PackageManager.GET_SIGNATURES;
    }

    private Signature @Nullable [] getPackageSignatures(PackageInfo pi) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (pi.signingInfo == null) {
                return null;
            }
            return pi.signingInfo.getSigningCertificateHistory();
        }

        return pi.signatures;
    }
}
