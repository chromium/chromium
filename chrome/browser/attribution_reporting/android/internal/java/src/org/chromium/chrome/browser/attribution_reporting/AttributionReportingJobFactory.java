// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import org.chromium.components.background_task_scheduler.BackgroundTask;

/**
 * Factory for creating instances of BackgroundTasks for the Attribution Reporting module.
 */
public class AttributionReportingJobFactory {
    /**
     * @return an AttributionReportingProviderFlushTask instance.
     */
    public static BackgroundTask getAttributionReportingProviderFlushTask() {
        return new AttributionReportingProviderFlushTask();
    }
}
