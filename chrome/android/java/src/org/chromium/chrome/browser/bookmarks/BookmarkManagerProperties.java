// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.Callback;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of BookmarkManager views. */
public class BookmarkManagerProperties {
    public static final WritableObjectPropertyKey<BookmarkPromoHeader> BOOKMARK_PROMO_HEADER =
            new WritableObjectPropertyKey<>();
    public static final ReadableIntPropertyKey PROMO_TOP_MARGIN_RES = new ReadableIntPropertyKey();
    // TODO(https://crbug.com/1416611): Replace with individual fields.
    public static final WritableObjectPropertyKey<BookmarkListEntry> BOOKMARK_LIST_ENTRY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<BookmarkId> BOOKMARK_ID =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey LOCATION = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey IS_FROM_FILTER_VIEW =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_HIGHLIGHTED =
            new WritableBooleanPropertyKey();
    // TODO(https://crbug.com/1416611): Rework this property to not just expose functionality.
    public static final WritableObjectPropertyKey<Callback<BookmarkId>> OPEN_FOLDER =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        BOOKMARK_PROMO_HEADER,
        PROMO_TOP_MARGIN_RES,
        BOOKMARK_LIST_ENTRY,
        BOOKMARK_ID,
        LOCATION,
        IS_FROM_FILTER_VIEW,
        IS_HIGHLIGHTED,
        OPEN_FOLDER
    };
}
