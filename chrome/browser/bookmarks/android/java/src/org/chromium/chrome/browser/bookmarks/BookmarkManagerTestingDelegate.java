// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.bookmarks.BookmarkId;

/** Exposes business logic methods to the various bookmark integration. */
@NullMarked
public interface BookmarkManagerTestingDelegate {
    /**
     * Returns the bookmark id by the position of the bookmark. Ignores other item types when
     * searching for the position, for example if the index 0 then the 1st bookmark in the list will
     * be returned rather than having to offset by non-bookmark list items.
     */
    @Nullable BookmarkId getBookmarkIdByPositionForTesting(int position);

    /** Returns the ImprovedBookmarkRow by position, ignores other view types like above. */
    @Nullable ImprovedBookmarkRow getBookmarkRowByPosition(int position);

    /** Returns the bookmark's ViewHolder by position, ignores other view types like above. */
    @Nullable ViewHolder getBookmarkViewHolderByPosition(int position);

    /** Returns any view holder by position. */
    @Nullable ViewHolder getViewHolderByPosition(int position);

    /** Returns the bookmark count in the current context. */
    int getBookmarkCount();

    /** Returns the start index of bookmarks. */
    int getBookmarkStartIndex();

    /** Returns the end index of bookmarks. */
    int getBookmarkEndIndex();

    /** Search the bookmarks manager for the given query. */
    void searchForTesting(@Nullable String query);

    // TODO(crbug.com/40264714): Delete this method.
    void simulateSignInForTesting();
}
