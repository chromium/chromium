// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.metrics.UmaUtils;

import java.util.LinkedList;
import java.util.Queue;

/**
 * Handler for application level tasks to be completed on deferred startup.
 */
public class DeferredStartupHandler {
    private static class Holder {
        @SuppressLint("StaticFieldLeak")
        private static final DeferredStartupHandler INSTANCE = new DeferredStartupHandler();
    }

    private boolean mDeferredStartupCompletedForApp;
    private long mDeferredStartupDuration;
    private long mMaxTaskDuration;
    private final Context mAppContext;

    private final Queue<Runnable> mDeferredTasks;

    /**
     * This class is an application specific object that handles the deferred startup.
     * @return The singleton instance of {@link DeferredStartupHandler}.
     */
    public static DeferredStartupHandler getInstance() {
        return sDeferredStartupHandler == null ? Holder.INSTANCE : sDeferredStartupHandler;
    }

    @VisibleForTesting
    public static void setInstanceForTests(DeferredStartupHandler handler) {
        sDeferredStartupHandler = handler;
    }

    @SuppressLint("StaticFieldLeak")
    private static DeferredStartupHandler sDeferredStartupHandler;

    protected DeferredStartupHandler() {
        mAppContext = ContextUtils.getApplicationContext();
        mDeferredTasks = new LinkedList<>();
    }

    /**
     * Add the idle handler which will run deferred startup tasks in sequence when idle. This can
     * be called multiple times by different activities to schedule their own deferred startup
     * tasks.
     */
    public void queueDeferredTasksOnIdleHandler() {
        mMaxTaskDuration = 0;
        mDeferredStartupDuration = 0;
        Looper.myQueue().addIdleHandler(() -> {
            Runnable currentTask = mDeferredTasks.poll();
            if (currentTask == null) {
                if (!mDeferredStartupCompletedForApp) {
                    mDeferredStartupCompletedForApp = true;
                    recordDeferredStartupStats();
                }
                return false;
            }

            long startTime = SystemClock.uptimeMillis();
            currentTask.run();
            long timeTaken = SystemClock.uptimeMillis() - startTime;

            mMaxTaskDuration = Math.max(mMaxTaskDuration, timeTaken);
            mDeferredStartupDuration += timeTaken;
            return true;
        });
    }

    private void recordDeferredStartupStats() {
        RecordHistogram.recordLongTimesHistogram(
                "UMA.Debug.EnableCrashUpload.DeferredStartUpDuration", mDeferredStartupDuration);
        RecordHistogram.recordLongTimesHistogram(
                "UMA.Debug.EnableCrashUpload.DeferredStartUpMaxTaskDuration", mMaxTaskDuration);
        if (UmaUtils.hasComeToForeground()) {
            RecordHistogram.recordLongTimesHistogram(
                    "UMA.Debug.EnableCrashUpload.DeferredStartUpCompleteTime",
                    SystemClock.uptimeMillis() - UmaUtils.getForegroundStartTicks());
        }
    }

    /**
     * Adds a single deferred task to the queue. The caller is responsible for calling
     * queueDeferredTasksOnIdleHandler after adding tasks.
     *
     * @param deferredTask The tasks to be run.
     */
    public void addDeferredTask(Runnable deferredTask) {
        ThreadUtils.assertOnUiThread();
        mDeferredTasks.add(deferredTask);
    }

    /**
     * @return Whether deferred startup has been completed.
     */
    @VisibleForTesting
    public boolean isDeferredStartupCompleteForApp() {
        return mDeferredStartupCompletedForApp;
    }
}
