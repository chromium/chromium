// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.build.annotations.NullMarked;

/** UMA/Histogram recorder for in-app history. */
@NullMarked
public class AppHistoryUmaRecorder extends HistoryUmaRecorder {
    private static final String APP_METRICS_PREFIX = "Android.AppHistoryPage.";

    @Override
    public String getPrefix() {
        return APP_METRICS_PREFIX;
    }

    @Override
    public void recordOpenFullHistory() {
        recordUserAction("OpenFullHistory");
    }

    @Override
    public void recordClearBrowsingData() {
        // Clear data is disabled for in-app history for now.
    }

    @Override
    public void recordQueryAppDuration(long timeMs) {
        // Querying app is not performed for in-app history.
    }
}
