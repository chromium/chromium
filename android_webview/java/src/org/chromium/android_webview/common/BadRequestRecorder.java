// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/**
 * A small wrapper around RecordHistogram to consolidate the webview bad request metric constants
 * in one location. This exists because this metric is reported from both the non embedded process
 * and the embedded process.
 */
public class BadRequestRecorder {
    private static final String HISTOGRAM_BAD_REQUEST = "Android.WebView.Metrics.BadRequest";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({Reason.EXCEPTION_WAITING_FOR_SERVICE, Reason.UNAVAILABLE_PLATFORM_API,
            Reason.UNKNOWN_PLATFORM_RESPONSE, Reason.COUNT})
    public @interface Reason {
        int EXCEPTION_WAITING_FOR_SERVICE = 0;
        int UNAVAILABLE_PLATFORM_API = 1;
        int UNKNOWN_PLATFORM_RESPONSE = 2;
        // Keep this one at the end and increment appropriately when adding new entries.
        int COUNT = 3;
    }

    public static void record(@Reason int reason) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM_BAD_REQUEST, reason, Reason.COUNT);
    }
}
