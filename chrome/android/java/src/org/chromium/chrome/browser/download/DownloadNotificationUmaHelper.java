// Copyright 2017 The Chromium Authors
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

/** Helper to track necessary stats in UMA related to downloads notifications. */
public final class DownloadNotificationUmaHelper {
    // NOTE: Keep these lists/classes in sync with DownloadNotification[...] in enums.xml.
    @IntDef({ForegroundLifecycle.START, ForegroundLifecycle.UPDATE, ForegroundLifecycle.STOP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ForegroundLifecycle {
        int START = 0; // Initial startForeground.
        int UPDATE = 1; // Switching pinned notification.
        int STOP = 2; // Calling stopForeground.
        int NUM_ENTRIES = 3;
    }

    private static List<String> sInteractions =
            Arrays.asList(
                    ACTION_NOTIFICATION_CLICKED, // Opening a download where
                    // LegacyHelpers.isLegacyDownload.
                    ACTION_DOWNLOAD_OPEN, // Opening a download that is not a legacy download.
                    ACTION_DOWNLOAD_CANCEL,
                    ACTION_DOWNLOAD_PAUSE,
                    ACTION_DOWNLOAD_RESUME);

    @IntDef({LaunchType.LAUNCH, LaunchType.RELAUNCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchType {
        int LAUNCH = 0; // "Denominator" for expected launched notifications.
        int RELAUNCH = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({
        ServiceStopped.STOPPED,
        ServiceStopped.DESTROYED,
        ServiceStopped.TASK_REMOVED,
        ServiceStopped.LOW_MEMORY,
        ServiceStopped.START_STICKY
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ServiceStopped {
        int STOPPED = 0; // Expected, intentional stops, serves as a "denominator".
        int DESTROYED = 1;
        int TASK_REMOVED = 2;
        int LOW_MEMORY = 3;
        int START_STICKY = 4;
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
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.NotificationInteraction",
                actionType,
                sInteractions.size());
    }

    /**
     * Records an instance where the foreground stops, using expected stops as the denominator to
     * understand the frequency of unexpected stops (low memory, task removed, etc).
     * @param stopType Type of the foreground stop that is being recorded ({@link ServiceStopped}).
     */
    static void recordServiceStoppedHistogram(@ServiceStopped int stopType) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.ServiceStopped.DownloadForeground",
                stopType,
                ServiceStopped.NUM_ENTRIES);
    }

    /**
     * Records an instance where the foreground undergoes a lifecycle change (when the foreground
     * starts, changes pinned notification, or stops).
     * @param lifecycleStep The lifecycle step that is being recorded ({@link ForegroundLifecycle}).
     */
    static void recordForegroundServiceLifecycleHistogram(@ForegroundLifecycle int lifecycleStep) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.ForegroundServiceLifecycle",
                lifecycleStep,
                ForegroundLifecycle.NUM_ENTRIES);
    }
}
