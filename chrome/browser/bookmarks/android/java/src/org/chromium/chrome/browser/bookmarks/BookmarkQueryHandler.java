// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.power_bookmarks.PowerBookmarkType;

import java.util.List;
import java.util.Set;

/** Builds {@link List} of {@link BookmarkListEntry} for various scenarios. */
public interface BookmarkQueryHandler {
    /** Cleans up observers this class held. */
    default void destroy() {}

    /**
     * Builds entries for a given parent folder.
     *
     * @param parentId The id of the parent.
     * @return The list of bookmarks to shown.
     */
    default List<BookmarkListEntry> buildBookmarkListForParent(@NonNull BookmarkId parentId) {
        return buildBookmarkListForParent(parentId, /* powerFilter= */ null);
    }

    /**
     * Builds entries for a given parent folder.
     *
     * @param parentId The id of the parent.
     * @param powerFilter The filter to be applied to the {@link parentId} subtree. When a filter is
     *     present, all matching descendants must be returned,
     * @return The list of bookmarks to shown.
     */
    List<BookmarkListEntry> buildBookmarkListForParent(
            @NonNull BookmarkId parentId, @Nullable Set<PowerBookmarkType> powerFilter);

    /**
     * Builds entries for a search query.
     *
     * @param query The search string, empty means match everything.
     * @param powerFilter Unless empty, return only bookmarks with matching power. Can be null.
     * @return The list of bookmarks to shown.
     */
    List<BookmarkListEntry> buildBookmarkListForSearch(
            @NonNull String query, @Nullable Set<PowerBookmarkType> powerFilter);

    /**
     * Builds entries for folder selection.
     *
     * @param parentId The id of the parent.
     * @return The list of bookmarks to shown, includes all root folder even if they're empty.
     *     Result is a list of folder which bookmarks can be added. Doesn't perform any specific
     *     checks on the bookmarks being moved, so there still may be invalid folder included for
     *     specific scenarios. One specific scenario is that reading list will be included in the
     *     result which may be invalid if the URL isn't supported or if a folder is being moved.
     */
    List<BookmarkListEntry> buildBookmarkListForFolderSelect(@NonNull BookmarkId parentId);
}
