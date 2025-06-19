// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Objects;

/** Business logic for the improved bookmark row. */
@NullMarked
public class ImprovedBookmarkRowCoordinator {
    private final Context mContext;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final ShoppingService mShoppingService;
    private int mImageSize;

    /**
     * @param context The calling context.
     * @param bookmarkImageFetcher Fetches images for bookmarks.
     * @param bookmarkModel The bookmark model used to query bookmark properties.
     * @param bookmarkUiPrefs Tracks the user's preferences about the bookmark manager.
     * @param shoppingService The bookmark model used to query bookmark properties.
     */
    public ImprovedBookmarkRowCoordinator(
            Context context,
            BookmarkImageFetcher bookmarkImageFetcher,
            BookmarkModel bookmarkModel,
            BookmarkUiPrefs bookmarkUiPrefs,
            ShoppingService shoppingService) {
        mContext = context;
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mShoppingService = shoppingService;
        onBookmarkRowDisplayPrefChanged(mBookmarkUiPrefs.getBookmarkRowDisplayPref());
    }

    private void onBookmarkRowDisplayPrefChanged(@BookmarkRowDisplayPref int displayPref) {
        mImageSize = BookmarkViewUtils.getImageIconSize(mContext.getResources(), displayPref);
    }

    /** Sets the given bookmark id. */
    public PropertyModel createBasePropertyModel(BookmarkId bookmarkId) {
        PropertyModel propertyModel = new PropertyModel(ImprovedBookmarkRowProperties.ALL_KEYS);
        BookmarkItem bookmarkItem = assumeNonNull(mBookmarkModel.getBookmarkById(bookmarkId));
        PowerBookmarkMeta meta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();

        propertyModel.set(BookmarkManagerProperties.BOOKMARK_ID, bookmarkId);

        // Title.
        if (displayPref == BookmarkRowDisplayPref.COMPACT && bookmarkItem.isFolder()) {
            propertyModel.set(
                    ImprovedBookmarkRowProperties.TITLE,
                    String.format(
                            "%s (%s)",
                            bookmarkItem.getTitle(),
                            BookmarkViewUtils.getChildCountForDisplay(bookmarkId, mBookmarkModel)));
        } else {
            propertyModel.set(ImprovedBookmarkRowProperties.TITLE, bookmarkItem.getTitle());
        }

        // Description and content description.
        boolean isFolder = bookmarkItem.isFolder();
        boolean isLocalBookmark =
                mBookmarkModel.areAccountBookmarkFoldersActive()
                        && !bookmarkItem.isAccountBookmark();
        propertyModel.set(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK, isLocalBookmark);
        propertyModel.set(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE, !isFolder);
        if (isFolder) {
            String contentDescription =
                    String.format(
                            "%s %s",
                            bookmarkItem.getTitle(),
                            BookmarkViewUtils.getFolderDescriptionText(
                                    bookmarkId, mBookmarkModel, mContext.getResources()));
            if (isLocalBookmark) {
                contentDescription =
                        String.format(
                                "%s %s",
                                contentDescription,
                                mContext.getString(R.string.local_bookmarks_section_header));
            }
            propertyModel.set(
                    ImprovedBookmarkRowProperties.CONTENT_DESCRIPTION, contentDescription);
        } else {
            propertyModel.set(
                    ImprovedBookmarkRowProperties.DESCRIPTION, bookmarkItem.getUrlForDisplay());
        }

        // Selection and drag state setup.
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTED, false);
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
        propertyModel.set(ImprovedBookmarkRowProperties.EDITABLE, bookmarkItem.isEditable());

        // Shopping coordinator setup.
        if (PowerBookmarkUtils.isShoppingListItem(mShoppingService, meta)) {
            ShoppingAccessoryCoordinator shoppingAccessoryCoordinator =
                    new ShoppingAccessoryCoordinator(
                            mContext, meta.getShoppingSpecifics(), mShoppingService);
            propertyModel.set(
                    ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR,
                    shoppingAccessoryCoordinator);
            propertyModel.set(
                    ImprovedBookmarkRowProperties.ACCESSORY_VIEW,
                    shoppingAccessoryCoordinator.getView());
        } else {
            propertyModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, null);
        }

        // Icon.
        resolveImagesForBookmark(propertyModel, bookmarkItem);

        return propertyModel;
    }

    private void resolveImagesForBookmark(PropertyModel propertyModel, BookmarkItem item) {
        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();
        propertyModel.set(
                ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY,
                item.isFolder() && displayPref == BookmarkRowDisplayPref.VISUAL
                        ? ImageVisibility.FOLDER_DRAWABLE
                        : ImageVisibility.DRAWABLE);

        if (item.isFolder() && displayPref == BookmarkRowDisplayPref.VISUAL) {
            populateVisualFolderProperties(propertyModel, item);
        } else if (item.isFolder()) {
            propertyModel.set(
                    ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                    BookmarkViewUtils.getIconBackground(mContext, mBookmarkModel, item));
            propertyModel.set(
                    ImprovedBookmarkRowProperties.START_ICON_TINT,
                    BookmarkViewUtils.getIconTint(mContext, mBookmarkModel, item));
        } else {
            propertyModel.set(
                    ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                    SemanticColorUtils.getColorSurfaceContainerLow(mContext));
            propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_TINT, null);
        }

        LazyOneshotSupplier<Drawable> drawableSupplier =
                new LazyOneshotSupplierImpl<>() {
                    @Override
                    public void doSet() {
                        if (item.isFolder()) {
                            set(
                                    BookmarkViewUtils.getFolderIcon(
                                            mContext, item.getId(), mBookmarkModel, displayPref));
                        } else if (shouldShowImagesForBookmark(item, displayPref)) {
                            mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(
                                    item, mImageSize, this::set);
                        } else {
                            mBookmarkImageFetcher.fetchFaviconForBookmark(item, this::set);
                        }
                    }
                };
        propertyModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, drawableSupplier);
    }

    private void populateVisualFolderProperties(
            PropertyModel propertyModel, BookmarkItem bookmarkItem) {
        propertyModel.set(
                ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT,
                BookmarkViewUtils.getChildCountForDisplay(bookmarkItem.getId(), mBookmarkModel));
        propertyModel.set(
                ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT_TEXT_STYLE,
                mBookmarkModel.isSpecialFolder(bookmarkItem)
                        ? R.style.TextAppearance_SpecialFolderChildCount
                        : R.style.TextAppearance_RegularFolderChildCount);
        propertyModel.set(
                ImprovedBookmarkRowProperties.FOLDER_START_AREA_BACKGROUND_COLOR,
                BookmarkViewUtils.getIconBackground(mContext, mBookmarkModel, bookmarkItem));
        propertyModel.set(
                ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT,
                BookmarkViewUtils.getIconTint(mContext, mBookmarkModel, bookmarkItem));
        propertyModel.set(
                ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE,
                BookmarkViewUtils.getFolderIcon(
                        mContext,
                        bookmarkItem.getId(),
                        mBookmarkModel,
                        BookmarkRowDisplayPref.VISUAL));
        LazyOneshotSupplierImpl<Pair<Drawable, Drawable>> drawablesSupplier =
                new LazyOneshotSupplierImpl<>() {
                    @Override
                    public void doSet() {
                        if (shouldShowImagesForFolder(bookmarkItem.getId())) {
                            mBookmarkImageFetcher.fetchFirstTwoImagesForFolder(
                                    bookmarkItem, mImageSize, this::set);
                        } else {
                            set(new Pair<>(null, null));
                        }
                    }
                };
        propertyModel.set(
                ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                drawablesSupplier);
    }

    /**
     * Returns whether images should be shown for a given bookmark, which is true if the user has
     * selected the visual display preference and the bookmark is synced with google.
     */
    boolean shouldShowImagesForBookmark(
            BookmarkItem item, @BookmarkRowDisplayPref int displayPref) {
        return displayPref == BookmarkRowDisplayPref.VISUAL;
    }

    /**
     * Returns whether images should be shown for a given folder, which is true if the user has
     * selected the visual display preference and the folder is synced with google.
     */
    boolean shouldShowImagesForFolder(BookmarkId folder) {
        BookmarkId rootNodeId = mBookmarkModel.getRootFolderId();
        BookmarkId desktopNodeId = mBookmarkModel.getDesktopFolderId();
        BookmarkId mobileNodeId = mBookmarkModel.getMobileFolderId();
        BookmarkId othersNodeId = mBookmarkModel.getOtherFolderId();

        List<BookmarkId> specialFoldersIds = mBookmarkModel.getTopLevelFolderIds();
        return !Objects.equals(folder, rootNodeId)
                && !Objects.equals(folder, desktopNodeId)
                && !Objects.equals(folder, mobileNodeId)
                && !Objects.equals(folder, othersNodeId)
                && !specialFoldersIds.contains(folder);
    }
}
