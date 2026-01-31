// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/** Records metrics for the Incognito New Tab Page. */
@NullMarked
public class IncognitoNtpMetrics {
    public static final String HISTOGRAM_TIME_TO_FIRST_NAVIGATION =
            "NewTabPage.Incognito.TimeToFirstNavigation";
    private long mNtpLoadedTimeMs;

    public void markNtpLoaded() {
        mNtpLoadedTimeMs = SystemClock.elapsedRealtime();
    }

    public void recordNavigatedAway() {
        if (mNtpLoadedTimeMs == 0) {
            return;
        }

        long timeSpentMs = SystemClock.elapsedRealtime() - mNtpLoadedTimeMs;
        recordTimeToFirstNavigation(timeSpentMs);
        mNtpLoadedTimeMs = 0;
    }

    private void recordTimeToFirstNavigation(long durationMs) {
        RecordHistogram.recordLongTimesHistogram100(HISTOGRAM_TIME_TO_FIRST_NAVIGATION, durationMs);
    }
}
