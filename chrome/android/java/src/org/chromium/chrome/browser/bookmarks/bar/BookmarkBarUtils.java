// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utilities for the bookmark bar which provides users with bookmark access from top chrome. */
class BookmarkBarUtils {

    /** Enumeration of view type identifiers for views which are rendered in the bookmark bar. */
    @IntDef({ViewType.ITEM})
    @Retention(RetentionPolicy.SOURCE)
    public static @interface ViewType {
        int ITEM = 1;
    }

    private BookmarkBarUtils() {}

    /**
     * Creates a list item to render in the bookmark bar for the specified bookmark item.
     *
     * @param context the context in which the created list item will be rendered.
     * @param item the bookmark item for which to create a renderable list item.
     * @return the created list item to render in the bookmark bar.
     */
    public static @NonNull ListItem createListItemFor(
            @NonNull Context context, @NonNull BookmarkItem item) {
        // TODO(crbug.com/347632437): Replace star filled icon w/ favicon.
        return new ListItem(
                ViewType.ITEM,
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(
                                BookmarkBarButtonProperties.ICON,
                                AppCompatResources.getDrawable(context, R.drawable.btn_star_filled))
                        .with(BookmarkBarButtonProperties.TITLE, item.getTitle())
                        .build());
    }
}
