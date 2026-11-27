// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Records UMA metrics for the download toolbar button. */
@NullMarked
final class DownloadToolbarButtonMetrics {
    @VisibleForTesting
    static final String CLICKED_HISTOGRAM = "Android.DownloadToolbarButton.Clicked";

    @VisibleForTesting static final String SHOWN_HISTOGRAM = "Android.DownloadToolbarButton.Shown";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(ButtonState)
    @IntDef({
        ButtonState.IN_PROGRESS,
        ButtonState.COMPLETE_ACTIVE,
        ButtonState.COMPLETE_INACTIVE,
        ButtonState.COUNT
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface ButtonState {
        int IN_PROGRESS = 0;
        int COMPLETE_ACTIVE = 1;
        int COMPLETE_INACTIVE = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidDownloadToolbarButtonState)

    private DownloadToolbarButtonMetrics() {}

    static void recordClicked(@ButtonState int state) {
        RecordHistogram.recordEnumeratedHistogram(CLICKED_HISTOGRAM, state, ButtonState.COUNT);
    }

    static void recordShown() {
        RecordHistogram.recordBooleanHistogram(SHOWN_HISTOGRAM, true);
    }
}
