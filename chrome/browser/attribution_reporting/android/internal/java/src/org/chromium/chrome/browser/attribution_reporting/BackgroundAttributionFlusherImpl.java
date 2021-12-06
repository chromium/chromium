// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import org.chromium.components.background_task_scheduler.BackgroundTask;

/**
 * Handles flushing of background attributions that arrived before the native library was loaded.
 */
/* package */ class BackgroundAttributionFlusherImpl {
    /**
     * Flush the Attributions that arrived in the background and were cached before the native
     * library was loaded.
     */
    public static void flushPreNativeAttributions(Runnable callback) {
        // Re-use the AttributionReportingProviderFlushTask to perform the flushing.
        new AttributionReportingProviderFlushTask().onStartTask(
                null, null, new BackgroundTask.TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        callback.run();
                    }
                });
    }
}
