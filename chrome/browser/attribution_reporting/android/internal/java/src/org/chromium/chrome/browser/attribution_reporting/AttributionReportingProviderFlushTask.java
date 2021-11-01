// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * TODO(mthiesse): Implement this.
 */
public class AttributionReportingProviderFlushTask implements BackgroundTask {
    @MainThread
    @Override
    public boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        return false;
    }

    @MainThread
    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        return false;
    }

    @MainThread
    @Override
    public void reschedule(Context context) {}
}
