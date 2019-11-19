// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.os.Handler;

import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * An implementation of BackgroundTask that is responsible for resuming any in-progress downloads.
 * This class currently just starts the {@link DownloadNotificationService} or calls
 * {@link DownloadNotificationService}, which handles the actual resumption.
 */
public class DownloadResumptionBackgroundTask extends NativeBackgroundTask {
    // NativeBackgroundTask implementation.
    @Override
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, final TaskFinishedCallback callback) {
        DownloadResumptionScheduler.getDownloadResumptionScheduler().resume();
        new Handler().post(() -> callback.taskFinished(false));
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        // The task is not necessary once started, so no need to auto-resume here.  The started
        // service will handle rescheduling the job if necessary.
        return false;
    }

    @Override
    protected boolean supportsServiceManagerOnly() {
        return FeatureUtilities.isServiceManagerForDownloadResumptionEnabled();
    }

    @Override
    public void reschedule(Context context) {
        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();
    }
}
