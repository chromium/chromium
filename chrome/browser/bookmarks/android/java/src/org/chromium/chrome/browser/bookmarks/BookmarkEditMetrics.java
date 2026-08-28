// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics recording helper for bookmark edit flows. */
@NullMarked
public class BookmarkEditMetrics {
    // LINT.IfChange(BookmarkEditOutcome)
    @IntDef({
        BookmarkEditOutcome.SAVED,
        BookmarkEditOutcome.DELETED,
        BookmarkEditOutcome.CLOSED,
        BookmarkEditOutcome.DISMISSED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkEditOutcome {
        int SAVED = 0;
        int DELETED = 1;
        int CLOSED = 2;
        int DISMISSED = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkEditOutcome)

    public static void recordOutcome(@BookmarkEditOutcome int outcome, boolean isFolder) {
        String histogramName =
                isFolder
                        ? "Bookmarks.Edit.BookmarkFolder.Outcome"
                        : "Bookmarks.Edit.BookmarkItem.Outcome";
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, outcome, BookmarkEditOutcome.NUM_ENTRIES);
    }

    public static void recordFolderPickerOpened() {
        RecordUserAction.record("BookmarkEdit.FolderPickerOpened");
    }
}
