// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A helper class to log metrics. */
public class ScreenshotShareSheetMetrics {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ScreenshotShareSheetAction.EDIT,
        ScreenshotShareSheetAction.SHARE,
        ScreenshotShareSheetAction.SAVE,
        ScreenshotShareSheetAction.DELETE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenshotShareSheetAction {
        int EDIT = 0;
        int SHARE = 1;
        int SAVE = 2;
        int DELETE = 3;
        int COUNT = 4;
    };

    /**
     * A helper function to log histograms for the image editor.
     * @param action the action to be logged.
     */
    public static void logScreenshotAction(@ScreenshotShareSheetAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.ScreenshotFallback.Action", action, ScreenshotShareSheetAction.COUNT);
    }
}
