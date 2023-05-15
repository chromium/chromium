// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.bookmarks.BookmarkId;

import java.util.List;

/** Builds {@link List} of {@link BookmarkListEntry} for various scenarios. */
public interface BookmarkQueryHandler {
    /** Cleans up observers this class held. */
    default void destroy() {}

    /**
     * Builds entries for a given parent folder.
     * @param parentId The id of the parent.
     * @return The list of bookmarks to shown.
     */
    List<BookmarkListEntry> buildBookmarkListForParent(BookmarkId parentId);

    /**
     * Builds entries for a search query.
     * @param query The search string.
     * @return The list of bookmarks to shown.
     */
    List<BookmarkListEntry> buildBookmarkListForSearch(String query);
}
