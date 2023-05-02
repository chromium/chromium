// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** New implementation of {@link BookmarkQueryHandler} that expands the root. */
public class ImprovedBookmarkQueryHandler implements BookmarkQueryHandler {
    private static class NoDragWrappedBookmarkItem extends BookmarkItem {
        NoDragWrappedBookmarkItem(BookmarkItem item) {
            super(item.getId(), item.getTitle(), item.getUrl(), item.isFolder(), item.getParentId(),
                    item.isEditable(), item.isManaged(), item.getDateAdded(), item.isRead());
        }

        @Override
        public boolean isReorderable() {
            return false;
        }
    }

    private final BookmarkModel mBookmarkModel;
    private final BasicBookmarkQueryHandler mBasicBookmarkQueryHandler;

    /**
     * Constructs a handle that operates on the given backend.
     * @param bookmarkModel The backend that holds the truth of what the bookmark state looks like.
     */
    public ImprovedBookmarkQueryHandler(BookmarkModel bookmarkModel) {
        mBookmarkModel = bookmarkModel;
        mBasicBookmarkQueryHandler = new BasicBookmarkQueryHandler(bookmarkModel);
    }

    @Override
    public void destroy() {
        mBasicBookmarkQueryHandler.destroy();
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(BookmarkId parentId) {
        if (parentId.equals(mBookmarkModel.getRootFolderId())) {
            return buildBookmarkListForRootView();
        } else {
            return mBasicBookmarkQueryHandler.buildBookmarkListForParent(parentId);
        }
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(String query) {
        // TODO(https://crbug.com/1439584): Sort based on selection in toolbar.
        return mBasicBookmarkQueryHandler.buildBookmarkListForSearch(query);
    }

    private List<BookmarkListEntry> buildBookmarkListForRootView() {
        List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId topLevelId : BookmarkUtils.populateTopLevelFolders(mBookmarkModel)) {
            for (BookmarkId childId : mBookmarkModel.getChildIds(topLevelId)) {
                bookmarkListEntries.add(listEntryFromIdForRootView(childId));
            }
        }
        bookmarkListEntries.add(listEntryFromIdForRootView(mBookmarkModel.getReadingListFolder()));
        bookmarkListEntries.add(listEntryFromIdForRootView(mBookmarkModel.getDesktopFolderId()));
        Collections.sort(bookmarkListEntries, this::compareBookmarkListEntry);
        return bookmarkListEntries;
    }

    private BookmarkListEntry listEntryFromIdForRootView(BookmarkId bookmarkId) {
        PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        // Root view items are never re-orderable, as it's not a single folder.
        bookmarkItem = new NoDragWrappedBookmarkItem(bookmarkItem);
        return BookmarkListEntry.createBookmarkEntry(bookmarkItem, powerBookmarkMeta);
    }

    private int compareBookmarkListEntry(BookmarkListEntry entry1, BookmarkListEntry entry2) {
        BookmarkItem item1 = entry1.getBookmarkItem();
        BookmarkItem item2 = entry2.getBookmarkItem();

        // Sort folders before urls.
        int folderComparison = Boolean.compare(item2.isFolder(), item1.isFolder());
        if (folderComparison != 0) {
            return folderComparison;
        }

        // TODO(https://crbug.com/1439584): Sort based on selection in toolbar.
        int titleComparison = item1.getTitle().compareToIgnoreCase(item2.getTitle());
        if (titleComparison != 0) {
            return titleComparison;
        }

        // Fall back to id in case other fields tie. Order will be arbitrary but consistent.
        return Long.compare(item1.getId().getId(), item2.getId().getId());
    }
}
