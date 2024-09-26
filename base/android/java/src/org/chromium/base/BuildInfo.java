// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static android.content.Context.UI_MODE_SERVICE;

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Process;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JniType;

import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import javax.annotation.concurrent.GuardedBy;

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

    /**
     * The package name of the host app which has loaded WebView, retrieved from the application
     * context. In the context of the SDK Runtime, the package name of the app that owns this
     * particular instance of the SDK Runtime will also be included.
     * e.g. com.google.android.sdksandbox:com:com.example.myappwithads
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
    private String mHostSigningCertSha256;

    /** The versionCode of Play Services. Can be overridden in tests. */
    private String mGmsVersionCode;

    private Object mCertLock = new Object();

    private static class Holder {
        private static final BuildInfo INSTANCE = new BuildInfo();
    }

    @CalledByNative
    private static String[] getAll() {
        return BuildInfo.getInstance().getAllProperties();
    }

    @CalledByNative
    private static String lazyGetHostSigningCertSha256() {
        return BuildInfo.getInstance().getHostSigningCertSha256();
    }

    @CalledByNativeForTesting
    private static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        getInstance().mGmsVersionCode = gmsVersionCode;
    }

    /** Returns a serialized string array of all properties of this class. */
    private String[] getAllProperties() {
        // This implementation needs to be kept in sync with the native BuildInfo constructor.
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
            mGmsVersionCode,
            installerPackageName,
            abiString,
            customThemes,
            resourcesVersion,
            String.valueOf(
                    ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion),
            isDebugAndroid() ? "1" : "0",
            isTV ? "1" : "0",
            Build.VERSION.INCREMENTAL,
            Build.HARDWARE,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU ? "1" : "0",
            isAutomotive ? "1" : "0",
            Build.VERSION.SDK_INT >= VERSION_CODES.UPSIDE_DOWN_CAKE ? "1" : "0",
            targetsAtLeastU() ? "1" : "0",
            Build.VERSION.CODENAME,
            String.valueOf(vulkanDeqpLevel),
            isFoldable ? "1" : "0",
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? Build.SOC_MANUFACTURER : "",
            isDebugApp() ? "1" : "0",
            isDesktop ? "1" : "0",
        };
    }

    private static String nullToEmpty(CharSequence seq) {
        return seq == null ? "" : seq.toString();
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
        // Some tests mock out things BuildInfo is based on, so disable caching in tests to ensure
        // such mocking is not defeated by caching.
        if (BuildConfig.IS_FOR_TEST) {
            return new BuildInfo();
        }
        return Holder.INSTANCE;
    }

    /** The versionCode of Play Services. */
    public String getGmsVersionCode() {
        return mGmsVersionCode;
    }

    private BuildInfo() {
        sInitialized = true;
        Context appContext = ContextUtils.getApplicationContext();
        String appContextPackageName = appContext.getPackageName();
        PackageManager pm = appContext.getPackageManager();

        String providedHostPackageName = null;
        String providedHostPackageLabel = null;
        String providedPackageName = null;
        String providedPackageVersionName = null;
        Long providedHostVersionCode = null;

        // The child processes are running in an isolated process so they can't grab a lot of
        // package information in the same way that we normally would retrieve them. To get around
        // this, we feed the information as command line switches.
        if (CommandLine.isInitialized()) {
            CommandLine commandLine = CommandLine.getInstance();
            providedHostPackageName = commandLine.getSwitchValue(BaseSwitches.HOST_PACKAGE_NAME);
            providedHostPackageLabel = commandLine.getSwitchValue(BaseSwitches.HOST_PACKAGE_LABEL);
            providedPackageName = commandLine.getSwitchValue(BaseSwitches.PACKAGE_NAME);
            providedPackageVersionName =
                    commandLine.getSwitchValue(BaseSwitches.PACKAGE_VERSION_NAME);

            if (commandLine.hasSwitch(BaseSwitches.HOST_VERSION_CODE)) {
                providedHostVersionCode =
                        Long.parseLong(commandLine.getSwitchValue(BaseSwitches.HOST_VERSION_CODE));
            }
        }

        boolean hostInformationProvided =
                providedHostPackageName != null
                        && providedHostPackageLabel != null
                        && providedHostVersionCode != null
                        && providedPackageName != null
                        && providedPackageVersionName != null;

        // We want to retrieve the original package installed to verify to host package name.
        // In the case of the SDK Runtime, we would like to retrieve the package name loading the
        // SDK.
        String appInstalledPackageName = appContextPackageName;

        if (hostInformationProvided) {
            hostPackageName = providedHostPackageName;
            hostPackageLabel = providedHostPackageLabel;
            hostVersionCode = providedHostVersionCode;
            versionName = providedPackageVersionName;
            packageName = providedPackageName;

            sBrowserApplicationInfo = appContext.getApplicationInfo();
        } else {
            // The SDK Qualified package name will retrieve the same information as
            // appInstalledPackageName but prefix it with the SDK Sandbox process so that we can
            // tell SDK Runtime data apart from regular data in our logs and metrics.
            String sdkQualifiedName = appInstalledPackageName;

            // TODO(bewise): There isn't currently an official API to grab the host package name
            // with the SDK Runtime. We can work around this because SDKs loaded in the SDK
            // Runtime have the host UID + 10000. This should be updated if a public API comes
            // along that we can use.
            // You can see more about this in the Android source:
            // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/os/Process.java;l=292;drc=47fffdd53115a9af1820e3f89d8108745be4b55d
            if (ContextUtils.isSdkSandboxProcess()) {
                final int hostId = Process.myUid() - 10000;
                final String[] packageNames = pm.getPackagesForUid(hostId);

                if (packageNames.length > 0) {
                    // We could end up with more than one package name if the app used a
                    // sharedUserId but these are deprecated so this is a safe bet to rely on the
                    // first package name.
                    appInstalledPackageName = packageNames[0];
                    sdkQualifiedName += ":" + appInstalledPackageName;
                }
            }

            PackageInfo pi = PackageUtils.getPackageInfo(appInstalledPackageName, 0);
            hostPackageName = sdkQualifiedName;
            hostPackageLabel = nullToEmpty(pm.getApplicationLabel(pi.applicationInfo));
            hostVersionCode = packageVersionCode(pi);

            if (sBrowserPackageInfo != null) {
                packageName = sBrowserPackageInfo.packageName;
                versionName = nullToEmpty(sBrowserPackageInfo.versionName);
                sBrowserApplicationInfo = sBrowserPackageInfo.applicationInfo;
                sBrowserPackageInfo = null;
            } else {
                packageName = appContextPackageName;
                versionName = nullToEmpty(pi.versionName);
                sBrowserApplicationInfo = appContext.getApplicationInfo();
            }
        }

        installerPackageName = nullToEmpty(pm.getInstallerPackageName(appInstalledPackageName));

        PackageInfo gmsPackageInfo = PackageUtils.getPackageInfo("com.google.android.gms", 0);
        mGmsVersionCode =
                gmsPackageInfo != null
                        ? String.valueOf(packageVersionCode(gmsPackageInfo))
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
                currentResourcesVersion =
                        ContextUtils.getApplicationContext()
                                .getString(BuildConfig.R_STRING_PRODUCT_VERSION);
            } catch (Exception e) {
                currentResourcesVersion = "Not found";
            }
        }
        resourcesVersion = currentResourcesVersion;

        abiString = TextUtils.join(", ", Build.SUPPORTED_ABIS);

        // The value is truncated, as this is used for crash and UMA reporting.
        androidBuildFingerprint =
                Build.FINGERPRINT.substring(
                        0, Math.min(Build.FINGERPRINT.length(), MAX_FINGERPRINT_LENGTH));

        // See https://developer.android.com/training/tv/start/hardware.html#runtime-check.
        UiModeManager uiModeManager = (UiModeManager) appContext.getSystemService(UI_MODE_SERVICE);
        isTV =
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
        this.isAutomotive = isAutomotive;

        // Detect whether device is foldable.
        this.isFoldable =
                Build.VERSION.SDK_INT >= VERSION_CODES.R
                        && pm.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE);

        this.isDesktop = pm.hasSystemFeature(PackageManager.FEATURE_PC);

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
        vulkanDeqpLevel = vulkanLevel;
    }

    /**
     * Check if this is a debuggable build of Android. This is a rough approximation of the hidden
     * API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    /*
     * Check if the app is declared debuggable in its manifest.
     * In WebView, this refers to the host app.
     */
    public static boolean isDebugApp() {
        int appFlags = ContextUtils.getApplicationContext().getApplicationInfo().flags;
        return (appFlags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    /**
     * Check if this is either a debuggable build of Android or of the host app.
     * Use this to enable developer-only features.
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
     * Checks if the application targets pre-release SDK U.
     * This must be manually maintained as the SDK goes through finalization!
     * Avoid depending on this if possible; this is only intended for WebView.
     */
    public static boolean targetsAtLeastU() {
        int target = ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;

        // Logic for pre-API-finalization:
        // return BuildCompat.isAtLeastU() && target == Build.VERSION_CODES.CUR_DEVELOPMENT;

        // Logic for after API finalization but before public SDK release has to just hardcode the
        // appropriate SDK integer. This will include Android builds with the finalized SDK, and
        // also pre-API-finalization builds (because CUR_DEVELOPMENT == 10000).
        // return target >= 34;

        // Now that the public SDK is upstreamed we can use the defined constant. All users of this
        // should now just inline this check themselves.
        return target >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
    }

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

    private Signature[] getPackageSignatures(PackageInfo pi) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (pi.signingInfo == null) {
                return null;
            }
            return pi.signingInfo.getSigningCertificateHistory();
        }

        return pi.signatures;
    }
}
