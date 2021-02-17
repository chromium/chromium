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
            AwComponentUpdateServiceJni.get().maybeStartComponentUpdateService();
            return true;
        }
        Log.e(TAG, "couldn't init native, aborting starting AwComponentUpdaterService");
        return false;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        // TODO(crbug.com/1177393) call jobFinished when component_updater::ComponentUpdateService
        // finishes checking for updates. We are leaving scheduling of the recurring update job for
        // the native service for now. Always return true for now because we don't know if the
        // native update service has finished or not.
        return true;
    }

    @NativeMethods
    interface Natives {
        void maybeStartComponentUpdateService();
    }
}
