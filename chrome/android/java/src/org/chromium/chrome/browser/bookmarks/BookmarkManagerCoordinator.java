// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;

/** Responsible for setting up sub-components and routing incoming/outgoing signals */
public class BookmarkManagerCoordinator {
    private final @NonNull ImageFetcher mImageFetcher;
    private final @NonNull SnackbarManager mSnackbarManager;

    private @Nullable BookmarkPromoHeader mPromoHeaderManager;
    private @Nullable BookmarkDelegate mDelegate;

    /**
     * Creates a partially initialized instance, needs {@link onBookmarkDelegateInitialized}.
     * @param profile Profile instance corresponding to the active profile.
     * @param snackbarManager Allows control over the app snackbar.
     */
    public BookmarkManagerCoordinator(
            @NonNull Profile profile, @NonNull SnackbarManager snackbarManager) {
        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(), GlobalDiscardableReferencePool.getReferencePool());
        mSnackbarManager = snackbarManager;
    }

    /**
     * Fully initializes, ready to be used after this is called.
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     * @param bookmarkPromoHeader Used to show the signin promo header.
     */
    public void onBookmarkDelegateInitialized(
            @NonNull BookmarkDelegate delegate, @NonNull BookmarkPromoHeader bookmarkPromoHeader) {
        mDelegate = delegate;
        mPromoHeaderManager = bookmarkPromoHeader;
    }

    /**
     * Should only be called after fully initialized.
     * @param parent The parent to which the new {@link View} will be added as a child.
     * @param viewType The type of row being created.
     * @return A new View that can be added to the view hierarchy.
     */
    public View createView(@NonNull ViewGroup parent, @ViewType int viewType) {
        assert mDelegate != null;

        // The shopping-specific bookmark row is only shown with the visual refresh. When
        // there's a mismatch, the ViewType is downgraded to ViewType.BOOKMARK.
        if (viewType == ViewType.SHOPPING_POWER_BOOKMARK
                && !BookmarkFeatures.isBookmarksVisualRefreshEnabled()) {
            viewType = ViewType.BOOKMARK;
        }

        switch (viewType) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
            case ViewType.PERSONALIZED_SYNC_PROMO:
                return buildPersonalizedPromoView(parent);
            case ViewType.SYNC_PROMO:
                return buildLegacyPromoView(parent);
            case ViewType.SECTION_HEADER:
                return buildSectionHeaderView(parent);
            case ViewType.FOLDER:
                return buildBookmarkFolderView(parent);
            case ViewType.BOOKMARK:
                return buildBookmarkItemView(parent);
            case ViewType.SHOPPING_POWER_BOOKMARK:
                return buildShoppingItemView(parent);
            case ViewType.DIVIDER:
                return buildDividerView(parent);
            case ViewType.SHOPPING_FILTER:
                return buildShoppingFilterView(parent);
            default:
                assert false;
                return null;
        }
    }

    private View buildPersonalizedPromoView(ViewGroup parent) {
        return mPromoHeaderManager.createPersonalizedSigninAndSyncPromoHolder(parent);
    }

    private View buildLegacyPromoView(ViewGroup parent) {
        return mPromoHeaderManager.createSyncPromoHolder(parent);
    }

    private View buildSectionHeaderView(ViewGroup parent) {
        return inflate(parent, org.chromium.chrome.R.layout.bookmark_section_header);
    }

    private View buildBookmarkFolderView(ViewGroup parent) {
        return inflateBookmarkRow(parent, org.chromium.chrome.R.layout.bookmark_folder_row);
    }

    private View buildBookmarkItemView(ViewGroup parent) {
        return inflateBookmarkRow(parent, org.chromium.chrome.R.layout.bookmark_item_row);
    }

    private View buildShoppingItemView(ViewGroup parent) {
        PowerBookmarkShoppingItemRow row = (PowerBookmarkShoppingItemRow) inflateBookmarkRow(
                parent, org.chromium.chrome.R.layout.power_bookmark_shopping_item_row);
        row.init(mImageFetcher, mDelegate.getModel(), mSnackbarManager);
        return row;
    }

    private View buildDividerView(ViewGroup parent) {
        return inflate(parent, org.chromium.chrome.R.layout.horizontal_divider);
    }

    private View buildShoppingFilterView(ViewGroup parent) {
        return inflate(parent, org.chromium.chrome.R.layout.shopping_filter_row);
    }

    private View inflate(ViewGroup parent, @LayoutRes int layoutId) {
        Context context = parent.getContext();
        return LayoutInflater.from(context).inflate(layoutId, parent, false);
    }

    private View inflateBookmarkRow(ViewGroup parent, @LayoutRes int layoutId) {
        BookmarkRow row = (BookmarkRow) inflate(parent, layoutId);
        (row).onDelegateInitialized(mDelegate);
        return row;
    }
}