// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.modelutil.PropertyModel;

/** Business logic for the improved bookmark row. */
public class ImprovedBookmarkRowCoordinator {
    private final Context mContext;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final ShoppingService mShoppingService;

    /**
     * @param context The calling context.
     * @param bookmarkImageFetcher Fetches images for bookmarks.
     * @param bookmarkModel The bookmark model used to query bookmark properties.
     * @param bookmarkUiPrefs Tracks the user's preferences about the bookmark manager.
     * @param shoppingService The bookmark model used to query bookmark properties.
     */
    public ImprovedBookmarkRowCoordinator(Context context,
            BookmarkImageFetcher bookmarkImageFetcher, BookmarkModel bookmarkModel,
            BookmarkUiPrefs bookmarkUiPrefs, ShoppingService shoppingService) {
        mContext = context;
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mShoppingService = shoppingService;
    }

    /** Sets the given bookmark id. */
    public PropertyModel createBasePropertyModel(BookmarkId bookmarkId) {
        PropertyModel propertyModel = new PropertyModel(ImprovedBookmarkRowProperties.ALL_KEYS);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        PowerBookmarkMeta meta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();

        propertyModel.set(BookmarkManagerProperties.BOOKMARK_ID, bookmarkId);

        // Title
        if (displayPref == BookmarkRowDisplayPref.COMPACT && bookmarkItem.isFolder()) {
            propertyModel.set(ImprovedBookmarkRowProperties.TITLE,
                    String.format(bookmarkItem.getTitle() + " (%s)",
                            BookmarkUtils.getChildCountForDisplay(bookmarkId, mBookmarkModel)));
        } else {
            propertyModel.set(ImprovedBookmarkRowProperties.TITLE, bookmarkItem.getTitle());
        }

        // Description
        propertyModel.set(
                ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE, !bookmarkItem.isFolder());
        // Only bookmarks have descriptions.
        if (!bookmarkItem.isFolder()) {
            propertyModel.set(
                    ImprovedBookmarkRowProperties.DESCRIPTION, bookmarkItem.getUrlForDisplay());
        }

        // Selection and drag state setup.
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTED, false);
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
        propertyModel.set(ImprovedBookmarkRowProperties.DRAG_ENABLED, false);
        propertyModel.set(ImprovedBookmarkRowProperties.EDITABLE, bookmarkItem.isEditable());

        // Shopping coordinator setup.
        if (PowerBookmarkUtils.isShoppingListItem(meta)) {
            ShoppingAccessoryCoordinator shoppingAccessoryCoordinator =
                    new ShoppingAccessoryCoordinator(
                            mContext, meta.getShoppingSpecifics(), mShoppingService);
            propertyModel.set(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR,
                    shoppingAccessoryCoordinator);
            propertyModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW,
                    shoppingAccessoryCoordinator.getView());
        } else {
            propertyModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, null);
        }

        // Icon
        resolveImagesForBookmark(propertyModel, bookmarkItem);

        return propertyModel;
    }

    private void resolveImagesForBookmark(PropertyModel propertyModel, BookmarkItem item) {
        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();
        boolean useImages = displayPref == BookmarkRowDisplayPref.VISUAL;
        propertyModel.set(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY,
                item.isFolder() && useImages ? ImageVisibility.FOLDER_DRAWABLE
                                             : ImageVisibility.DRAWABLE);

        @BookmarkType
        int type = item.getId().getType();
        if (item.isFolder()) {
            if (displayPref == BookmarkRowDisplayPref.VISUAL) {
                propertyModel.set(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR,
                        new ImprovedBookmarkFolderViewCoordinator(
                                mContext, mBookmarkImageFetcher, mBookmarkModel));
                propertyModel.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR)
                        .setBookmarkId(item.getId());
            }
            propertyModel.set(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                    BookmarkUtils.getIconBackground(mContext, mBookmarkModel, item));
            propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_TINT,
                    BookmarkUtils.getIconTint(mContext, mBookmarkModel, item));
            propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE,
                    BookmarkUtils.getFolderIcon(mContext, type, displayPref));
        } else {
            propertyModel.set(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                    ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_1));
            propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_TINT, null);
            propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, null);
            if (useImages) {
                mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(item, image -> {
                    propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, image);
                });
            } else {
                mBookmarkImageFetcher.fetchFaviconForBookmark(item, image -> {
                    propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, image);
                });
            }
        }
    }
}
