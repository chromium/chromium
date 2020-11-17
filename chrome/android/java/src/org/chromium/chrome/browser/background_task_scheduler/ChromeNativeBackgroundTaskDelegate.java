// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import org.chromium.base.Log;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.metrics.BackgroundTaskMemoryMetricsEmitter;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerExternalUma;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.NativeBackgroundTaskDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.Random;

/**
 * Chrome implementation of {@link NativeBackgroundTaskDelegate} that handles native initialization.
 */
public class ChromeNativeBackgroundTaskDelegate implements NativeBackgroundTaskDelegate {
    private static final String TAG = "BTS_NativeBkgrdTask";
    private static final int MAX_MEMORY_MEASUREMENT_DELAY_MS = 60 * 1000;

    @Override
    public void initializeNativeAsync(
            int taskId, boolean minimalBrowserMode, Runnable onSuccess, Runnable onFailure) {
        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, onSuccess);
                recordMemoryUsageWithRandomDelay(taskId, minimalBrowserMode);
            }
            @Override
            public boolean startMinimalBrowser() {
                return minimalBrowserMode;
            }
            @Override
            public void onStartupFailure(Exception failureCause) {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, onFailure);
            }
        };

        try {
            ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);

            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(
                    true /* isAsync */, parts);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Background Launch Error", e);
            onFailure.run();
        }
    }

    @Override
    public void recordMemoryUsageWithRandomDelay(int taskId, boolean minimalBrowserMode) {
        // The metrics are emitted only once so that the average does not converge towards the
        // memory usage after the task is executed, but reflects the task being executed.
        // The statistical distribution of the delay is uniform between 0s and 60s. The Offline
        // Prefetch background task, for example, starts downloads ~25s after the start of the task.
        final int delay = (int) (new Random().nextFloat() * MAX_MEMORY_MEASUREMENT_DELAY_MS);

        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
            BackgroundTaskMemoryMetricsEmitter.reportMemoryUsage(minimalBrowserMode,
                    BackgroundTaskSchedulerExternalUma.toMemoryHistogramAffixFromTaskId(taskId));
        }, delay);
    }

    @Override
    public BackgroundTaskSchedulerExternalUma getUmaReporter() {
        return BackgroundTaskSchedulerFactory.getUmaReporter();
    }
}
