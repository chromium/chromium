// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;

import org.chromium.base.StrictModeContext;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Handles servicing of background WebAPK update requests coming via background_task_scheduler
 * component. Will update multiple WebAPKs if there are multiple WebAPKs pending update.
 */
public class WebApkUpdateTask extends NativeBackgroundTask {
    /** The WebappDataStorage for the WebAPK to update. */
    private WebappDataStorage mStorageToUpdate;

    /** Whether there are more WebAPKs to update than just {@link mStorageToUpdate}. */
    private boolean mMoreToUpdate;

    @Override
    @StartBeforeNativeResult
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.WEBAPK_UPDATE_JOB_ID;

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            WebappRegistry.warmUpSharedPrefs();
        }


        return StartBeforeNativeResult.DONE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, final TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.WEBAPK_UPDATE_JOB_ID;

        WebApkUpdateManager.updateWhileNotRunning(
                mStorageToUpdate, () -> callback.taskFinished(mMoreToUpdate));
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.WEBAPK_UPDATE_JOB_ID;

        // Native didn't complete loading, but it was supposed to. Presume that we need to
        // reschedule.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.WEBAPK_UPDATE_JOB_ID;

        // Updating a single WebAPK is a fire and forget task. However, there might be several
        // WebAPKs that we need to update.
        return true;
    }

    @Override
    public void reschedule(Context context) {}
}
