// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuAction;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuEntrypoint;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuGesture;

@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarContextMenuMetricsTest {

    @Test
    @SmallTest
    public void testGetEntrypointString() {
        assertEquals(
                "EmptySpace",
                BookmarkBarContextMenuMetrics.getEntrypointString(
                        BookmarkBarContextMenuEntrypoint.EMPTY_SPACE));
        assertEquals(
                "BookmarkBarFolder",
                BookmarkBarContextMenuMetrics.getEntrypointString(
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER));
        assertEquals(
                "BookmarkBarItem",
                BookmarkBarContextMenuMetrics.getEntrypointString(
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM));
        assertEquals(
                "PopupFolder",
                BookmarkBarContextMenuMetrics.getEntrypointString(
                        BookmarkBarContextMenuEntrypoint.POPUP_FOLDER));
        assertEquals(
                "PopupItem",
                BookmarkBarContextMenuMetrics.getEntrypointString(
                        BookmarkBarContextMenuEntrypoint.POPUP_ITEM));
    }

    @Test
    @SmallTest
    public void testGetGestureString() {
        assertEquals(
                "RightClick",
                BookmarkBarContextMenuMetrics.getGestureString(
                        BookmarkBarContextMenuGesture.RIGHT_CLICK));
        assertEquals(
                "LongPress",
                BookmarkBarContextMenuMetrics.getGestureString(
                        BookmarkBarContextMenuGesture.LONG_PRESS));
    }

    @Test
    @SmallTest
    public void testRecordOpened() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarItem"
                                        + ".LongPress.Opened",
                                true)
                        .build();

        BookmarkBarContextMenuMetrics.recordOpened(
                BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM,
                BookmarkBarContextMenuGesture.LONG_PRESS);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordAction() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarFolder.Action",
                                BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB_GROUP)
                        .build();

        BookmarkBarContextMenuMetrics.recordAction(
                BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER,
                BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB_GROUP);

        histogramWatcher.assertExpected();
    }
}
