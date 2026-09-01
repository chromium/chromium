// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkDesktopPopupMetrics.BookmarkDesktopPopupOutcome;

/** Unit tests for {@link BookmarkDesktopPopupMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkDesktopPopupMetricsUnitTest {

    @Test
    @SmallTest
    public void testRecordOutcome_Add_Saved() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Add.Outcome",
                                BookmarkDesktopPopupOutcome.SAVED)
                        .build();

        BookmarkDesktopPopupMetrics.recordOutcome(
                BookmarkDesktopPopupOutcome.SAVED, /* isNewBookmark= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_Add_Removed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Add.Outcome",
                                BookmarkDesktopPopupOutcome.REMOVED)
                        .build();

        BookmarkDesktopPopupMetrics.recordOutcome(
                BookmarkDesktopPopupOutcome.REMOVED, /* isNewBookmark= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_Add_EditDialogOpened() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Add.Outcome",
                                BookmarkDesktopPopupOutcome.EDIT_DIALOG_OPENED)
                        .build();

        BookmarkDesktopPopupMetrics.recordOutcome(
                BookmarkDesktopPopupOutcome.EDIT_DIALOG_OPENED, /* isNewBookmark= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_Edit_Saved() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Edit.Outcome",
                                BookmarkDesktopPopupOutcome.SAVED)
                        .build();

        BookmarkDesktopPopupMetrics.recordOutcome(
                BookmarkDesktopPopupOutcome.SAVED, /* isNewBookmark= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_Edit_Dismissed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.DesktopPopup.Edit.Outcome",
                                BookmarkDesktopPopupOutcome.DISMISSED)
                        .build();

        BookmarkDesktopPopupMetrics.recordOutcome(
                BookmarkDesktopPopupOutcome.DISMISSED, /* isNewBookmark= */ false);

        histogramWatcher.assertExpected();
    }
}
