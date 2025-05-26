// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Process;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * AndroidInfo is a utility class to access the APK-related information.
 *
 * <p>Warning: Adding any new field here will means that all products of Chromium that initializes
 * this class will have to pay the cost of that new field, this can be costy especially when the
 * field added is not useful to the entire codebase. Over time this can lead to performance
 * regressions.
 */
@JNINamespace("base::android::apk_info")
@NullMarked
public final class ApkInfo {
    private static final String TAG = "ApkInfo";

    private static boolean sInitialized;
    private static @Nullable PackageInfo sBrowserPackageInfo;

    private final ApplicationInfo mBrowserApplicationInfo;
    private final IApkInfo mIApkInfo;

    private static volatile @Nullable ApkInfo sInstance;

    private static final Object CREATION_LOCK = new Object();

    // Called by the native code to retrieve field values. There is no easy way to
    // return several fields from Java to native, so instead this calls back into
    // native, passing the fields as parameters to a native function.
    // The native code expects native `fillFields()` to be called inline from this
    // function.
    @CalledByNative
    private static void nativeReadyForFields() {
        sendToNative(getInstance().mIApkInfo);
    }

    public static void sendToNative(IApkInfo info) {
        ApkInfoJni.get()
                .fillFields(
                        /* hostPackageName= */ info.hostPackageName,
                        /* hostVersionCode= */ info.hostVersionCode,
                        /* hostPackageLabel= */ info.hostPackageLabel,
                        /* packageVersionCode= */ info.packageVersionCode,
                        /* packageVersionName= */ info.packageVersionName,
                        /* packageName= */ info.packageName,
                        /* resourcesVersion= */ info.resourcesVersion,
                        /* installerPackageName= */ info.installerPackageName,
                        /* isDebugApp= */ info.isDebugApp,
                        /* targetSdkVersion= */ info.targetSdkVersion);
    }

    public static IApkInfo getAidlInfo() {
        return getInstance().mIApkInfo;
    }

    public static String getHostPackageName() {
        return getInstance().mIApkInfo.hostPackageName;
    }

    public static String getHostVersionCode() {
        return getInstance().mIApkInfo.hostVersionCode;
    }

    public static String getHostPackageLabel() {
        return getInstance().mIApkInfo.hostPackageLabel;
    }

    public static String getPackageName() {
        return getInstance().mIApkInfo.packageName;
    }

    public static String getPackageVersionCode() {
        return getInstance().mIApkInfo.packageVersionCode;
    }

    public static String getPackageVersionName() {
        return getInstance().mIApkInfo.packageVersionName;
    }

    public static String getInstallerPackageName() {
        return getInstance().mIApkInfo.installerPackageName;
    }

    public static String getResourcesVersion() {
        return getInstance().mIApkInfo.resourcesVersion;
    }

    public static boolean isDebugApp() {
        return getInstance().mIApkInfo.isDebugApp;
    }

    /**
     * Checks if the application targets pre-release SDK B. This must be manually maintained as the
     * SDK goes through finalization.
     */
    public static boolean targetAtLeastB() {
        int target = ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;

        // Logic for pre-API-finalization:
        // return BuildCompat.isAtLeastU() && target == Build.VERSION_CODES.CUR_DEVELOPMENT;

        // Logic for after API finalization but before public SDK release has to just hardcode the
        // appropriate SDK integer. This will include Android builds with the finalized SDK, and
        // also pre-API-finalization builds (because CUR_DEVELOPMENT == 10000).
        return target >= 36;

        // Now that the public SDK is upstreamed we can use the defined constant. All users of this
        // should now just inline this check themselves.
        // return target >= Build.VERSION_CODES.<PLACE_HOLDER>;
    }

    public static boolean isInitializedForTesting() {
        return sInitialized;
    }

    public static ApkInfo getInstance() {
        // Some tests mock out things BuildInfo is based on, so disable caching in tests to ensure
        // such mocking is not defeated by caching.
        if (BuildConfig.IS_FOR_TEST) {
            return new ApkInfo();
        }

        if (sInstance == null) {
            synchronized (CREATION_LOCK) {
                if (sInstance == null) {
                    sInstance = new ApkInfo();
                }
            }
        }
        return sInstance;
    }

    /**
     * @return ApplicationInfo for Chrome/WebView (as opposed to host app).
     */
    public ApplicationInfo getBrowserApplicationInfo() {
        return mBrowserApplicationInfo;
    }

    /**
     * @param packageInfo Package for Chrome/WebView (as opposed to host app).
     */
    public static void setBrowserPackageInfo(PackageInfo packageInfo) {
        assert !sInitialized;
        sBrowserPackageInfo = packageInfo;
    }

    private static String nullToEmpty(@Nullable CharSequence seq) {
        return seq == null ? "" : seq.toString();
    }

    private ApkInfo() {
        sInitialized = true;
        mIApkInfo = new IApkInfo();
        Context appContext = ContextUtils.getApplicationContext();
        String appContextPackageName = appContext.getPackageName();
        PackageManager pm = appContext.getPackageManager();

        String providedHostPackageName = null;
        String providedHostPackageLabel = null;
        String providedPackageName = null;
        String providedPackageVersionName = null;
        Long providedHostVersionCode = null;
        mIApkInfo.packageVersionCode = String.valueOf(BuildConfig.VERSION_CODE);

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

            String flagValue = commandLine.getSwitchValue(BaseSwitches.HOST_VERSION_CODE);

            if (flagValue != null) {
                providedHostVersionCode = Long.parseLong(flagValue);
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
        ApplicationInfo appInfo = appContext.getApplicationInfo();
        mIApkInfo.isDebugApp = (appInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;

        if (hostInformationProvided) {
            mIApkInfo.hostPackageName = assumeNonNull(providedHostPackageName);
            mIApkInfo.hostPackageLabel = assumeNonNull(providedHostPackageLabel);
            mIApkInfo.hostVersionCode = String.valueOf(assumeNonNull(providedHostVersionCode));
            mIApkInfo.packageVersionName = assumeNonNull(providedPackageVersionName);
            mIApkInfo.packageName = assumeNonNull(providedPackageName);
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

                if (packageNames != null && packageNames.length > 0) {
                    // We could end up with more than one package name if the app used a
                    // sharedUserId but these are deprecated so this is a safe bet to rely on the
                    // first package name.
                    appInstalledPackageName = packageNames[0];
                    sdkQualifiedName += ":" + appInstalledPackageName;
                }
            }

            mIApkInfo.hostPackageName = sdkQualifiedName;
            mIApkInfo.hostPackageLabel = nullToEmpty(pm.getApplicationLabel(appInfo));

            if (sBrowserPackageInfo != null) {
                PackageInfo pi =
                        assumeNonNull(PackageUtils.getPackageInfo(appInstalledPackageName, 0));
                mIApkInfo.hostVersionCode = String.valueOf(PackageUtils.packageVersionCode(pi));
                mIApkInfo.packageName = sBrowserPackageInfo.packageName;
                mIApkInfo.packageVersionName = nullToEmpty(sBrowserPackageInfo.versionName);
                appInfo = sBrowserPackageInfo.applicationInfo;
                sBrowserPackageInfo = null;
            } else {
                mIApkInfo.packageName = appContextPackageName;
                mIApkInfo.hostVersionCode = String.valueOf(BuildConfig.VERSION_CODE);
                mIApkInfo.packageVersionName = VersionInfo.getProductVersion();
            }
        }
        assert appInfo != null;
        mBrowserApplicationInfo = appInfo;

        mIApkInfo.installerPackageName =
                nullToEmpty(pm.getInstallerPackageName(appInstalledPackageName));

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
        mIApkInfo.resourcesVersion = currentResourcesVersion;
        mIApkInfo.targetSdkVersion = appInfo.targetSdkVersion;
    }

    @NativeMethods
    interface Natives {
        void fillFields(
                @JniType("std::string") String hostPackageName,
                @JniType("std::string") String hostVersionCode,
                @JniType("std::string") String hostPackageLabel,
                @JniType("std::string") String packageVersionCode,
                @JniType("std::string") String packageVersionName,
                @JniType("std::string") String packageName,
                @JniType("std::string") String resourcesVersion,
                @JniType("std::string") String installerPackageName,
                boolean isDebugApp,
                int targetSdkVersion);
    }
}
