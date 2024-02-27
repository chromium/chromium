// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class that encapsulates {@link BookmarkBridge} and provides extra features such as undo, large
 * icon fetching, reader mode url redirecting, etc. This class should serve as the single class for
 * the UI to acquire data from the backend.
 */
public class BookmarkModel extends BookmarkBridge {
    private static BookmarkModel sInstanceForTesting;

    /** Set an instance for testing. */
    public static void setInstanceForTesting(BookmarkModel bookmarkModel) {
        sInstanceForTesting = bookmarkModel;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /**
     * Observer that listens to delete event. This interface is used by undo controllers to know
     * which bookmarks were deleted. Note this observer only listens to events that go through
     * bookmark model.
     */
    public interface BookmarkDeleteObserver {

        /**
         * Callback being triggered immediately before bookmarks are deleted.
         *
         * @param titles All titles of the bookmarks to be deleted.
         * @param isUndoable Whether the deletion is undoable.
         */
        void onDeleteBookmarks(String[] titles, boolean isUndoable);
    }

    private ObserverList<BookmarkDeleteObserver> mDeleteObservers = new ObserverList<>();

    /**
     * Provides an instance of the bookmark model for the provided profile.
     *
     * @param profile A profile for which the bookmark model is provided.
     * @return An instance of the bookmark model.
     */
    public static final BookmarkModel getForProfile(@NonNull Profile profile) {
        assert profile != null;
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        ThreadUtils.assertOnUiThread();
        return BookmarkBridge.getForProfile(profile);
    }

    BookmarkModel(long nativeBookmarkBridge) {
        super(nativeBookmarkBridge);
    }

    /**
     * Add an observer that listens to delete events that go through the bookmark model.
     *
     * @param observer The observer to add.
     */
    void addDeleteObserver(BookmarkDeleteObserver observer) {
        mDeleteObservers.addObserver(observer);
    }

    /**
     * Remove the observer from listening to bookmark deleting events.
     *
     * @param observer The observer to remove.
     */
    void removeDeleteObserver(BookmarkDeleteObserver observer) {
        mDeleteObservers.removeObserver(observer);
    }

    /**
     * Delete one or multiple bookmarks from model. If more than one bookmarks are passed here, this
     * method will group these delete operations into one undo bundle so that later if the user
     * clicks undo, all bookmarks deleted here will be restored.
     *
     * @param bookmarks Bookmarks to delete. Note this array should not contain a folder and its
     *     children, because deleting folder will also remove all its children, and deleting
     *     children once more will cause errors.
     */
    public void deleteBookmarks(BookmarkId... bookmarks) {
        assert bookmarks != null && bookmarks.length > 0;
        // Store all titles of bookmarks.
        List<String> titles = new ArrayList<>();
        boolean isUndoable = true;

        startGroupingUndos();
        for (BookmarkId bookmarkId : bookmarks) {
            BookmarkItem bookmarkItem = getBookmarkById(bookmarkId);
            if (bookmarkItem == null) continue;
            isUndoable &= (bookmarkId.getType() == BookmarkType.NORMAL);
            titles.add(bookmarkItem.getTitle());
            deleteBookmark(bookmarkId);
        }
        endGroupingUndos();

        for (BookmarkDeleteObserver observer : mDeleteObservers) {
            observer.onDeleteBookmarks(titles.toArray(new String[titles.size()]), isUndoable);
        }
    }

    /**
     * Calls {@link BookmarkBridge#moveBookmark(BookmarkId, BookmarkId, int)} for the given bookmark
     * list. The bookmarks are appended at the end.
     */
    public void moveBookmarks(List<BookmarkId> bookmarkIds, BookmarkId newParentId) {
        Set<BookmarkId> existingChildren = new HashSet<>(getChildIds(newParentId));
        int appendIndex = getChildCount(newParentId);
        for (BookmarkId child : bookmarkIds) {
            if (!existingChildren.contains(child)) {
                moveBookmark(child, newParentId, appendIndex++);
            }
        }
    }

    /**
     * @see org.chromium.chrome.browser.bookmarks.BookmarkItem#getTitle()
     */
    public String getBookmarkTitle(BookmarkId bookmarkId) {
        BookmarkItem bookmarkItem = getBookmarkById(bookmarkId);
        if (bookmarkItem == null) return "";
        return bookmarkItem.getTitle();
    }

    /**
     * @return The id of the default folder to view bookmarks.
     */
    public BookmarkId getDefaultFolderViewLocation() {
        return getRootFolderId();
    }

    /**
     * @param tab Tab whose current URL is checked against.
     * @return {@code true} if the current tab URL has a bookmark associated with it. If the
     *     bookmark backend is not loaded, return {@code false}.
     */
    public boolean hasBookmarkIdForTab(@Nullable Tab tab) {
        if (tab == null) return false;
        return isBookmarked(tab.getOriginalUrl());
    }

    /**
     * @param tab Tab whose current URL is checked against.
     * @return BookmarkId or {@link null} if bookmark backend is not loaded.
     */
    public @Nullable BookmarkId getUserBookmarkIdForTab(@Nullable Tab tab) {
        if (tab == null) return null;
        return getMostRecentlyAddedUserBookmarkIdForUrl(tab.getOriginalUrl());
    }
}
