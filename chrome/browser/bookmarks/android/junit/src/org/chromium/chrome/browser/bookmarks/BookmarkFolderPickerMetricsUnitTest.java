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
import org.chromium.chrome.browser.bookmarks.BookmarkFolderPickerMetrics.BookmarkFolderPickerOutcome;

/** Unit tests for {@link BookmarkFolderPickerMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkFolderPickerMetricsUnitTest {
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
    public void testRecordOutcome_Moved() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.FolderPicker.Outcome", BookmarkFolderPickerOutcome.MOVED)
                        .build();

        BookmarkFolderPickerMetrics.recordOutcome(BookmarkFolderPickerOutcome.MOVED);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_Closed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.FolderPicker.Outcome",
                                BookmarkFolderPickerOutcome.CLOSED)
                        .build();

        BookmarkFolderPickerMetrics.recordOutcome(BookmarkFolderPickerOutcome.CLOSED);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOutcome_Dismissed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.FolderPicker.Outcome",
                                BookmarkFolderPickerOutcome.DISMISSED)
                        .build();

        BookmarkFolderPickerMetrics.recordOutcome(BookmarkFolderPickerOutcome.DISMISSED);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordCreateNewFolderOpened() {
        BookmarkFolderPickerMetrics.recordCreateNewFolderOpened();
        assertTrue(
                mUserActionTester
                        .getActions()
                        .contains("BookmarkFolderPicker.CreateNewFolderOpened"));
    }
}
