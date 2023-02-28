// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of BookmarkToolbar views. */
class BookmarkToolbarProperties {
    static final WritableObjectPropertyKey<BookmarkDelegate> BOOKMARK_DELEGATE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<DragReorderableListAdapter>
            DRAG_REORDERABLE_LIST_ADAPTER = new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey BOOKMARK_UI_STATE = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
            BOOKMARK_DELEGATE, DRAG_REORDERABLE_LIST_ADAPTER, BOOKMARK_UI_STATE};
}
