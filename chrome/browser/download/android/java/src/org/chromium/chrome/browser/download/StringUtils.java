// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.BrowserStartupController;

import java.text.NumberFormat;
import java.util.Locale;

/** Helper class to handle converting downloads to UI strings. */
public final class StringUtils {
    @VisibleForTesting static final String ELLIPSIS = "\u2026";

    @VisibleForTesting
    private static final int[] BYTES_AVAILABLE_STRINGS = {
        R.string.download_manager_ui_space_free_kb,
        R.string.download_manager_ui_space_free_mb,
        R.string.download_manager_ui_space_free_gb
    };

    private StringUtils() {}

    /**
     * Helper method to determine the progress text to use for an in progress download UI.
     * @param progress The {@link Progress} struct that represents the current state of an in
     *                 progress download.
     * @return         The {@link String} that represents the progress.
     */
    public static String getProgressTextForUi(Progress progress) {
        Context context = ContextUtils.getApplicationContext();

        if (progress.isIndeterminate() && progress.value == 0) {
            return context.getResources().getString(R.string.download_started);
        }

        switch (progress.unit) {
            case OfflineItemProgressUnit.PERCENTAGE:
                return progress.isIndeterminate()
                        ? context.getResources().getString(R.string.download_started)
                        : percentageForUi(progress.getPercentage());
            case OfflineItemProgressUnit.BYTES:
                String bytes = DownloadUtils.getStringForBytes(context, progress.value);
                if (progress.isIndeterminate()) {
                    return context.getResources()
                            .getString(R.string.download_ui_indeterminate_bytes, bytes);
                } else {
                    String total = DownloadUtils.getStringForBytes(context, progress.max);
                    return context.getResources()
                            .getString(R.string.download_ui_determinate_bytes, bytes, total);
                }
            case OfflineItemProgressUnit.FILES:
                if (progress.isIndeterminate()) {
                    int fileCount = (int) Math.min(Integer.MAX_VALUE, progress.value);
                    return context.getResources()
                            .getQuantityString(
                                    R.plurals.download_ui_files_downloaded, fileCount, fileCount);
                } else {
                    return filesLeftForUi(context, progress);
                }
            default:
                assert false;
        }

        return "";
    }

    /**
     * Format remaining time for the given millis, in the following format:
     * 5 hours; will include 1 unit, can go down to seconds precision.
     * This is similar to what android.java.text.Formatter.formatShortElapsedTime() does. Don't use
     * ui::TimeFormat::Simple() as it is very expensive.
     *
     * @param context the application context.
     * @param millis the remaining time in milli seconds.
     * @return the formatted remaining time.
     */
    public static String timeLeftForUi(Context context, long millis) {
        long secondsLong = millis / 1000;

        int days = 0;
        int hours = 0;
        int minutes = 0;
        if (secondsLong >= TimeUtils.SECONDS_PER_DAY) {
            days = (int) (secondsLong / TimeUtils.SECONDS_PER_DAY);
            secondsLong -= days * TimeUtils.SECONDS_PER_DAY;
        }
        if (secondsLong >= TimeUtils.SECONDS_PER_HOUR) {
            hours = (int) (secondsLong / TimeUtils.SECONDS_PER_HOUR);
            secondsLong -= hours * TimeUtils.SECONDS_PER_HOUR;
        }
        if (secondsLong >= TimeUtils.SECONDS_PER_MINUTE) {
            minutes = (int) (secondsLong / TimeUtils.SECONDS_PER_MINUTE);
            secondsLong -= minutes * TimeUtils.SECONDS_PER_MINUTE;
        }
        int seconds = (int) secondsLong;

        if (days >= 2) {
            days += (hours + 12) / 24;
            return context.getString(R.string.remaining_duration_days, days);
        } else if (days > 0) {
            return context.getString(R.string.remaining_duration_one_day);
        } else if (hours >= 2) {
            hours += (minutes + 30) / 60;
            return context.getString(R.string.remaining_duration_hours, hours);
        } else if (hours > 0) {
            return context.getString(R.string.remaining_duration_one_hour);
        } else if (minutes >= 2) {
            minutes += (seconds + 30) / 60;
            return context.getString(R.string.remaining_duration_minutes, minutes);
        } else if (minutes > 0) {
            return context.getString(R.string.remaining_duration_one_minute);
        } else if (seconds == 1) {
            return context.getString(R.string.remaining_duration_one_second);
        } else {
            return context.getString(R.string.remaining_duration_seconds, seconds);
        }
    }

    /**
     * Determine the status string for a failed download.
     *
     * @param failState Reason download failed.
     * @return String representing the current download status.
     */
    public static String getFailStatusForUi(@FailState int failState) {
        if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
            return StringUtilsJni.get().getFailStateMessage(failState);
        }
        Context context = ContextUtils.getApplicationContext();
        return context.getString(R.string.download_notification_failed);
    }

    /**
     * Determine the status string for a pending download.
     *
     * @param pendingState Reason download is pending.
     * @return String representing the current download status.
     */
    public static String getPendingStatusForUi(@PendingState int pendingState) {
        Context context = ContextUtils.getApplicationContext();
        // When foreground service restarts and there is no connection to native, use the default
        // pending status. The status will be replaced when connected to native.
        if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
            switch (pendingState) {
                case PendingState.PENDING_NETWORK:
                    return context.getString(R.string.download_notification_pending_network);
                case PendingState.PENDING_ANOTHER_DOWNLOAD:
                    return context.getString(
                            R.string.download_notification_pending_another_download);
                default:
                    return context.getString(R.string.download_notification_pending);
            }
        } else {
            return context.getString(R.string.download_notification_pending);
        }
    }

    /**
     * Format the number of available bytes into KB, MB, or GB and return the corresponding string
     * resource. Uses default format "20 KB available."
     *
     * @param context   Context to use.
     * @param bytes     Number of bytes needed to display.
     * @return          The formatted string to be displayed.
     */
    public static String getAvailableBytesForUi(Context context, long bytes) {
        return DownloadUtils.getStringForBytes(context, BYTES_AVAILABLE_STRINGS, bytes);
    }

    /**
     * Abbreviate a file name into a given number of characters with ellipses.
     * e.g. "thisisaverylongfilename.txt" => "thisisave....txt".
     * @param fileName File name to abbreviate.
     * @param limit Character limit.
     * @return Abbreviated file name.
     */
    public static String getAbbreviatedFileName(String fileName, int limit) {
        assert limit >= 1; // Abbreviated file name should at least be 1 characters (a...)

        if (TextUtils.isEmpty(fileName) || fileName.length() <= limit) return fileName;

        // Find the file name extension
        int index = fileName.lastIndexOf(".");
        int extensionLength = fileName.length() - index;

        // If the extension is too long, just use truncate the string from beginning.
        if (extensionLength >= limit) {
            return fileName.substring(0, limit) + ELLIPSIS;
        }
        int remainingLength = limit - extensionLength;
        return fileName.substring(0, remainingLength) + ELLIPSIS + fileName.substring(index);
    }

    /**
     * Create a string that represents the percentage of the file that has downloaded.
     * @param percentage Current percentage of the file.
     * @return String representing the percentage of the file that has been downloaded.
     */
    private static String percentageForUi(int percentage) {
        NumberFormat formatter = NumberFormat.getPercentInstance(Locale.getDefault());
        return formatter.format(percentage / 100.0);
    }

    /**
     * Creates a string that represents the number of files left to be downloaded.
     * @param progress Current download progress.
     * @return String representing the number of files left.
     */
    private static String filesLeftForUi(Context context, Progress progress) {
        int filesLeft = (int) (progress.max - progress.value);
        return filesLeft == 1
                ? context.getResources().getString(R.string.one_file_left)
                : context.getResources().getString(R.string.files_left, filesLeft);
    }

    @NativeMethods
    interface Natives {
        String getFailStateMessage(@FailState int failState);
    }
}
