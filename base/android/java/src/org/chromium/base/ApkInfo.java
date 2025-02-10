// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Process;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
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
public final class ApkInfo {
    private static final String TAG = "ApkInfo";

    private static boolean sInitialized;
    private static @Nullable PackageInfo sBrowserPackageInfo;

    private ApplicationInfo mBrowserApplicationInfo;

    /**
     * The package name of the host app which has loaded WebView, retrieved from the application
     * context. In the context of the SDK Runtime, the package name of the app that owns this
     * particular instance of the SDK Runtime will also be included. e.g.
     * com.google.android.sdksandbox:com:com.example.myappwithads
     */
    private final String mHostPackageName;

    /**
     * The application name (e.g. "Chrome"). For WebView, this is name of the embedding app. In the
     * context of the SDK Runtime, this is the name of the app that owns this particular instance of
     * the SDK Runtime.
     */
    private final String mHostPackageLabel;

    /**
     * By default: same as versionCode. For WebView: versionCode of the embedding app. In the
     * context of the SDK Runtime, this is the versionCode of the app that owns this particular
     * instance of the SDK Runtime.
     */
    private final long mHostVersionCode;

    /** The versionName of Chrome/WebView. Use application context for host app versionName. */
    private final String mVersionName;

    /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
    private final String mInstallerPackageName;

    /**
     * The packageName of Chrome/WebView. Use application context for host app packageName. Same as
     * the host information within any child process.
     */
    private final String mPackageName;

    /** Product version as stored in Android resources. */
    private final String mResourcesVersion;

    private static volatile ApkInfo sInstance;

    private static final Object CREATION_LOCK = new Object();

    // Called by the native code to retrieve field values. There is no easy way to
    // return several fields from Java to native, so instead this calls back into
    // native, passing the fields as parameters to a native function.
    // The native code expects native `fillFields()` to be called inline from this
    // function.
    @CalledByNative
    private static void nativeReadyForFields() {
        ApkInfo instance = getInstance();
        ApkInfoJni.get()
                .fillFields(
                        /* hostPackageName= */ instance.mHostPackageName,
                        /* hostVersionCode= */ String.valueOf(instance.mHostVersionCode),
                        /* hostPackageLabel= */ instance.mHostPackageLabel,
                        /* packageVersionCode= */ String.valueOf(BuildConfig.VERSION_CODE),
                        /* packageVersionName= */ instance.mVersionName,
                        /* packageName= */ instance.mPackageName,
                        /* resourcesVersion= */ instance.mResourcesVersion,
                        /* installerPackageName= */ instance.mInstallerPackageName,
                        /* isDebugApp= */ isDebugApp(),
                        /* targetsAtleastU= */ targetsAtLeastU(),
                        /* targetSdkVersion= */ ContextUtils.getApplicationContext()
                                .getApplicationInfo()
                                .targetSdkVersion);
    }

    public static String getHostPackageName() {
        return getInstance().mHostPackageName;
    }

    public static long getHostVersionCode() {
        return getInstance().mHostVersionCode;
    }

    public static String getHostPackageLabel() {
        return getInstance().mHostPackageLabel;
    }

    public static String getPackageName() {
        return getInstance().mPackageName;
    }

    public static String getPackageVersionCode() {
        return String.valueOf(BuildConfig.VERSION_CODE);
    }

    public static String getPackageVersionName() {
        return getInstance().mVersionName;
    }

    public static String getInstallerPackageName() {
        return getInstance().mInstallerPackageName;
    }

    public static String getResourcesVersion() {
        return getInstance().mResourcesVersion;
    }

    /**
     * Checks if the application targets pre-release SDK U. This must be manually maintained as the
     * SDK goes through finalization! Avoid depending on this if possible; this is only intended for
     * WebView.
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

        if (hostInformationProvided) {
            mHostPackageName = providedHostPackageName;
            mHostPackageLabel = providedHostPackageLabel;
            mHostVersionCode = providedHostVersionCode;
            mVersionName = providedPackageVersionName;
            mPackageName = providedPackageName;

            mBrowserApplicationInfo = appContext.getApplicationInfo();
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

            ApplicationInfo appInfo = appContext.getApplicationInfo();
            mHostPackageName = sdkQualifiedName;
            mHostPackageLabel = nullToEmpty(pm.getApplicationLabel(appInfo));

            if (sBrowserPackageInfo != null) {
                PackageInfo pi =
                        assumeNonNull(PackageUtils.getPackageInfo(appInstalledPackageName, 0));
                mHostVersionCode = PackageUtils.packageVersionCode(pi);
                mPackageName = sBrowserPackageInfo.packageName;
                mVersionName = nullToEmpty(sBrowserPackageInfo.versionName);
                mBrowserApplicationInfo = sBrowserPackageInfo.applicationInfo;
                sBrowserPackageInfo = null;
            } else {
                mPackageName = appContextPackageName;
                mHostVersionCode = BuildConfig.VERSION_CODE;
                mVersionName = VersionInfo.getProductVersion();
                mBrowserApplicationInfo = appInfo;
            }
        }

        mInstallerPackageName = nullToEmpty(pm.getInstallerPackageName(appInstalledPackageName));

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
        mResourcesVersion = currentResourcesVersion;
    }

    /*
     * Check if the app is declared debuggable in its manifest.
     * In WebView, this refers to the host app.
     */
    public static boolean isDebugApp() {
        int appFlags = ContextUtils.getApplicationContext().getApplicationInfo().flags;
        return (appFlags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    @NativeMethods
    interface Natives {
        void fillFields(
                String hostPackageName,
                String hostVersionCode,
                String hostPackageLabel,
                String packageVersionCode,
                String packageVersionName,
                String packageName,
                String resourcesVersion,
                String installerPackageName,
                boolean isDebugApp,
                boolean targetsAtleastU,
                int targetSdkVersion);
    }
}
