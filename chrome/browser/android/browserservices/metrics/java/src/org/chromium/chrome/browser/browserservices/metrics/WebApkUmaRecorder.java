// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.metrics;

import android.content.ContentResolver;
import android.os.Environment;
import android.os.StatFs;
import android.provider.Settings;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.webapps.WebApkDistributor;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for WebAPKs. NOTE: Histogram names and values are defined in
 * tools/metrics/histograms/histograms.xml. Please update that file if any change is made.
 */
public class WebApkUmaRecorder {
    // This enum is used to back UMA histograms, and should therefore be treated as append-only.
    @IntDef({UpdateRequestSent.WHILE_WEBAPK_CLOSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateRequestSent {
        // Deprecated: FIRST_TRY = 0;
        // Deprecated: ONSTOP = 1;
        // Deprecated: WHILE_WEBAPK_IN_FOREGROUND = 2;
        int WHILE_WEBAPK_CLOSED = 3;
        int NUM_ENTRIES = 4;
    }

    // This enum is used to back UMA histograms, and should therefore be treated as append-only.
    @IntDef({
        GooglePlayInstallResult.SUCCESS,
        GooglePlayInstallResult.FAILED_NO_DELEGATE,
        GooglePlayInstallResult.FAILED_TO_CONNECT_TO_SERVICE,
        GooglePlayInstallResult.FAILED_CALLER_VERIFICATION_FAILURE,
        GooglePlayInstallResult.FAILED_POLICY_VIOLATION,
        GooglePlayInstallResult.FAILED_API_DISABLED,
        GooglePlayInstallResult.FAILED_REQUEST_FAILED,
        GooglePlayInstallResult.FAILED_DOWNLOAD_CANCELLED,
        GooglePlayInstallResult.FAILED_DOWNLOAD_ERROR,
        GooglePlayInstallResult.FAILED_INSTALL_ERROR,
        GooglePlayInstallResult.FAILED_INSTALL_TIMEOUT,
        GooglePlayInstallResult.REQUEST_FAILED_POLICY_DISABLED,
        GooglePlayInstallResult.REQUEST_FAILED_UNKNOWN_ACCOUNT,
        GooglePlayInstallResult.REQUEST_FAILED_NETWORK_ERROR,
        GooglePlayInstallResult.REQUSET_FAILED_RESOLVE_ERROR,
        GooglePlayInstallResult.REQUEST_FAILED_NOT_GOOGLE_SIGNED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface GooglePlayInstallResult {
        int SUCCESS = 0;
        int FAILED_NO_DELEGATE = 1;
        int FAILED_TO_CONNECT_TO_SERVICE = 2;
        int FAILED_CALLER_VERIFICATION_FAILURE = 3;
        int FAILED_POLICY_VIOLATION = 4;
        int FAILED_API_DISABLED = 5;
        int FAILED_REQUEST_FAILED = 6;
        int FAILED_DOWNLOAD_CANCELLED = 7;
        int FAILED_DOWNLOAD_ERROR = 8;
        int FAILED_INSTALL_ERROR = 9;
        int FAILED_INSTALL_TIMEOUT = 10;
        // REQUEST_FAILED_* errors are the error codes shown in the "reason" of
        // the returned Bundle when calling installPackage() API returns false.
        int REQUEST_FAILED_POLICY_DISABLED = 11;
        int REQUEST_FAILED_UNKNOWN_ACCOUNT = 12;
        int REQUEST_FAILED_NETWORK_ERROR = 13;
        int REQUSET_FAILED_RESOLVE_ERROR = 14;
        int REQUEST_FAILED_NOT_GOOGLE_SIGNED = 15;
        int NUM_ENTRIES = 16;
    }

    public static final String HISTOGRAM_UPDATE_REQUEST_SENT = "WebApk.Update.RequestSent";

    public static final String HISTOGRAM_UPDATE_REQUEST_SHELL_VERSION =
            "WebApk.Update.ShellVersion";

    private static final String HISTOGRAM_LAUNCH_TO_SPLASHSCREEN_VISIBLE =
            "WebApk.Startup.Cold.ShellLaunchToSplashscreenVisible";
    private static final String HISTOGRAM_NEW_STYLE_LAUNCH_TO_SPLASHSCREEN_VISIBLE =
            "WebApk.Startup.Cold.NewStyle.ShellLaunchToSplashscreenVisible";

    public static final int WEBAPK_OPEN_LAUNCH_SUCCESS = 0;
    // Obsolete: WEBAPK_OPEN_NO_LAUNCH_INTENT = 1;
    public static final int WEBAPK_OPEN_ACTIVITY_NOT_FOUND = 2;

    private static final long WEBAPK_EXTRA_INSTALLATION_SPACE_BYTES =
            100 * (long) ConversionUtils.BYTES_PER_MEGABYTE; // 100 MB

    /**
     * Records the time point when a request to update a WebAPK is sent to the WebAPK Server.
     * @param type representing when the update request is sent to the WebAPK server.
     */
    public static void recordUpdateRequestSent(@UpdateRequestSent int type) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_UPDATE_REQUEST_SENT, type, UpdateRequestSent.NUM_ENTRIES);
    }

    /**
     * Records the times that an update request has been queued once, twice and three times before
     * sending to WebAPK server.
     *
     * @param times representing the times that an update has been queued.
     */
    public static void recordQueuedUpdateShellVersion(int shellApkVersion) {
        RecordHistogram.recordSparseHistogram(
                HISTOGRAM_UPDATE_REQUEST_SHELL_VERSION, shellApkVersion);
    }

    /**
     * Records duration between starting the WebAPK shell until the splashscreen is shown.
     * @param durationMs duration in milliseconds
     */
    public static void recordShellApkLaunchToSplashVisible(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                HISTOGRAM_LAUNCH_TO_SPLASHSCREEN_VISIBLE, durationMs);
    }

    /**
     * Records duration between starting the WebAPK shell until the shell displays the
     * splashscreen for new-style WebAPKs.
     */
    public static void recordNewStyleShellApkLaunchToSplashVisible(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                HISTOGRAM_NEW_STYLE_LAUNCH_TO_SPLASHSCREEN_VISIBLE, durationMs);
    }

    /** Records the notification permission status for a WebAPK. */
    public static void recordNotificationPermissionStatus(@ContentSettingValues int settingValue) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.Notification.Permission.Status2",
                settingValue,
                ContentSettingValues.NUM_SETTINGS);
    }

    /** Records the notification permission request result for a WebAPK. */
    public static void recordNotificationPermissionRequestResult(
            @ContentSettingValues int settingValue) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.Notification.PermissionRequestResult",
                settingValue,
                ContentSettingValues.NUM_SETTINGS);
    }

    /**
     * Records whether installing a WebAPK from Google Play succeeded. If not, records the reason
     * that the install failed.
     */
    public static void recordGooglePlayInstallResult(@GooglePlayInstallResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.Install.GooglePlayInstallResult",
                result,
                GooglePlayInstallResult.NUM_ENTRIES);
    }

    /** Records the error code if installing a WebAPK via Google Play fails. */
    public static void recordGooglePlayInstallErrorCode(int errorCode) {
        // Don't use an enumerated histogram as there are > 30 potential error codes. In practice,
        // a given client will always get the same error code.
        RecordHistogram.recordSparseHistogram(
                "WebApk.Install.GooglePlayErrorCode", Math.min(errorCode, 10000));
    }

    /**
     * Records whether updating a WebAPK from Google Play succeeded. If not, records the reason
     * that the update failed.
     */
    public static void recordGooglePlayUpdateResult(@GooglePlayInstallResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.Update.GooglePlayUpdateResult",
                result,
                GooglePlayInstallResult.NUM_ENTRIES);
    }

    /** Records the duration of a WebAPK session (from launch/foreground to background). */
    public static void recordWebApkSessionDuration(
            @WebApkDistributor int distributor, long duration) {
        RecordHistogram.recordLongTimesHistogram(
                "WebApk.Session.TotalDuration3." + getWebApkDistributorUmaSuffix(distributor),
                duration);
    }

    /** Records the current Shell APK version. */
    public static void recordShellApkVersion(
            int shellApkVersion, @WebApkDistributor int distributor) {
        RecordHistogram.recordSparseHistogram(
                "WebApk.ShellApkVersion2." + getWebApkDistributorUmaSuffix(distributor),
                shellApkVersion);
    }

    private static String getWebApkDistributorUmaSuffix(@WebApkDistributor int distributor) {
        switch (distributor) {
            case WebApkDistributor.BROWSER:
                return "Browser";
            case WebApkDistributor.DEVICE_POLICY:
                return "DevicePolicy";
            default:
                return "Other";
        }
    }

    /** Records to UMA the count of old "WebAPK update request" files. */
    public static void recordNumberOfStaleWebApkUpdateRequestFiles(int count) {
        RecordHistogram.recordCount1MHistogram("WebApk.Update.NumStaleUpdateRequestFiles", count);
    }

    /** Records the network error code caught when a WebAPK is launched. */
    public static void recordNetworkErrorWhenLaunch(int errorCode) {
        RecordHistogram.recordSparseHistogram("WebApk.Launch.NetworkError", -errorCode);
    }

    /** Records number of unique origins for WebAPKs in WebappRegistry */
    public static void recordWebApksCount(int count) {
        RecordHistogram.recordCount100Histogram("WebApk.WebappRegistry.NumberOfOrigins", count);
    }

    private static int roundByteToMb(long bytes) {
        int mbs = (int) (bytes / (long) ConversionUtils.BYTES_PER_MEGABYTE / 10L * 10L);
        return Math.min(1000, Math.max(-1000, mbs));
    }

    private static long getDirectorySizeInByte(File dir) {
        if (dir == null) return 0;
        if (!dir.isDirectory()) return dir.length();

        long sizeInByte = 0;
        try {
            File[] files = dir.listFiles();
            if (files == null) return 0;
            for (File file : files) sizeInByte += getDirectorySizeInByte(file);
        } catch (SecurityException e) {
            return 0;
        }
        return sizeInByte;
    }

    /**
     * @return The available space that can be used to install WebAPK. Negative value means there is
     * less free space available than the system's minimum by the given amount.
     */
    public static long getAvailableSpaceAboveLowSpaceLimit() {
        StatFs partitionStats = new StatFs(Environment.getDataDirectory().getAbsolutePath());
        long partitionAvailableBytes = partitionStats.getAvailableBytes();
        long partitionTotalBytes = partitionStats.getTotalBytes();
        long minimumFreeBytes = getLowSpaceLimitBytes(partitionTotalBytes);

        long webApkExtraSpaceBytes = WEBAPK_EXTRA_INSTALLATION_SPACE_BYTES;
        return partitionAvailableBytes - minimumFreeBytes + webApkExtraSpaceBytes;
    }

    /**
     * @return Size of the cache directory.
     */
    public static long getCacheDirSize() {
        return getDirectorySizeInByte(ContextUtils.getApplicationContext().getCacheDir());
    }

    /** Mirror the system-derived calculation of reserved bytes and return that value. */
    private static long getLowSpaceLimitBytes(long partitionTotalBytes) {
        // Copied from android/os/storage/StorageManager.java
        final int defaultThresholdPercentage = 10;
        // Copied from android/os/storage/StorageManager.java
        final long defaultThresholdMaxBytes = 500 * ConversionUtils.BYTES_PER_MEGABYTE;
        // Copied from android/provider/Settings.java
        final String sysStorageThresholdPercentage = "sys_storage_threshold_percentage";
        // Copied from android/provider/Settings.java
        final String sysStorageThresholdMaxBytes = "sys_storage_threshold_max_bytes";

        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();

        // Retrieve platform-appropriate values first
        int minFreePercent =
                Settings.Global.getInt(
                        resolver, sysStorageThresholdPercentage, defaultThresholdPercentage);
        long minFreeBytes =
                Settings.Global.getLong(
                        resolver, sysStorageThresholdMaxBytes, defaultThresholdMaxBytes);

        long minFreePercentInBytes = (partitionTotalBytes * minFreePercent) / 100;

        return Math.min(minFreeBytes, minFreePercentInBytes);
    }

    private WebApkUmaRecorder() {}
}
