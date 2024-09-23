// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;

import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabLocator;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.webapps.WebappsUtils;

import java.lang.ref.WeakReference;
import java.util.List;

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
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.WEBAPK_UPDATE_JOB_ID;

        WebappRegistry.warmUpSharedPrefs();

        WebappsUtils.prepareIsRequestPinShortcutSupported();

        List<String> ids = WebappRegistry.getInstance().findWebApksWithPendingUpdate();
        for (String id : ids) {
            WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(id);
            WeakReference<BaseCustomTabActivity> activity =
                    CustomTabLocator.findRunningWebappActivityWithId(storage.getId());
            if (activity == null || activity.get() == null) {
                mStorageToUpdate = storage;
                mMoreToUpdate = ids.size() > 1;
                return StartBeforeNativeResult.LOAD_NATIVE;
            }
        }
        return ids.isEmpty() ? StartBeforeNativeResult.DONE : StartBeforeNativeResult.RESCHEDULE;
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
}
