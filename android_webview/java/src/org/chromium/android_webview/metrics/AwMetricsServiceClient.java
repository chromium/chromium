// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Determines user consent and app opt-out for metrics. See aw_metrics_service_client.h for more
 * explanation.
 */
@JNINamespace("android_webview")
public class AwMetricsServiceClient {
    private static final String TAG = "AwMetricsServiceCli-";

    // Individual apps can use this meta-data tag in their manifest to opt out of metrics
    // reporting. See https://developer.android.com/reference/android/webkit/WebView.html
    private static final String OPT_OUT_META_DATA_STR = "android.webkit.WebView.MetricsOptOut";

    private static final String PLAY_STORE_PACKAGE_NAME = "com.android.vending";

    private static boolean isAppOptedOut(Context ctx) {
        try {
            ApplicationInfo info = ctx.getPackageManager().getApplicationInfo(
                    ctx.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found.
                return false;
            }
            // getBoolean returns false if the key is not found, which is what we want.
            return info.metaData.getBoolean(OPT_OUT_META_DATA_STR);
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            // The conservative thing is to assume the app HAS opted out.
            return true;
        }
    }

    @CalledByNative
    private static boolean canRecordPackageNameForAppType() {
        // Only record if it's a system app or it was installed from Play Store.
        Context ctx = ContextUtils.getApplicationContext();
        String packageName = ctx.getPackageName();
        String installerPackageName = ctx.getPackageManager().getInstallerPackageName(packageName);
        return (ctx.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0
                || (PLAY_STORE_PACKAGE_NAME.equals(installerPackageName));
    }

    public static void setConsentSetting(Context ctx, boolean userConsent) {
        ThreadUtils.assertOnUiThread();
        AwMetricsServiceClientJni.get().setHaveMetricsConsent(userConsent, !isAppOptedOut(ctx));
    }

    @VisibleForTesting
    public static void setFastStartupForTesting(boolean fastStartupForTesting) {
        AwMetricsServiceClientJni.get().setFastStartupForTesting(fastStartupForTesting);
    }

    @VisibleForTesting
    public static void setUploadIntervalForTesting(long uploadIntervalMs) {
        AwMetricsServiceClientJni.get().setUploadIntervalForTesting(uploadIntervalMs);
    }

    @CalledByNative
    private static String getAppPackageName() {
        // Return this unconditionally; let native code enforce whether or not it's OK to include
        // this in the logs.
        Context ctx = ContextUtils.getApplicationContext();
        return ctx.getPackageName();
    }

    /**
     * Gets a long representing the install time of the embedder application. Units are in seconds,
     * as this is the resolution used by the metrics service. Returns {@code -1} upon failure.
     */
    // TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has been persisted
    // for one or two milestones.
    @CalledByNative
    private static long getAppInstallTime() {
        try {
            Context ctx = ContextUtils.getApplicationContext();
            long installTimeMs = ctx.getPackageManager()
                                         .getPackageInfo(ctx.getPackageName(), 0 /* flags */)
                                         .firstInstallTime;
            long installTimeSec = installTimeMs / 1000;
            return installTimeSec;
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            return -1;
        }
    }

    @NativeMethods
    interface Natives {
        void setHaveMetricsConsent(boolean userConsent, boolean appConsent);
        void setFastStartupForTesting(boolean fastStartupForTesting);
        void setUploadIntervalForTesting(long uploadIntervalMs);
    }
}
