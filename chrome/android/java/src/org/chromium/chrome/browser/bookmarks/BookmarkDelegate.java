// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;

/**
 * Interface used by UI components in the main bookmarks UI to broadcast UI change notifications and
 * get bookmark data model.
 */
public interface BookmarkDelegate {
    /**
     * Returns whether the bookmarks UI will be shown in a dialog, instead of a NativePage. This is
     * typically true on phones and false on tablets, but not always, e.g. in multi-window mode or
     * after upgrading to the new bookmarks.
     */
    boolean isDialogUi();

    /**
     * Shows bookmarks contained in the specified folder.
     *
     * @param folder Parent folder that contains bookmarks to show as its children.
     */
    void openFolder(BookmarkId folder);

    /**
     * @return The {@link SelectionDelegate} responsible for tracking selected bookmarks.
     */
    SelectionDelegate<BookmarkId> getSelectionDelegate();

    /**
     * @return The {@link SelectableListLayout} displaying the list of bookmarks.
     */
    SelectableListLayout<BookmarkId> getSelectableListLayout();

    /**
     * Notifies the current mode set event to the given observer. For example, if the current mode
     * is MODE_ALL_BOOKMARKS, it calls onAllBookmarksModeSet.
     */
    void notifyStateChange(BookmarkUiObserver observer);

    /**
     * Closes the Bookmark UI (if on phone) and opens the given bookmark.
     *
     * @param bookmark Bookmark to open.
     */
    void openBookmark(BookmarkId bookmark);

    /**
     * Closes the Bookmark UI (if on phone) and opens the given list of bookmarks in new tabs.
     *
     * @param bookmark Bookmarks to open.
     * @param incognito Whether the bookmarks should be opened in an incognito tab.
     */
    void openBookmarksInNewTabs(List<BookmarkId> bookmark, boolean incognito);

    /** Shows the search UI. */
    void openSearchUi();

    /** Add an observer to bookmark UI changes. */
    void addUiObserver(BookmarkUiObserver observer);

    /** Remove an observer of bookmark UI changes. */
    void removeUiObserver(BookmarkUiObserver observer);

    /**
     * @return Bookmark data model associated with this UI.
     */
    BookmarkModel getModel();

    /**
     * Returns current mode of bookmark main UI. If no mode is stored, {@link
     * BookmarkUiMode.LOADING} is returned.
     */
    @BookmarkUiMode
    int getCurrentUiMode();

    /**
     * @return LargeIconBridge instance. By sharing the instance, we can also share the cache.
     */
    LargeIconBridge getLargeIconBridge();

    /**
     * @return The drag state delegate that is associated with this list of bookmarks.
     */
    DragStateDelegate getDragStateDelegate();

    /**
     * Move a bookmark one position down within its folder.
     *
     * @param bookmarkId The bookmark to move.
     */
    void moveDownOne(BookmarkId bookmarkId);

    /**
     * Move a bookmark one position up within its folder.
     *
     * @param bookmarkId The bookmark to move.
     */
    void moveUpOne(BookmarkId bookmarkId);

    /** Notified when the menu is opened for a bookmark row displayed in the UI. */
    void onBookmarkItemMenuOpened();

    /**
     * Scroll the bookmarks list such that bookmarkId is shown in the view, and highlight it.
     *
     * @param bookmarkId The BookmarkId of the bookmark of interest.
     */
    void highlightBookmark(BookmarkId bookmarkId);
}
