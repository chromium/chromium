// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;

/** Task for fetching password check status from GMSCore. */
public class SafetyHubFetchTask extends NativeBackgroundTask {
    @Override
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        // TODO(crbug.com/40254448): Remove use of getLastUsedRegularProfile here.
        SafetyHubFetchServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile())
                .fetchBreachedCredentialsCount(callback::taskFinished);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        // Reschedule task if native didn't complete loading, the call to GMSCore wouldn't have been
        // made at this point.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        // GMSCore has no mechanism to abort dispatched tasks.
        return false;
    }
}
