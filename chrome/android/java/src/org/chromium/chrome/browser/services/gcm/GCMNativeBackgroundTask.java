// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Context;
import android.os.PersistableBundle;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMMessage;

/**
 * Processes jobs that have been scheduled for delivering GCM messages to the native GCM Driver,
 * processing for which may exceed the lifetime of the GcmListenerService.
 */
public class GCMNativeBackgroundTask extends NativeBackgroundTask {
    private static final String TAG = GCMNativeBackgroundTask.class.getSimpleName();
    private GCMMessage mMessage;

    @Override
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        PersistableBundle extras = taskParameters.getExtras();
        mMessage = GCMMessage.createFromPersistableBundle(extras);
        if (mMessage == null) {
            RecordHistogram.recordBooleanHistogram("GCM.MessageValid", false);
            Log.e(TAG, "The received bundle containing message data could not be validated.");
            return NativeBackgroundTask.StartBeforeNativeResult.DONE;
        } else {
            RecordHistogram.recordBooleanHistogram("GCM.MessageValid", true);
        }
        return NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE;
    }

    /**
     * Called when a GCM message is ready to be delivered to the GCM Driver consumer. Because we
     * don't yet know when a message has been fully processed, the task claims that processing has
     * been finished once the message is dispatched, and we hope that the system keeps us alive long
     * enough to actually finish processing.
     */
    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        GCMDriver.dispatchMessage(mMessage);
        callback.taskFinished(false);
    }

    /**
     * Called when the system has determined that processing the GCM message must be stopped.
     *
     * @return Boolean indicating whether the task has to be rescheduled.
     */
    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        // The GCM Driver has no mechanism for aborting previously dispatched messages.
        return false;
    }

    /**
     * Called when the system has determined that processing the GCM message must be stopped.
     *
     * @return Boolean indicating whether the task has to be rescheduled.
     */
    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        // The GCM Driver has no mechanism for aborting previously dispatched messages.
        return false;
    }
}
