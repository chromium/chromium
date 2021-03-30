// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The properties used to define UI elements in bookmark bottom sheet.
 */
class BookmarkBottomSheetItemProperties {
    /**
     * The view holder type used in the {@link RecyclerView} in the bookmark bottom sheet.
     */
    @IntDef({ItemType.FOLDER_ROW})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        int FOLDER_ROW = 1;
    }

    /**
     * The title of the bottom sheet item. e.g. Mobile bookmarks, reading list.
     */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /**
     * The subtitle of the bottom sheet item. e.g. 4 bookmarks, 8 unread pages.
     */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> SUBTITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /**
     * The pair of {@link Drawable} and the color resource id(as Integer) of the bottom sheet item
     * icon. e.g. A folder icon and its color resource id.
     */
    static final PropertyModel
            .ReadableObjectPropertyKey<Pair<Drawable, Integer>> ICON_DRAWABLE_AND_COLOR =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /**
     * A callback invoked when the bottom sheet bookmark item is clicked.
     */
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new PropertyModel.ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TITLE, SUBTITLE, ICON_DRAWABLE_AND_COLOR, ON_CLICK_LISTENER};
}
