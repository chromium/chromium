// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.bookmarks.BookmarkEditMetrics.BookmarkEditOutcome;

/** Unit tests for {@link BookmarkEditMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkEditMetricsUnitTest {
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkItem_Saved() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.SAVED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.SAVED, /* isFolder= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkItem_Deleted() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.DELETED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.DELETED, /* isFolder= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkItem_Closed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome", BookmarkEditOutcome.CLOSED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.CLOSED, /* isFolder= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkItem_Dismissed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkItem.Outcome",
                                BookmarkEditOutcome.DISMISSED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.DISMISSED, /* isFolder= */ false);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkFolder_Saved() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkFolder.Outcome", BookmarkEditOutcome.SAVED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.SAVED, /* isFolder= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkFolder_Deleted() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkFolder.Outcome",
                                BookmarkEditOutcome.DELETED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.DELETED, /* isFolder= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkFolder_Closed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkFolder.Outcome", BookmarkEditOutcome.CLOSED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.CLOSED, /* isFolder= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_BookmarkFolder_Dismissed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.Edit.BookmarkFolder.Outcome",
                                BookmarkEditOutcome.DISMISSED)
                        .build();

        BookmarkEditMetrics.recordOutcome(BookmarkEditOutcome.DISMISSED, /* isFolder= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFolderPickerOpened() {
        BookmarkEditMetrics.recordFolderPickerOpened();
        assertTrue(mUserActionTester.getActions().contains("BookmarkEdit.FolderPickerOpened"));
    }
}
