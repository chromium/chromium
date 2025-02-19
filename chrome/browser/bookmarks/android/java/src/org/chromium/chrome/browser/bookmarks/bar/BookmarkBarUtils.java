// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BiConsumer;

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
     * @param clickCallback the callback to invoke on list item click events.
     * @param context the context in which the created list item will be rendered.
     * @param imageFetcher the image fetcher to use for rendering favicons.
     * @param item the bookmark item for which to create a renderable list item.
     * @return the created list item to render in the bookmark bar.
     */
    public static @NonNull ListItem createListItemFor(
            @NonNull BiConsumer<BookmarkItem, Integer> clickCallback,
            @NonNull Context context,
            @NonNull BookmarkImageFetcher imageFetcher,
            @NonNull BookmarkItem item) {
        return new ListItem(
                ViewType.ITEM,
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(
                                BookmarkBarButtonProperties.CLICK_CALLBACK,
                                (metaState) -> clickCallback.accept(item, metaState))
                        .with(
                                BookmarkBarButtonProperties.ICON_SUPPLIER,
                                createIconSupplierFor(context, imageFetcher, item))
                        .with(
                                BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                                item.isFolder()
                                        ? R.color.default_icon_color_tint_list
                                        : Resources.ID_NULL)
                        .with(BookmarkBarButtonProperties.TITLE, item.getTitle())
                        .build());
    }

    private static @NonNull LazyOneshotSupplier<Drawable> createIconSupplierFor(
            @NonNull Context context,
            @NonNull BookmarkImageFetcher imageFetcher,
            @NonNull BookmarkItem item) {
        if (item.isFolder()) {
            return LazyOneshotSupplier.fromSupplier(
                    () ->
                            AppCompatResources.getDrawable(
                                    context, R.drawable.ic_folder_outline_24dp));
        }
        return new LazyOneshotSupplierImpl<>() {
            @Override
            public void doSet() {
                imageFetcher.fetchFaviconForBookmark(item, this::set);
            }
        };
    }
}
