// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.nonembedded;

import android.app.job.JobParameters;
import android.app.job.JobService;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Background service that launches native component_updater::ComponentUpdateService and component
 * registration. It has to be launched via JobScheduler. This is a JobService rather just a Service
 * because the new restrictions introduced in Android O+ on background execution.
 */
@JNINamespace("android_webview")
public class AwComponentUpdateService extends JobService {
    private static final String TAG = "AwCUS";

    @Override
    public boolean onStartJob(JobParameters params) {
        // TODO(http://crbug.com/1179297) look at doing this in a task on a background thread
        // instead of the main thread.
        if (WebViewApkApplication.initializeNative()) {
            AwComponentUpdateServiceJni.get().startComponentUpdateService(
                    () -> { jobFinished(params, /* needReschedule= */ false); });
            return true;
        }
        Log.e(TAG, "couldn't init native, aborting starting AwComponentUpdaterService");
        return false;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        // This should only be called if the service needs to be shut down before we've called
        // jobFinished. Request reschedule so we can finish downloading component updates.
        return /*reschedule= */ true;
    }

    @NativeMethods
    interface Natives {
        void startComponentUpdateService(Runnable finishedCallback);
    }
}
