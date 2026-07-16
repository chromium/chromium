// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.List;

/** Interface handling context menu operations for the bookmarks bar. */
@NullMarked
public interface BookmarkBarContextMenuDelegate {
    void openInNewTab(BookmarkId id);

    void openInNewWindow(BookmarkId id);

    void openInIncognitoWindow(BookmarkId id);

    void openAll(List<BookmarkId> ids);

    void openAllInNewWindow(List<BookmarkId> ids);

    void openAllInIncognitoWindow(List<BookmarkId> ids);

    void openAllInNewTabGroup(List<BookmarkId> ids, @Nullable String title);

    void editBookmark(BookmarkId id);

    void moveBookmark(BookmarkId id);

    void deleteBookmark(BookmarkId id);

    void addPage(BookmarkId parentId);

    void addFolder(BookmarkId parentId);

    void openBookmarksManager(BookmarkId folderId);

    void toggleBookmarksBar();
}
