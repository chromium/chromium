// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class BookmarkBarContextMenuMetrics {
    // LINT.IfChange(BookmarkBarContextMenuAction)
    @IntDef({
        BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB,
        BookmarkBarContextMenuAction.OPEN_IN_NEW_WINDOW,
        BookmarkBarContextMenuAction.OPEN_IN_INCOGNITO_WINDOW,
        BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB_GROUP,
        BookmarkBarContextMenuAction.EDIT,
        BookmarkBarContextMenuAction.MOVE,
        BookmarkBarContextMenuAction.DELETE,
        BookmarkBarContextMenuAction.ADD_PAGE,
        BookmarkBarContextMenuAction.ADD_FOLDER,
        BookmarkBarContextMenuAction.OPEN_BOOKMARKS_MANAGER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkBarContextMenuAction {
        int OPEN_IN_NEW_TAB = 0;
        int OPEN_IN_NEW_WINDOW = 1;
        int OPEN_IN_INCOGNITO_WINDOW = 2;
        int OPEN_IN_NEW_TAB_GROUP = 3;
        int EDIT = 4;
        int MOVE = 5;
        int DELETE = 6;
        int ADD_PAGE = 7;
        int ADD_FOLDER = 8;
        int OPEN_BOOKMARKS_MANAGER = 9;
        int NUM_ENTRIES = 10;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkBarContextMenuAction)

    // LINT.IfChange(BookmarkBarContextMenuEntrypoint)
    @IntDef({
        BookmarkBarContextMenuEntrypoint.EMPTY_SPACE,
        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER,
        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM,
        BookmarkBarContextMenuEntrypoint.POPUP_FOLDER,
        BookmarkBarContextMenuEntrypoint.POPUP_ITEM
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkBarContextMenuEntrypoint {
        int EMPTY_SPACE = 0;
        int BOOKMARK_BAR_FOLDER = 1;
        int BOOKMARK_BAR_ITEM = 2;
        int POPUP_FOLDER = 3;
        int POPUP_ITEM = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/histograms.xml:BookmarkBarContextMenuEntrypoint)

    // LINT.IfChange(BookmarkBarContextMenuGesture)
    @IntDef({BookmarkBarContextMenuGesture.RIGHT_CLICK, BookmarkBarContextMenuGesture.LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkBarContextMenuGesture {
        int RIGHT_CLICK = 0;
        int LONG_PRESS = 1;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/bookmarks/histograms.xml:BookmarkBarContextMenuGesture)

    public static String getEntrypointString(@BookmarkBarContextMenuEntrypoint int entrypoint) {
        switch (entrypoint) {
            case BookmarkBarContextMenuEntrypoint.EMPTY_SPACE:
                return "EmptySpace";
            case BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER:
                return "BookmarkBarFolder";
            case BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM:
                return "BookmarkBarItem";
            case BookmarkBarContextMenuEntrypoint.POPUP_FOLDER:
                return "PopupFolder";
            case BookmarkBarContextMenuEntrypoint.POPUP_ITEM:
                return "PopupItem";
        }
        assert false : "Unknown entrypoint";
        return "Unknown";
    }

    public static String getGestureString(@BookmarkBarContextMenuGesture int gesture) {
        switch (gesture) {
            case BookmarkBarContextMenuGesture.RIGHT_CLICK:
                return "RightClick";
            case BookmarkBarContextMenuGesture.LONG_PRESS:
                return "LongPress";
        }
        assert false : "Unknown gesture";
        return "Unknown";
    }

    public static void recordOpened(
            @BookmarkBarContextMenuEntrypoint int entrypoint,
            @BookmarkBarContextMenuGesture int gesture) {
        String slicedOpened =
                String.join(
                        ".",
                        "Bookmarks.BookmarkBar.ContextMenu",
                        getEntrypointString(entrypoint),
                        getGestureString(gesture),
                        "Opened");

        // Granular sliced metric
        RecordHistogram.recordBooleanHistogram(slicedOpened, true);
    }

    public static void recordAction(
            @BookmarkBarContextMenuEntrypoint int entrypoint,
            @BookmarkBarContextMenuAction int action) {
        String slicedAction =
                String.join(
                        ".",
                        "Bookmarks.BookmarkBar.ContextMenu",
                        getEntrypointString(entrypoint),
                        "Action");

        // Granular sliced metric
        RecordHistogram.recordEnumeratedHistogram(
                slicedAction, action, BookmarkBarContextMenuAction.NUM_ENTRIES);
    }
}
