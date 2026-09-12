// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper to track necessary stats in UMA related to downloads notifications. */
@NullMarked
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
     * Records an instance where the foreground stops, using expected stops as the denominator to
     * understand the frequency of unexpected stops (low memory, task removed, etc).
     *
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
