// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.SystemClock;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Utility class for recording complex, multi-metric startup and creation time UMA histograms and
 * trace events during Android WebView initialization.
 *
 * <p>Centralizing dense metric blocks in the Aw layer reduces line count and code duplication in
 * caller classes, especially when input state and durations are shared across multiple metric
 * dimensions.
 */
@NullMarked
public final class StartupMetrics {
    /** Helper class for recording WebView startup histograms. */
    private StartupMetrics() {}

    private static final AtomicBoolean sFirstWebViewInstanceCreated = new AtomicBoolean();

    // =========================================================================
    // Stage 2 / Chromium Init Creation Time Histograms
    // =========================================================================
    private static final String HISTOGRAM_START_CHROMIUM_LOCKED =
            "Android.WebView.Startup.CreationTime.StartChromiumLocked";
    private static final String HISTOGRAM_LONGEST_UI_BLOCKING_TASK_TIME =
            "Android.WebView.Startup.ChromiumInitTime.LongestUiBlockingTaskTime";
    private static final String HISTOGRAM_STARTUP_MODE =
            "Android.WebView.Startup.ChromiumInitTime.StartupMode";
    private static final String HISTOGRAM_INIT_REASON2 =
            "Android.WebView.Startup.CreationTime.InitReason2";
    private static final String HISTOGRAM_WALL_CLOCK_TIME =
            "Android.WebView.Startup.ChromiumInitTime.WallClockTime";

    // =========================================================================
    // Provider Init Creation Time Histograms
    // =========================================================================
    private static final String HISTOGRAM_FIRST_INSTANCE_AFTER_GLOBAL_STARTUP =
            "Android.WebView.Startup.CreationTime.FirstInstanceAfterGlobalStartup";
    private static final String HISTOGRAM_FIRST_INSTANCE_WITH_GLOBAL_STARTUP =
            "Android.WebView.Startup.CreationTime.FirstInstanceWithGlobalStartup";
    private static final String HISTOGRAM_NOT_FIRST_INSTANCE =
            "Android.WebView.Startup.CreationTime.NotFirstInstance";

    // =========================================================================
    // Recording Helpers
    // =========================================================================

    /** Records creation time metrics and trace events for a newly constructed WebView instance. */
    public static void webViewInstanceCreated(
            long startTimeMs, boolean wasChromiumAlreadyInitialized) {
        long elapsedTimeMs = SystemClock.uptimeMillis() - startTimeMs;
        boolean isFirstWebViewInstance = !sFirstWebViewInstanceCreated.getAndSet(true);

        if (isFirstWebViewInstance) {
            if (wasChromiumAlreadyInitialized) {
                // This is the first WebView created, but global Chromium initialization happened
                // before the constructor was called.
                RecordHistogram.recordTimesHistogram(
                        HISTOGRAM_FIRST_INSTANCE_AFTER_GLOBAL_STARTUP, elapsedTimeMs);
                TraceEvent.webViewStartupFirstInstance(startTimeMs, elapsedTimeMs, false);
            } else {
                // This is the first WebView created, and we blocked running global Chromium
                // initialization during the constructor.
                RecordHistogram.recordTimesHistogram(
                        HISTOGRAM_FIRST_INSTANCE_WITH_GLOBAL_STARTUP, elapsedTimeMs);
                TraceEvent.webViewStartupFirstInstance(startTimeMs, elapsedTimeMs, true);
            }
        } else {
            // This is not the first WebView created; global Chromium initialization must have
            // happened beforehand.
            RecordHistogram.recordTimesHistogram(HISTOGRAM_NOT_FIRST_INSTANCE, elapsedTimeMs);
            TraceEvent.webViewStartupNotFirstInstance(startTimeMs, elapsedTimeMs);
        }
    }

    /**
     * Records all Chromium startup metrics including mode-specific slices and call site reasons
     * from {@link StartupTasksRunner.StartupTimings}, as well as the corresponding trace events.
     */
    public static void recordChromiumInitTimes(StartupTasksRunner.StartupTimings timings) {
        String startupModeSuffix = getStartupModeSuffix(timings.startupMode);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_START_CHROMIUM_LOCKED, timings.totalTimeTakenMs);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_START_CHROMIUM_LOCKED + startupModeSuffix, timings.totalTimeTakenMs);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_LONGEST_UI_BLOCKING_TASK_TIME, timings.longestUiBlockingTaskTimeMs);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_LONGEST_UI_BLOCKING_TASK_TIME + startupModeSuffix,
                timings.longestUiBlockingTaskTimeMs);
        RecordHistogram.recordTimesHistogram(HISTOGRAM_WALL_CLOCK_TIME, timings.wallClockTimeMs);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_WALL_CLOCK_TIME + startupModeSuffix, timings.wallClockTimeMs);

        // Record traces
        TraceEvent.webViewStartupStartChromiumLocked(
                timings.startTimeMs,
                timings.totalTimeTakenMs,
                /* startCallSite= */ timings.startCallSite,
                /* finishCallSite= */ timings.finishCallSite,
                /* startupMode= */ timings.startupMode);

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_STARTUP_MODE, timings.startupMode, StartupTasksRunner.StartupMode.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_INIT_REASON2, timings.startCallSite, StartupCallSite.COUNT);
    }

    private static String getStartupModeSuffix(@StartupTasksRunner.StartupMode int startupMode) {
        return switch (startupMode) {
            case StartupTasksRunner.StartupMode.FULLY_SYNC -> ".FullySync";
            case StartupTasksRunner.StartupMode.FULLY_ASYNC -> ".FullyAsync";
            case StartupTasksRunner.StartupMode.ASYNC_BUT_FULLY_SYNC -> ".AsyncButFullySync";
            case StartupTasksRunner.StartupMode.PARTIAL_ASYNC_THEN_SYNC -> ".PartialAsyncThenSync";
            default -> ".Unknown";
        };
    }
}
