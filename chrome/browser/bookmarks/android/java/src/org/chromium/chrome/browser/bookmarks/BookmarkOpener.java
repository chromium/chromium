// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.List;

/** Consolidates logic about opening bookmarks. */
@NullMarked
public interface BookmarkOpener {
    /**
     * Open the given id in the current tab.
     *
     * @param incognito Whether the bookmark should be opened in incognito mode.
     * @return Whether the bookmark id was successfully opened.
     */
    boolean openBookmarkInCurrentTab(BookmarkId id, boolean incognito);

    /**
     * Open the given bookmarkIds in new tabs.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @return Whether the bookmark ids were successfully opened.
     */
    default boolean openBookmarksInNewTabs(List<BookmarkId> bookmarkIds, boolean incognito) {
        return openBookmarksInNewTabs(bookmarkIds, incognito, /* tabLaunchType= */ null);
    }

    /**
     * Open the given bookmarkIds in new tabs.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @param tabLaunchType The launch type to use when creating new tabs.
     * @return Whether the bookmark ids were successfully opened.
     */
    default boolean openBookmarksInNewTabs(
            List<BookmarkId> bookmarkIds,
            boolean incognito,
            @Nullable @TabLaunchType Integer tabLaunchType) {
        return openBookmarksInNewTabs(bookmarkIds, incognito, tabLaunchType, /* extras= */ null);
    }

    /**
     * Open the given bookmarkIds in new tabs.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @param tabLaunchType The launch type to use when creating new tabs.
     * @param extras Extras to put in the launch intent, can be null.
     * @return Whether the bookmark ids were successfully opened.
     */
    boolean openBookmarksInNewTabs(
            List<BookmarkId> bookmarkIds,
            boolean incognito,
            @Nullable @TabLaunchType Integer tabLaunchType,
            @Nullable Bundle extras);

    /**
     * Open the given bookmarkIds in a new window.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @return Whether the bookmark ids were successfully opened.
     */
    default boolean openBookmarksInNewWindow(List<BookmarkId> bookmarkIds, boolean incognito) {
        return openBookmarksInNewWindow(bookmarkIds, incognito, /* extras= */ null);
    }

    boolean openBookmarksInNewWindow(
            List<BookmarkId> bookmarkIds, boolean incognito, @Nullable Bundle extras);

    /**
     * @return Whether opening bookmarks in a new window is supported.
     */
    boolean isOpenInNewWindowSupported();

    /**
     * Open the given bookmarkIds in new tabs in a new tab group with an optional title.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @param title The title of the tab group, can be null.
     * @return Whether the bookmark ids were successfully opened.
     */
    boolean openBookmarksInNewTabGroup(
            List<BookmarkId> bookmarkIds, boolean incognito, @Nullable String title);

    /**
     * Open the given folder's children in new tabs. Non-bookmark items like folders are ignored.
     *
     * @param folderId The folder id to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @param tabLaunchType The launch type to use when creating new tabs.
     * @return Whether the folder's bookmarks were successfully opened.
     */
    boolean openFolderBookmarksInNewTabs(
            BookmarkId folderId, boolean incognito, @Nullable @TabLaunchType Integer tabLaunchType);
}
