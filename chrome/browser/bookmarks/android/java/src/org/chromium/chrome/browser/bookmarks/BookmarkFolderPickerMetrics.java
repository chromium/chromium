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

/** Metrics recording helper for bookmark folder picker / move-to flows. */
@NullMarked
public class BookmarkFolderPickerMetrics {
    private BookmarkFolderPickerMetrics() {}

    // LINT.IfChange(BookmarkFolderPickerOutcome)
    @IntDef({
        BookmarkFolderPickerOutcome.MOVED,
        BookmarkFolderPickerOutcome.CLOSED,
        BookmarkFolderPickerOutcome.DISMISSED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkFolderPickerOutcome {
        int MOVED = 0;
        int CLOSED = 1;
        int DISMISSED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkFolderPickerOutcome)

    public static void recordOutcome(@BookmarkFolderPickerOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                "Bookmarks.FolderPicker.Outcome", outcome, BookmarkFolderPickerOutcome.NUM_ENTRIES);
    }

    public static void recordCreateNewFolderOpened() {
        RecordUserAction.record("BookmarkFolderPicker.CreateNewFolderOpened");
    }
}
