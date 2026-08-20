// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.ArrayDeque;
import java.util.Locale;

/**
 * Runner responsible for executing Chromium startup tasks asynchronously or synchronously depending
 * on whether startup was triggered from the background or UI thread.
 */
@NullMarked
public final class StartupTasksRunner {
    private static final String TAG = "StartupTasksRunner";

    @IntDef({
        StartupRequestMode.UNSET,
        StartupRequestMode.SYNC,
        StartupRequestMode.ASYNC,
    })
    public @interface StartupRequestMode {
        int UNSET = 0;
        int SYNC = 1;
        int ASYNC = 2;
    }

    // LINT.IfChange(WebViewChromiumStartupMode)
    @IntDef({
        StartupMode.FULLY_SYNC,
        StartupMode.FULLY_ASYNC,
        StartupMode.PARTIAL_ASYNC_THEN_SYNC,
        StartupMode.ASYNC_BUT_FULLY_SYNC,
        StartupMode.COUNT,
    })
    public @interface StartupMode {
        /** Startup was triggered on the UI thread and completed synchronously. */
        int FULLY_SYNC = 0;

        /** Startup was triggered on a background thread and completed asynchronously. */
        int FULLY_ASYNC = 1;

        /**
         * Startup was triggered on a background thread, some tasks ran asynchronously. Then another
         * init call on the UI thread preempted the async run and startup completed synchronously.
         */
        int PARTIAL_ASYNC_THEN_SYNC = 2;

        /**
         * Startup was triggered on a background thread, but the posted task was not run yet. Then
         * another init call on the UI thread was started before the posted task and startup fully
         * completed synchronously.
         */
        int ASYNC_BUT_FULLY_SYNC = 3;

        int COUNT = 4;
    }

    // LINT.ThenChange(//base/tracing/protos/chrome_track_event.proto:WebViewChromiumStartupMode,//tools/metrics/histograms/metadata/android/enums.xml:WebViewChromiumStartupMode)

    /** Immutable value class containing timings and state recorded during Chromium startup. */
    public static class StartupTimings {
        public final long startTimeMs;
        public final long totalTimeTakenMs;
        public final long longestUiBlockingTaskTimeMs;
        public final long wallClockTimeMs;
        public final @StartupMode int startupMode;
        public final @StartupCallSite int startCallSite;
        public final @StartupCallSite int finishCallSite;

        StartupTimings(StartupTasksRunner runner) {
            this.startTimeMs = runner.mStartupTimeMs;
            this.totalTimeTakenMs = runner.mTotalTimeTakenMs;
            this.longestUiBlockingTaskTimeMs = runner.mLongestUiBlockingTaskTimeMs;
            this.wallClockTimeMs = SystemClock.uptimeMillis() - runner.mStartupTimeMs;
            this.startupMode = runner.calculateStartupMode();
            this.startCallSite = runner.mStartCallSite;
            this.finishCallSite = runner.mFinishCallSite;
        }
    }

    /** Delegate interface for communicating back with the startup coordinator. */
    public interface Delegate {
        /** Called when all tasks are complete to record metrics and notify listeners. */
        void onStartupComplete(StartupTimings timings);

        /** Called when a startup task throws a runtime exception. */
        void onStartupFailed(RuntimeException e);

        /** Called when a startup task throws an error. */
        void onStartupFailed(Error e);

        /** Returns true if startup has already finished. */
        boolean isStartupFinished();
    }

    private final Delegate mDelegate;
    private final ArrayDeque<Runnable> mPreBrowserProcessStartQueue;
    private final ArrayDeque<Runnable> mPostBrowserProcessStartQueue;
    private final int mPreBrowserProcessStartTasksSize;
    private final int mNumTasks;
    private final @StartupRequestMode int mChromiumFirstStartupRequestMode;

    private boolean mAsyncHasBeenTriggered;
    private long mLongestUiBlockingTaskTimeMs;
    private long mTotalTimeTakenMs;
    private long mStartupTimeMs;
    private boolean mStartupStarted;
    private @StartupCallSite int mStartCallSite = StartupCallSite.COUNT;
    private @StartupCallSite int mFinishCallSite = StartupCallSite.COUNT;
    private boolean mFirstTaskFromSynchronousCall;
    private @StartupRequestMode int mRunState = StartupRequestMode.UNSET;

    public StartupTasksRunner(
            Delegate delegate,
            ArrayDeque<Runnable> preBrowserProcessStartTasks,
            ArrayDeque<Runnable> postBrowserProcessStartTasks,
            @StartupRequestMode int chromiumFirstStartupRequestMode) {
        mDelegate = delegate;
        mPreBrowserProcessStartQueue = preBrowserProcessStartTasks;
        mPostBrowserProcessStartQueue = postBrowserProcessStartTasks;
        mPreBrowserProcessStartTasksSize = preBrowserProcessStartTasks.size();
        mNumTasks = mPreBrowserProcessStartTasksSize + postBrowserProcessStartTasks.size();
        mChromiumFirstStartupRequestMode = chromiumFirstStartupRequestMode;
    }

    private boolean shouldRunStartupTasksAsync() {
        return !CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_RUN_STARTUP_TASKS_SYNC);
    }

    public void run(@StartupCallSite int callSite, boolean triggeredFromUIThread) {
        assert ThreadUtils.runningOnUiThread();

        if (!mStartupStarted) {
            mStartupStarted = true;
            mFirstTaskFromSynchronousCall = triggeredFromUIThread;
            mStartCallSite = callSite;
            mFinishCallSite = callSite;
            mStartupTimeMs = SystemClock.uptimeMillis();
        }

        // Early return to avoid repeating the return call within sync and async blocks
        if (mPostBrowserProcessStartQueue.isEmpty()) {
            assert mDelegate.isStartupFinished();
            return;
        }

        if (shouldRunStartupTasksAsync() && !triggeredFromUIThread) {
            // Prevents triggering async run multiple times and thus reduces the interval between
            // tasks.
            if (mAsyncHasBeenTriggered) {
                return;
            }
            mAsyncHasBeenTriggered = true;
            startAsyncRun();
        } else {
            // This lets us track the reason for a sync finish, especially relevant if we started
            // off asynchronously.
            mFinishCallSite = callSite;
            try (DualTraceEvent event =
                    DualTraceEvent.scoped("WebViewChromiumAwInit.startChromiumLockedSync")) {
                timedRunWithExceptionHandling(this::runSync);
            }
        }
    }

    /**
     * Continues running tasks in postBrowserProcessStartQueue. Often called inline, so post the
     * next task in order to maintain the gap between the previous task and the next task.
     */
    public void finishAsyncRun() {
        AwThreadUtils.postToUiThreadLooper(
                () ->
                        runAsyncStartupTaskAndPostNext(
                                mPreBrowserProcessStartTasksSize + 1,
                                mPostBrowserProcessStartQueue));
    }

    /**
     * Records metrics for tasks that were posted by BrowserStartupController since
     * StartupTasksRunner cannot account for them directly.
     */
    public void recordContentMetrics(BrowserStartupController.@Nullable StartupMetrics metrics) {
        assert metrics != null;
        mLongestUiBlockingTaskTimeMs =
                Math.max(mLongestUiBlockingTaskTimeMs, metrics.getLongestDurationOfPostedTasksMs());
        mTotalTimeTakenMs += metrics.getTotalDurationOfPostedTasksMs();
    }

    /** Returns the state in which the StartupTasksRunner is running (UNSET, SYNC, or ASYNC). */
    public int getRunState() {
        return mRunState;
    }

    private void runSync() {
        assert ThreadUtils.runningOnUiThread();

        // Avoid changing runState when there's no task to be run synchronously.
        if (mPreBrowserProcessStartQueue.isEmpty() && mPostBrowserProcessStartQueue.isEmpty()) {
            return;
        }

        mRunState = StartupRequestMode.SYNC;

        Runnable task = mPreBrowserProcessStartQueue.poll();
        while (task != null) {
            task.run();
            task = mPreBrowserProcessStartQueue.poll();
        }

        task = mPostBrowserProcessStartQueue.poll();
        while (task != null) {
            task.run();
            task = mPostBrowserProcessStartQueue.poll();
        }
    }

    private void startAsyncRun() {
        assert ThreadUtils.runningOnUiThread();
        runAsyncStartupTaskAndPostNext(/* taskNum= */ 1, mPreBrowserProcessStartQueue);
    }

    private void runAsyncStartupTaskAndPostNext(int taskNum, ArrayDeque<Runnable> queue) {
        assert ThreadUtils.runningOnUiThread();

        Runnable task = queue.poll();
        if (task == null) {
            return;
        }

        mRunState = StartupRequestMode.ASYNC;

        try (DualTraceEvent event =
                DualTraceEvent.scoped(
                        String.format(
                                Locale.US,
                                "WebViewChromiumAwInit.startChromiumLockedAsync_task%d/%d",
                                taskNum,
                                mNumTasks))) {
            timedRunWithExceptionHandling(task);
        }

        if (!queue.isEmpty()) { // Avoids unnecessarily posting to the UI thread
            AwThreadUtils.postToUiThreadLooper(
                    () -> runAsyncStartupTaskAndPostNext(taskNum + 1, queue));
        }
    }

    // Runs the startup task while keeping track of metrics and dealing with exceptions
    private void timedRunWithExceptionHandling(Runnable task) {
        assert ThreadUtils.runningOnUiThread();

        try {
            long startTimeMs = SystemClock.uptimeMillis();
            task.run();
            long durationMs = SystemClock.uptimeMillis() - startTimeMs;

            mLongestUiBlockingTaskTimeMs = Math.max(mLongestUiBlockingTaskTimeMs, durationMs);
            mTotalTimeTakenMs += durationMs;
            if (mPostBrowserProcessStartQueue.isEmpty()) {
                mDelegate.onStartupComplete(new StartupTimings(this));
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "WebView chromium startup failed", e);
            mDelegate.onStartupFailed(e);
            throw e;
        } catch (Error e) {
            Log.e(TAG, "WebView chromium startup failed", e);
            mDelegate.onStartupFailed(e);
            throw e;
        }
    }

    // To determine the startup mode, we track:
    // 1. Whether the initial startup request was synchronous or asynchronous.
    // 2. Whether the first task ran synchronously or asynchronously.
    // 3. Whether the last task ran synchronously or asynchronously.
    private @StartupMode int calculateStartupMode() {
        // TODO(abhijithnair): Evaluate if we need to consider the switch value here and remove if
        // not needed.
        if (!shouldRunStartupTasksAsync()) {
            return StartupMode.FULLY_SYNC;
        }

        if (mFirstTaskFromSynchronousCall) {
            return mChromiumFirstStartupRequestMode == StartupRequestMode.SYNC
                    ? StartupMode.FULLY_SYNC
                    : StartupMode.ASYNC_BUT_FULLY_SYNC;
        }
        return mRunState == StartupRequestMode.SYNC
                ? StartupMode.PARTIAL_ASYNC_THEN_SYNC
                : StartupMode.FULLY_ASYNC;
    }
}
