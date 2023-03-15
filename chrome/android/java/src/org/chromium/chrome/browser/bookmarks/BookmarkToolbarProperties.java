// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.Callback;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of BookmarkToolbar views. */
class BookmarkToolbarProperties {
    /** Dependencies */
    static final WritableObjectPropertyKey<BookmarkModel> BOOKMARK_MODEL =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<BookmarkOpener> BOOKMARK_OPENER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<SelectionDelegate> SELECTION_DELEGATE =
            new WritableObjectPropertyKey<>();

    /** UI state properties. */
    static final WritableObjectPropertyKey<Integer> BOOKMARK_UI_MODE =
            new WritableObjectPropertyKey<>(/*skipEquality=*/true);
    static final WritableObjectPropertyKey<Boolean> SOFT_KEYBOARD_VISIBLE =
            new WritableObjectPropertyKey<>(/*skipEquality=*/true);
    static final WritableBooleanPropertyKey IS_DIALOG_UI = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey DRAG_ENABLED = new WritableBooleanPropertyKey();

    /** Bookmark state properties. */
    static final WritableObjectPropertyKey<BookmarkId> CURRENT_FOLDER =
            new WritableObjectPropertyKey<>(/*skipEquality=*/true);

    /** Callables to delegate business logic back to the mediator */
    static final WritableObjectPropertyKey<Runnable> OPEN_SEARCH_UI_RUNNABLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Callback<BookmarkId>> OPEN_FOLDER_CALLBACK =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {BOOKMARK_MODEL, BOOKMARK_OPENER, SELECTION_DELEGATE,
            BOOKMARK_UI_MODE, SOFT_KEYBOARD_VISIBLE, IS_DIALOG_UI, DRAG_ENABLED, CURRENT_FOLDER,
            OPEN_SEARCH_UI_RUNNABLE, OPEN_FOLDER_CALLBACK};
}
