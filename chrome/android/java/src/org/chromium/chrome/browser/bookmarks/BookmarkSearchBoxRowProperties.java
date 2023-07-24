// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Responsible for hosting properties for {@link BookmarkSearchBoxRow}. */
class BookmarkSearchBoxRowProperties {
    public static final ReadableObjectPropertyKey<Callback<String>> QUERY_CALLBACK =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Callback<Boolean>> SHOPPING_CHIP_TOGGLE_CALLBACK =
            new ReadableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SHOPPING_CHIP_VISIBILITY =
            new WritableBooleanPropertyKey();
    public static final ReadableObjectPropertyKey<Integer> SHOPPING_CHIP_START_ICON_RES =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Integer> SHOPPING_CHIP_TEXT_RES =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {BookmarkManagerProperties.BOOKMARK_LIST_ENTRY,
            QUERY_CALLBACK, SHOPPING_CHIP_TOGGLE_CALLBACK, SHOPPING_CHIP_VISIBILITY,
            SHOPPING_CHIP_START_ICON_RES, SHOPPING_CHIP_TEXT_RES};
}
