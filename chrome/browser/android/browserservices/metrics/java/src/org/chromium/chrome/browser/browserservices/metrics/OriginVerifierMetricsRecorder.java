// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.metrics;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Class to record Uma related to Origin Verifier */
public class OriginVerifierMetricsRecorder {
    @IntDef({
        VerificationResult.ONLINE_SUCCESS,
        VerificationResult.ONLINE_FAILURE,
        VerificationResult.OFFLINE_SUCCESS,
        VerificationResult.OFFLINE_FAILURE,
        VerificationResult.HTTPS_FAILURE,
        VerificationResult.REQUEST_FAILURE,
        VerificationResult.CACHED_SUCCESS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerificationResult {
        // Don't reuse values or reorder values. If you add something new, change NUM_ENTRIES as
        // well.
        int ONLINE_SUCCESS = 0;
        int ONLINE_FAILURE = 1;
        int OFFLINE_SUCCESS = 2;
        int OFFLINE_FAILURE = 3;
        int HTTPS_FAILURE = 4;
        int REQUEST_FAILURE = 5;
        int CACHED_SUCCESS = 6;
        int NUM_ENTRIES = 7;
    }

    public static void recordVerificationResult(@VerificationResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "BrowserServices.VerificationResult", result, VerificationResult.NUM_ENTRIES);
    }

    public static void recordVerificationTime(long duration, boolean online) {
        if (online) {
            RecordHistogram.recordTimesHistogram(
                    "BrowserServices.VerificationTime.Online", duration);
        } else {
            RecordHistogram.recordTimesHistogram(
                    "BrowserServices.VerificationTime.Offline", duration);
        }
    }

    private OriginVerifierMetricsRecorder() {}
}
