// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.search.SearchBoxProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Responsible for hosting properties for the shopping chips in the BookmarkSearchBoxRow. */
@NullMarked
class BookmarkSearchBoxRowProperties {
    public static final ReadableObjectPropertyKey<Callback<Boolean>> SHOPPING_CHIP_TOGGLE_CALLBACK =
            new ReadableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SHOPPING_CHIP_SELECTED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey SHOPPING_CHIP_VISIBILITY =
            new WritableBooleanPropertyKey();
    public static final ReadableObjectPropertyKey<Integer> SHOPPING_CHIP_START_ICON_RES =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Integer> SHOPPING_CHIP_TEXT_RES =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] BOOKMARK_KEYS = {
        BookmarkManagerProperties.BOOKMARK_LIST_ENTRY,
        SHOPPING_CHIP_TOGGLE_CALLBACK,
        SHOPPING_CHIP_SELECTED,
        SHOPPING_CHIP_VISIBILITY,
        SHOPPING_CHIP_START_ICON_RES,
        SHOPPING_CHIP_TEXT_RES
    };

    static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(BOOKMARK_KEYS, SearchBoxProperties.ALL_KEYS);
}
