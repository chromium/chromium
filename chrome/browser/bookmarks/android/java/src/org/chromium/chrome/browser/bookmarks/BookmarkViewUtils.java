// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.UiUtils;

import java.util.Objects;

/** A class holding static util functions for bookmark views. */
@NullMarked
public class BookmarkViewUtils {
    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @param bookmarkId The bookmark id of the folder.
     * @param bookmarkModel The bookmark model.
     * @return A {@link Drawable} to use for displaying bookmark folders.
     */
    public static Drawable getFolderIcon(
            Context context,
            BookmarkId bookmarkId,
            BookmarkModel bookmarkModel,
            @BookmarkRowDisplayPref int displayPref) {
        ColorStateList tint = getFolderIconTint(context, bookmarkId.getType());
        if (bookmarkId.getType() == BookmarkType.READING_LIST) {
            return UiUtils.getTintedDrawable(context, R.drawable.ic_reading_list_folder_24dp, tint);
        } else if (bookmarkId.getType() == BookmarkType.NORMAL
                && Objects.equals(bookmarkId, bookmarkModel.getDesktopFolderId())) {
            return UiUtils.getTintedDrawable(context, R.drawable.ic_toolbar_24dp, tint);
        }

        return UiUtils.getTintedDrawable(
                context,
                displayPref == BookmarkRowDisplayPref.VISUAL
                        ? R.drawable.ic_folder_outline_24dp
                        : R.drawable.ic_folder_blue_24dp,
                tint);
    }

    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @param type The bookmark type of the folder.
     * @return The tint used on the bookmark folder icon.
     */
    public static ColorStateList getFolderIconTint(Context context, @BookmarkType int type) {
        if (type == BookmarkType.READING_LIST) {
            return ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColorAccent1(context));
        }

        return ColorStateList.valueOf(context.getColor(R.color.default_icon_color_tint_list));
    }

    /**
     * Gets the display count for folders.
     *
     * @param id The bookmark to get the description for, must be a folder.
     * @param bookmarkModel The bookmark model to get info on the bookmark.
     */
    public static int getChildCountForDisplay(BookmarkId id, BookmarkModel bookmarkModel) {
        if (id.getType() == BookmarkType.READING_LIST) {
            return bookmarkModel.getUnreadCount(id);
        } else {
            return bookmarkModel.getTotalBookmarkCount(id);
        }
    }

    /**
     * Returns the description to use for the folder in bookmarks manager.
     *
     * @param id The bookmark to get the description for, must be a folder.
     * @param bookmarkModel The bookmark model to get info on the bookmark.
     * @param resources Android resources object to get strings.
     */
    public static String getFolderDescriptionText(
            BookmarkId id, BookmarkModel bookmarkModel, Resources resources) {
        int count = getChildCountForDisplay(id, bookmarkModel);
        if (id.getType() == BookmarkType.READING_LIST) {
            return (count > 0)
                    ? resources.getQuantityString(
                            R.plurals.reading_list_unread_page_count, count, count)
                    : resources.getString(R.string.reading_list_no_unread_pages);
        } else {
            return (count > 0)
                    ? resources.getQuantityString(R.plurals.bookmarks_count, count, count)
                    : resources.getString(R.string.no_bookmarks);
        }
    }

    /** Returns the RoundedIconGenerator with the appropriate size. */
    public static RoundedIconGenerator getRoundedIconGenerator(
            Context context, @BookmarkRowDisplayPref int displayPref) {
        Resources res = context.getResources();
        int iconSize = getFaviconDisplaySize(res);

        return displayPref == BookmarkRowDisplayPref.VISUAL
                ? new RoundedIconGenerator(
                        iconSize,
                        iconSize,
                        iconSize / 2,
                        context.getColor(R.color.default_favicon_background_color),
                        getDisplayTextSize(res))
                : FaviconUtils.createCircularIconGenerator(context);
    }

    /** Returns the size to use when fetching favicons. */
    public static int getFaviconFetchSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.tile_view_icon_min_size);
    }

    /** Returns the size to use when displaying an image. */
    public static int getImageIconSize(
            Resources resources, @BookmarkRowDisplayPref int displayPref) {
        return displayPref == BookmarkRowDisplayPref.VISUAL
                ? resources.getDimensionPixelSize(R.dimen.improved_bookmark_start_image_size_visual)
                : resources.getDimensionPixelSize(
                        R.dimen.improved_bookmark_start_image_size_compact);
    }

    /** Returns the size to use when displaying the favicon. */
    public static int getFaviconDisplaySize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.tile_view_icon_size_modern);
    }

    /** Return the background color for the given {@link BookmarkType}. */
    public static @ColorInt int getIconBackground(
            Context context, BookmarkModel bookmarkModel, BookmarkItem item) {
        if (bookmarkModel.isSpecialFolder(item)) {
            return SemanticColorUtils.getColorPrimaryContainer(context);
        } else {
            return SemanticColorUtils.getColorSurfaceContainerLow(context);
        }
    }

    /** Return the icon tint for the given {@link BookmarkType}. */
    public static ColorStateList getIconTint(
            Context context, BookmarkModel bookmarkModel, BookmarkItem item) {
        if (bookmarkModel.isSpecialFolder(item)) {
            return ColorStateList.valueOf(
                    SemanticColorUtils.getDefaultIconColorOnAccent1Container(context));
        } else {
            return AppCompatResources.getColorStateList(
                    context, R.color.default_icon_color_secondary_tint_list);
        }
    }

    private static int getDisplayTextSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.improved_bookmark_favicon_text_size);
    }
}
