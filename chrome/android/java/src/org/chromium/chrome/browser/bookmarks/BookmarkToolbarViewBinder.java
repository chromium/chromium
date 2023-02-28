// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for hosting properties of BookmarkToolbar views. */
class BookmarkToolbarViewBinder {
    /** Binds the given property to the given model for the given view. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        BookmarkToolbar bookmarkToolbar = (BookmarkToolbar) view;
        if (key == BookmarkToolbarProperties.BOOKMARK_DELEGATE) {
            bookmarkToolbar.setBookmarkDelegate(
                    model.get(BookmarkToolbarProperties.BOOKMARK_DELEGATE));
        } else if (key == BookmarkToolbarProperties.DRAG_REORDERABLE_LIST_ADAPTER) {
            bookmarkToolbar.setDragReorderableListAdapter(
                    model.get(BookmarkToolbarProperties.DRAG_REORDERABLE_LIST_ADAPTER));
        } else if (key == BookmarkToolbarProperties.BOOKMARK_UI_STATE) {
            bookmarkToolbar.setBookmarkUiState(
                    model.get(BookmarkToolbarProperties.BOOKMARK_UI_STATE));
        }
    }
}
