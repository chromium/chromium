// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper to track necessary stats in UMA related to Actor foreground service. */
@NullMarked
public final class ActorForegroundServiceUmaHelper {
    // LINT.IfChange(ForegroundLifecycle)
    @IntDef({ForegroundLifecycle.STARTED, ForegroundLifecycle.UPDATED, ForegroundLifecycle.STOPPED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ForegroundLifecycle {
        int STARTED = 0;
        int UPDATED = 1;
        int STOPPED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorForegroundServiceLifecycle)

    // LINT.IfChange(StopReason)
    @IntDef({
        StopReason.STOPPED,
        StopReason.DESTROYED,
        StopReason.TASK_REMOVED,
        StopReason.LOW_MEMORY,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface StopReason {
        int STOPPED = 0;
        int DESTROYED = 1;
        int TASK_REMOVED = 2;
        int LOW_MEMORY = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorForegroundServiceStopReason)

    private ActorForegroundServiceUmaHelper() {}

    /**
     * Records an instance where the foreground undergoes a lifecycle change.
     *
     * @param lifecycleStep The lifecycle step that is being recorded.
     */
    public static void recordLifecycleHistogram(@ForegroundLifecycle int lifecycleStep) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.ForegroundService.Lifecycle",
                lifecycleStep,
                ForegroundLifecycle.NUM_ENTRIES);
    }

    /**
     * Records the reason why the service was stopped.
     *
     * @param stopReason The reason for stopping the service.
     */
    public static void recordStopReasonHistogram(@StopReason int stopReason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Actor.ForegroundService.StopReason", stopReason, StopReason.NUM_ENTRIES);
    }

    /**
     * Records the total duration of the foreground service.
     *
     * @param durationMs The duration in milliseconds.
     */
    public static void recordDurationHistogram(long durationMs) {
        RecordHistogram.recordLongTimesHistogram("Actor.ForegroundService.Duration", durationMs);
    }
}
