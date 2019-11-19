// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.DownloadManager.ACTION_NOTIFICATION_CLICKED;

import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_CANCEL;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_OPEN;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_PAUSE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_RESUME;

import androidx.annotation.IntDef;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

/**
 * Helper to track necessary stats in UMA related to downloads notifications.
 */
public final class DownloadNotificationUmaHelper {
    // The state of a download or offline page request at user-initiated cancel.
    // Keep in sync with enum OfflineItemsStateAtCancel in enums.xml.
    @IntDef({StateAtCancel.DOWNLOADING, StateAtCancel.PAUSED, StateAtCancel.PENDING_NETWORK,
            StateAtCancel.PENDING_ANOTHER_DOWNLOAD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateAtCancel {
        int DOWNLOADING = 0;
        int PAUSED = 1;
        int PENDING_NETWORK = 2;
        int PENDING_ANOTHER_DOWNLOAD = 3;

        int NUM_ENTRIES = 4;
    }

    // NOTE: Keep these lists/classes in sync with DownloadNotification[...] in enums.xml.
    @IntDef({ForegroundLifecycle.START, ForegroundLifecycle.UPDATE, ForegroundLifecycle.STOP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ForegroundLifecycle {
        int START = 0; // Initial startForeground.
        int UPDATE = 1; // Switching pinned notification.
        int STOP = 2; // Calling stopForeground.
        int NUM_ENTRIES = 3;
    }

    private static List<String> sInteractions = Arrays.asList(
            ACTION_NOTIFICATION_CLICKED, // Opening a download where LegacyHelpers.isLegacyDownload.
            ACTION_DOWNLOAD_OPEN, // Opening a download that is not a legacy download.
            ACTION_DOWNLOAD_CANCEL, ACTION_DOWNLOAD_PAUSE, ACTION_DOWNLOAD_RESUME);

    @IntDef({LaunchType.LAUNCH, LaunchType.RELAUNCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchType {
        int LAUNCH = 0; // "Denominator" for expected launched notifications.
        int RELAUNCH = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({ServiceStopped.STOPPED, ServiceStopped.DESTROYED, ServiceStopped.TASK_REMOVED,
            ServiceStopped.LOW_MEMORY, ServiceStopped.START_STICKY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ServiceStopped {
        int STOPPED = 0; // Expected, intentional stops, serves as a "denominator".
        int DESTROYED = 1;
        int TASK_REMOVED = 2;
        int LOW_MEMORY = 3;
        int START_STICKY = 4;
        int NUM_ENTRIES = 5;
    }

    // Values for the histogram MobileDownloadResumptionCount.
    @IntDef({UmaDownloadResumption.MANUAL_PAUSE, UmaDownloadResumption.BROWSER_KILLED,
            UmaDownloadResumption.CLICKED, UmaDownloadResumption.FAILED,
            UmaDownloadResumption.AUTO_STARTED, UmaDownloadResumption.BROWSER_RUNNING,
            UmaDownloadResumption.BROWSER_NOT_RUNNING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UmaDownloadResumption {
        int MANUAL_PAUSE = 0;
        int BROWSER_KILLED = 1;
        int CLICKED = 2;
        int FAILED = 3;
        int AUTO_STARTED = 4;
        int BROWSER_RUNNING = 5;
        int BROWSER_NOT_RUNNING = 6;
        int NUM_ENTRIES = 7;
    }

    // Values for the histograms MobileDownload.Background.*. Keep in sync with
    // MobileDownloadBackgroundDownloadEvent in enums.xml.
    @IntDef({UmaBackgroundDownload.STARTED, UmaBackgroundDownload.COMPLETED,
            UmaBackgroundDownload.CANCELLED, UmaBackgroundDownload.FAILED,
            UmaBackgroundDownload.INTERRUPTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UmaBackgroundDownload {
        int STARTED = 0;
        int COMPLETED = 1;
        int CANCELLED = 2;
        int FAILED = 3;
        int INTERRUPTED = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Records an instance where a user interacts with a notification (clicks on, pauses, etc).
     * @param action Notification interaction that was taken (ie. pause, resume).
     */
    static void recordNotificationInteractionHistogram(String action) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        int actionType = sInteractions.indexOf(action);
        if (actionType == -1) return;
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.NotificationInteraction",
                actionType, sInteractions.size());
    }

    /**
     * Records an instance where the foreground stops, using expected stops as the denominator to
     * understand the frequency of unexpected stops (low memory, task removed, etc).
     * @param stopType Type of the foreground stop that is being recorded ({@link ServiceStopped}).
     */
    static void recordServiceStoppedHistogram(
            @ServiceStopped int stopType, boolean withForeground) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        if (withForeground) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.ServiceStopped.DownloadForeground", stopType,
                    ServiceStopped.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.ServiceStopped.DownloadNotification", stopType,
                    ServiceStopped.NUM_ENTRIES);
        }
    }

    /**
     * Records an instance where the foreground undergoes a lifecycle change (when the foreground
     * starts, changes pinned notification, or stops).
     * @param lifecycleStep The lifecycle step that is being recorded ({@link ForegroundLifecycle}).
     */
    static void recordForegroundServiceLifecycleHistogram(@ForegroundLifecycle int lifecycleStep) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.ForegroundServiceLifecycle", lifecycleStep,
                ForegroundLifecycle.NUM_ENTRIES);
    }

    /**
     * Records the state of a request at user-initiated cancel.
     * @param isDownload True if the request is a download, false if it is an offline page.
     * @param state State of a request when cancelled (e.g. downloading, paused).
     */
    static void recordStateAtCancelHistogram(boolean isDownload, @StateAtCancel int state) {
        if (state == -1) return;
        if (!LibraryLoader.getInstance().isInitialized()) return;
        if (isDownload) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.OfflineItems.StateAtCancel.Downloads", state,
                    StateAtCancel.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.OfflineItems.StateAtCancel.OfflinePages", state,
                    StateAtCancel.NUM_ENTRIES);
        }
    }

    /**
     * Helper method to record the download resumption UMA.
     * @param type UMA type to be recorded.
     */
    static void recordDownloadResumptionHistogram(@UmaDownloadResumption int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.DownloadResumption", type, UmaDownloadResumption.NUM_ENTRIES);
    }

    /**
     * Helper method to record the background download resumption UMA.
     * @param type UMA type to be recorded.
     */
    static void recordBackgroundDownloadHistogram(@UmaBackgroundDownload int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.Background", type, UmaBackgroundDownload.NUM_ENTRIES);
    }

    /**
     * Helper method to record the first background download resumption UMA.
     * @param type UMA type to be recorded.
     * @param interruptionCount Number of interruptions since process launch.
     */
    static void recordFirstBackgroundDownloadHistogram(
            @UmaBackgroundDownload int type, int interruptionCount) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.Background.FirstDownload", type, UmaBackgroundDownload.NUM_ENTRIES);
        if (type != UmaBackgroundDownload.INTERRUPTED && type != UmaBackgroundDownload.STARTED) {
            RecordHistogram.recordCountHistogram(
                    "MobileDownload.FirstBackground.InterruptionCount", interruptionCount);
        }
    }
}
