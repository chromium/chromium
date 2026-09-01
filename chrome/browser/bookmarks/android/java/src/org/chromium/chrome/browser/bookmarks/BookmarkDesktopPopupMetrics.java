// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics recording helper for the Android desktop bookmark popup. */
@NullMarked
public class BookmarkDesktopPopupMetrics {
    // LINT.IfChange(BookmarkDesktopPopupOutcome)
    @IntDef({
        BookmarkDesktopPopupOutcome.SAVED,
        BookmarkDesktopPopupOutcome.REMOVED,
        BookmarkDesktopPopupOutcome.EDIT_DIALOG_OPENED,
        BookmarkDesktopPopupOutcome.DISMISSED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkDesktopPopupOutcome {
        int SAVED = 0;
        int REMOVED = 1;
        int EDIT_DIALOG_OPENED = 2;
        int DISMISSED = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkDesktopPopupOutcome)

    public static void recordOutcome(
            @BookmarkDesktopPopupOutcome int outcome, boolean isNewBookmark) {
        String mode = isNewBookmark ? "Add" : "Edit";
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.DesktopPopup." + mode + ".Outcome",
                outcome,
                BookmarkDesktopPopupOutcome.NUM_ENTRIES);
    }
}
