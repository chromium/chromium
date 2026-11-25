// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Records UMA metrics for the download toolbar button tray. */
@NullMarked
final class DownloadTrayMetrics {
    @VisibleForTesting static final String ACTION_HISTOGRAM = "Android.DownloadTray.Action";
    @VisibleForTesting static final String SHOWN_HISTOGRAM = "Android.DownloadTray.Shown";

    @VisibleForTesting
    static final String COMPLETION_POPUP_CLOSE_REASON_HISTOGRAM =
            "Android.DownloadTray.CompletionPopup.CloseReason";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(Action)
    @IntDef({
        Action.OPEN,
        Action.PAUSE,
        Action.RESUME,
        Action.RETRY,
        Action.CANCEL,
        Action.FULL_HISTORY,
        Action.CLOSE,
        Action.COUNT
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface Action {
        int OPEN = 0;
        int PAUSE = 1;
        int RESUME = 2;
        int RETRY = 3;
        int CANCEL = 4;
        int FULL_HISTORY = 5;
        int CLOSE = 6;
        int COUNT = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidDownloadTrayAction)

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(Surface)
    @IntDef({Surface.COMPLETION_POPUP, Surface.FULL_TRAY, Surface.COUNT})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface Surface {
        int COMPLETION_POPUP = 0;
        int FULL_TRAY = 1;
        int COUNT = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidDownloadTraySurface)

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(CompletionPopupCloseReason)
    @IntDef({
        CompletionPopupCloseReason.AUTO_DISMISSED,
        CompletionPopupCloseReason.ITEM_OPENED,
        CompletionPopupCloseReason.DISMISSED_OUTSIDE,
        CompletionPopupCloseReason.REPLACED_BY_FULL_TRAY,
        CompletionPopupCloseReason.ANCHOR_HIDDEN,
        CompletionPopupCloseReason.OTHER,
        CompletionPopupCloseReason.COUNT
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface CompletionPopupCloseReason {
        int AUTO_DISMISSED = 0;
        int ITEM_OPENED = 1;
        int DISMISSED_OUTSIDE = 2;
        int REPLACED_BY_FULL_TRAY = 3;
        int ANCHOR_HIDDEN = 4;
        int OTHER = 5;
        int COUNT = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidDownloadTrayCompletionPopupCloseReason)

    private DownloadTrayMetrics() {}

    static void recordAction(@Action int action) {
        RecordHistogram.recordEnumeratedHistogram(ACTION_HISTOGRAM, action, Action.COUNT);
    }

    static void recordShown(@Surface int surface) {
        RecordHistogram.recordEnumeratedHistogram(SHOWN_HISTOGRAM, surface, Surface.COUNT);
    }

    static void recordCompletionPopupCloseReason(@CompletionPopupCloseReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                COMPLETION_POPUP_CLOSE_REASON_HISTOGRAM, reason, CompletionPopupCloseReason.COUNT);
    }
}
