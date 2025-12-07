// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

public class BookmarkModelListTestUtil {
    /** Verifies that the given expectedViewTypes are present in the given ModelList. */
    public static void verifyModelListHasViewTypes(
            ModelList modelList, @ViewType int... expectedViewTypes) {
        assertEquals(expectedViewTypes.length, modelList.size());
        for (int i = 0; i < expectedViewTypes.length; ++i) {
            assertEquals(
                    "ViewType did not match at index " + i,
                    expectedViewTypes[i],
                    modelList.get(i).type);
        }
    }

    /** Verifies that the given expectedBookmarkIds are present in the given ModelList */
    public static void verifyModelListHasBookmarkIds(
            ModelList modelList, BookmarkId... expectedBookmarkIds) {
        assertEquals(expectedBookmarkIds.length, modelList.size());
        for (int i = 0; i < expectedBookmarkIds.length; ++i) {
            BookmarkId bookmarkId = getBookmarkIdFromModel(modelList.get(i).model);
            assertEquals(
                    "BookmarkId did not match at index " + i, expectedBookmarkIds[i], bookmarkId);
        }
    }

    private static @Nullable BookmarkId getBookmarkIdFromModel(PropertyModel propertyModel) {
        BookmarkListEntry bookmarkListEntry =
                propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
        if (bookmarkListEntry == null) {
            return null;
        }
        BookmarkItem bookmarkItem = bookmarkListEntry.getBookmarkItem();
        if (bookmarkItem == null) {
            return null;
        }
        return bookmarkItem.getId();
    }
}
