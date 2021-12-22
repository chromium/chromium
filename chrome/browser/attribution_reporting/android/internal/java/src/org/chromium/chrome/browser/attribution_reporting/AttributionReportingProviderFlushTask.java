// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.ChainedTasks;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.List;

/**
 * Flushes the background Attributions from storage, reporting them to the AttributionReporter.
 */
public class AttributionReportingProviderFlushTask implements BackgroundTask {
    private BackgroundTask.TaskFinishedCallback mCallback;
    private ImpressionPersistentStore mPersistentStore;

    private static final String TASK_TRACE_NAME = "AttributionFlushTask";

    private static class FlushRunnable implements Runnable {
        private BrowserContextHandle mBrowserContext;
        private AttributionParameters mParams;

        public FlushRunnable(BrowserContextHandle browserContext, AttributionParameters params) {
            mBrowserContext = browserContext;
            mParams = params;
        }

        @Override
        public void run() {
            AttributionMetrics.recordAttributionEvent(
                    AttributionMetrics.AttributionEvent.REPORTED_POST_NATIVE, 1);
            reportImpression(mBrowserContext, AttributionReporter.getInstance(), mParams);
        }
    }

    public AttributionReportingProviderFlushTask() {
        this(new ImpressionPersistentStore<DataOutputStream, DataInputStream>(
                new ImpressionPersistentStoreFileManagerImpl()));
    }

    public AttributionReportingProviderFlushTask(ImpressionPersistentStore persistentStore) {
        mPersistentStore = persistentStore;
    }

    @MainThread
    @Override
    public boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        TraceEvent.startAsync(TASK_TRACE_NAME, 0);

        mCallback = callback;
        final boolean browserStarted =
                BrowserStartupController.getInstance().isFullBrowserStarted();
        if (!browserStarted) {
            BrowserStartupController.getInstance().startBrowserProcessesSync(
                    LibraryProcessType.PROCESS_BROWSER, false);
        }

        new AsyncTask<List<AttributionParameters>>() {
            @Override
            protected List<AttributionParameters> doInBackground() {
                return mPersistentStore.getAndClearStoredImpressions();
            }

            @Override
            protected void onPostExecute(List<AttributionParameters> result) {
                if (!browserStarted) {
                    // Browser wasn't started, so we don't have to worry about doing too much work
                    // on the UI thread.
                    flushSync(Profile.getLastUsedRegularProfile(), result);
                } else {
                    // Avoid doing too much contiguous work on the UI thread.
                    flushAsync(Profile.getLastUsedRegularProfile(), result);
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return true;
    }

    private void flushSync(
            BrowserContextHandle browserContext, List<AttributionParameters> paramsList) {
        AttributionReporter reporter = AttributionReporter.getInstance();
        for (AttributionParameters params : paramsList) {
            reportImpression(browserContext, reporter, params);
        }
        AttributionMetrics.recordAttributionEvent(
                AttributionMetrics.AttributionEvent.REPORTED_POST_NATIVE, paramsList.size());
        taskFinished();
    }

    private void flushAsync(
            BrowserContextHandle browserContext, List<AttributionParameters> paramsList) {
        // Use ChainedTasks to avoid doing to much work contiguously on the UI thread.
        ChainedTasks chainedTasks = new ChainedTasks();
        for (AttributionParameters params : paramsList) {
            // Note that we can't use BEST_EFFORT as the browser may have been started in the
            // background and BEST_EFFORT tasks won't get run.
            chainedTasks.add(UiThreadTaskTraits.DEFAULT, new FlushRunnable(browserContext, params));
        }
        chainedTasks.add(UiThreadTaskTraits.DEFAULT, () -> taskFinished());
        chainedTasks.start(false /* coalesceTasks */);
    }

    private static void reportImpression(BrowserContextHandle browserContext,
            AttributionReporter reporter, AttributionParameters params) {
        // TODO(https://crbug.com/1210171): Add metrics for how long reporting an impression takes
        // and consider batching impression reports for the async case.
        reporter.reportAppImpression(browserContext, params.getSourcePackageName(),
                params.getSourceEventId(), params.getDestination(), params.getReportTo(),
                params.getExpiry(), params.getEventTime());
    }

    private void taskFinished() {
        TraceEvent.finishAsync(TASK_TRACE_NAME, 0);
        mCallback.taskFinished(false);
    }

    @MainThread
    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // We shouldn't be getting stopped as we don't have any requirements for this task. In the
        // unlikely event we do get stopped the only way to preserve the Attributions would be to
        // write them back to disk, which is challenging as we would have to figure out which ones
        // have already been reported through ChainedTasks.
        // TODO(https://crbug.com/1210171): Add metrics for how many attributions are getting
        // dropped.
        return false;
    }

    @MainThread
    @Override
    public void reschedule(Context context) {}
}
