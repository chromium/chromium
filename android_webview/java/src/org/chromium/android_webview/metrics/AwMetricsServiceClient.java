// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.Context;
import android.content.pm.ApplicationInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.base.ApkInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;

/**
 * Determines user consent and app opt-out for metrics. See aw_metrics_service_client.h for more
 * explanation.
 */
@JNINamespace("android_webview")
@NullMarked
public class AwMetricsServiceClient {
    private static final String PLAY_STORE_PACKAGE_NAME = "com.android.vending";
    private static final String METRICS_SUBDIR = ".webview";

    private static @InstallerPackageType @Nullable Integer sInstallerPackageTypeForTesting;

    /**
     * Set user consent settings.
     *
     * @param userConsent user consent via Android Usage & diagnostics settings.
     */
    public static void setConsentSetting(boolean userConsent) {
        ThreadUtils.assertOnUiThread();
        AwMetricsServiceClientJni.get()
                .setHaveMetricsConsent(
                        userConsent, !ManifestMetadataUtil.isAppOptedOutFromMetricsCollection());
    }

    public static void setFastStartupForTesting(boolean fastStartupForTesting) {
        AwMetricsServiceClientJni.get().setFastStartupForTesting(fastStartupForTesting);
    }

    public static void setUploadIntervalForTesting(long uploadIntervalMs) {
        AwMetricsServiceClientJni.get().setUploadIntervalForTesting(uploadIntervalMs);
    }

    /** Sets a callback to run each time after final metrics have been collected. */
    public static void setOnFinalMetricsCollectedListenerForTesting(Runnable listener) {
        AwMetricsServiceClientJni.get().setOnFinalMetricsCollectedListenerForTesting(listener);
    }

    @CalledByNative
    private static @InstallerPackageType int getInstallerPackageType() {
        ThreadUtils.assertOnUiThread();
        if (sInstallerPackageTypeForTesting != null) {
            return sInstallerPackageTypeForTesting;
        }
        // Only record if it's a system app or it was installed from Play Store.
        Context ctx = ContextUtils.getApplicationContext();
        if ((ctx.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
            return InstallerPackageType.SYSTEM_APP;
        } else {
            if (PLAY_STORE_PACKAGE_NAME.equals(ApkInfo.getInstallerPackageName())) {
                return InstallerPackageType.GOOGLE_PLAY_STORE;
            }
        }
        return InstallerPackageType.OTHER;
    }

    @CalledByNative
    private static String getAppPackageName() {
        // Return this unconditionally; let native code enforce whether or not it's OK to include
        // this in the logs.
        return ApkInfo.getHostPackageName();
    }

    public static void setInstallerPackageTypeForTesting(@InstallerPackageType int type) {
        ThreadUtils.assertOnUiThread();
        sInstallerPackageTypeForTesting = type;
    }

    @CalledByNative
    @JniType("std::string")
    private static String getNoBackupFilesDirForMetrics() {
        if (AwBrowserProcess.isDataDirBasePathOverridden()) {
            // If the base path has been overridden we shouldn't use the no-backup files directory,
            // because there's no API for the host app to override that directory. Return an empty
            // string as we have no directory to use.
            return "";
        }

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            File noBackupFilesDir = ContextUtils.getApplicationContext().getNoBackupFilesDir();
            String dataDirSuffix = AwBrowserProcess.getProcessDataDirSuffix();
            String subdir;
            if (dataDirSuffix == null) {
                subdir = METRICS_SUBDIR;
            } else {
                subdir = METRICS_SUBDIR + "_" + dataDirSuffix;
            }
            return new File(noBackupFilesDir, subdir).toString();
        }
    }

    @NativeMethods
    interface Natives {
        void setHaveMetricsConsent(boolean userConsent, boolean appConsent);

        void setFastStartupForTesting(boolean fastStartupForTesting);

        void setUploadIntervalForTesting(long uploadIntervalMs);

        void setOnFinalMetricsCollectedListenerForTesting(Runnable listener);
    }
}
