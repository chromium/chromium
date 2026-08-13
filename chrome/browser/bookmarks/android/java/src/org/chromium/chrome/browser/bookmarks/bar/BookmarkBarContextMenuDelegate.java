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
    /**
     * Opens a single bookmark in a new background tab.
     *
     * @param id The {@link BookmarkId} of the bookmark to open.
     */
    void openInNewTab(BookmarkId id);

    /**
     * Opens a single bookmark in a new regular window.
     *
     * @param id The {@link BookmarkId} of the bookmark to open.
     */
    void openInNewWindow(BookmarkId id);

    /**
     * Opens a single bookmark in a new incognito window.
     *
     * @param id The {@link BookmarkId} of the bookmark to open.
     */
    void openInIncognitoWindow(BookmarkId id);

    /**
     * Opens a list of bookmarks in new background tabs.
     *
     * @param ids The list of {@link BookmarkId}s to open.
     */
    void openBookmarksInNewTabs(List<BookmarkId> ids);

    /**
     * Opens a list of bookmarks in a new regular window.
     *
     * @param ids The list of {@link BookmarkId}s to open.
     */
    void openBookmarksInNewWindow(List<BookmarkId> ids);

    /**
     * Opens a list of bookmarks in a new incognito window.
     *
     * @param ids The list of {@link BookmarkId}s to open.
     */
    void openBookmarksInIncognitoWindow(List<BookmarkId> ids);

    /**
     * Opens a list of bookmarks in a new tab group with an optional group title.
     *
     * @param ids The list of {@link BookmarkId}s to open.
     * @param title The optional title to assign to the new tab group.
     */
    void openBookmarksInNewTabGroup(List<BookmarkId> ids, @Nullable String title);

    /**
     * Opens all non-folder bookmarks within the specified folder in new background tabs.
     *
     * @param folderId The {@link BookmarkId} of the folder whose bookmarks should be opened.
     */
    void openFolderInNewTabs(BookmarkId folderId);

    /**
     * Opens all non-folder bookmarks within the specified folder in a new regular window.
     *
     * @param folderId The {@link BookmarkId} of the folder whose bookmarks should be opened.
     */
    void openFolderInNewWindow(BookmarkId folderId);

    /**
     * Opens all non-folder bookmarks within the specified folder in a new incognito window.
     *
     * @param folderId The {@link BookmarkId} of the folder whose bookmarks should be opened.
     */
    void openFolderInIncognitoWindow(BookmarkId folderId);

    /**
     * Opens all non-folder bookmarks within the specified folder in a new tab group with an
     * optional group title.
     *
     * @param folderId The {@link BookmarkId} of the folder whose bookmarks should be opened.
     * @param title The optional title to assign to the new tab group.
     */
    void openFolderInNewTabGroup(BookmarkId folderId, @Nullable String title);

    /**
     * Starts the activity/dialog to edit the specified bookmark.
     *
     * @param id The {@link BookmarkId} of the bookmark to edit.
     */
    void editBookmark(BookmarkId id);

    /**
     * Starts the folder picker activity/dialog to move the specified bookmark.
     *
     * @param id The {@link BookmarkId} of the bookmark to move.
     */
    void moveBookmark(BookmarkId id);

    /**
     * Deletes the specified bookmark.
     *
     * @param id The {@link BookmarkId} of the bookmark to delete.
     */
    void deleteBookmark(BookmarkId id);

    /**
     * Adds the current active tab as a bookmark inside the specified parent folder.
     *
     * @param parentId The {@link BookmarkId} of the parent folder.
     */
    void addPage(BookmarkId parentId);

    /**
     * Opens the dialog to add a new folder inside the specified parent folder.
     *
     * @param parentId The {@link BookmarkId} of the parent folder.
     */
    void addFolder(BookmarkId parentId);

    /**
     * Opens the Bookmark Manager navigated to the specified folder.
     *
     * @param folderId The {@link BookmarkId} of the folder to navigate to.
     */
    void openBookmarksManager(BookmarkId folderId);

    /** Toggles the visibility of the bookmarks bar. */
    void toggleBookmarksBar();

    /** Sets the visibility state of the bookmarks bar to always hide. */
    void setBookmarksBarVisibilityToAlwaysHide();

    /** Sets the visibility state of the bookmarks bar to always show. */
    void setBookmarksBarVisibilityToAlwaysShow();

    /** Sets the visibility state of the bookmarks bar to only show on the NTP. */
    void setBookmarksBarVisibilityToOnlyShowOnNTP();
}
