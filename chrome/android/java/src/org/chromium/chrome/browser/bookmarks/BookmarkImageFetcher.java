// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;

import java.util.Iterator;

/** Class which encapsulates fetching images for bookmarks. */
public class BookmarkImageFetcher {
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final ImageFetcher mImageFetcher;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconFetchSize;
    private final CallbackController mCallbackController = new CallbackController();
    private final PageImageServiceQueue mPageImageServiceQueue;

    private RoundedIconGenerator mRoundedIconGenerator;
    private int mImageSize;
    private int mFaviconSize;

    /**
     * @param context The context used to create drawables.
     * @param bookmarkModel The bookmark model used to query information on bookmarks.
     * @param imageFetcher The image fetcher used to fetch images.
     * @param largeIconBridge The large icon fetcher used to fetch favicons.
     * @param roundedIconGenerator Generates fallback images for bookmark favicons.
     * @param imageSize The size when fetching an image. Used for scaling.
     * @param faviconSize The size when fetching a favicon. Used for scaling.
     */
    public BookmarkImageFetcher(Context context, BookmarkModel bookmarkModel,
            ImageFetcher imageFetcher, LargeIconBridge largeIconBridge,
            RoundedIconGenerator roundedIconGenerator, int imageSize, int faviconSize) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mImageFetcher = imageFetcher;
        mLargeIconBridge = largeIconBridge;
        mFaviconFetchSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mRoundedIconGenerator = roundedIconGenerator;
        mImageSize = imageSize;
        mFaviconSize = faviconSize;
        mPageImageServiceQueue = new PageImageServiceQueue(mBookmarkModel);
    }

    /** Destroys this object. */
    public void destroy() {
        mCallbackController.destroy();
        mPageImageServiceQueue.destroy();
    }

    /**
     * Setup the properties required for fetching.
     * @param roundedIconGenerator Generates fallback images for bookmark favicons.
     * @param imageSize The size when fetching an image. Used for scaling.
     * @param faviconSize The size when fetching a favicon. Used for scaling.
     */
    public void setupFetchProperties(
            RoundedIconGenerator roundedIconGenerator, int imageSize, int faviconSize) {
        mRoundedIconGenerator = roundedIconGenerator;
        mImageSize = imageSize;
        mFaviconSize = faviconSize;
    }

    /**
     * Returns the first two images for the given folder.
     * @param folder The folder to fetch the images for.
     * @param callback The callback to receive the images.
     */
    public void fetchFirstTwoImagesForFolder(
            BookmarkItem folder, Callback<Pair<Drawable, Drawable>> callback) {
        fetchFirstTwoImagesForFolderImpl(mBookmarkModel.getChildIds(folder.getId()).iterator(),
                /*firstDrawable=*/null, /*secondDrawable=*/null, callback);
    }

    /**
     * Returns a drawable with the image for the given bookmark. If none is found, then it falls
     * back to the favicon
     * @param item The bookmark to fetch the image for.
     * @param callback The callback to receive the image.
     */
    public void fetchImageForBookmarkWithFaviconFallback(
            BookmarkItem item, Callback<Drawable> callback) {
        fetchImageForBookmark(item, mCallbackController.makeCancelable(drawable -> {
            if (drawable == null) {
                fetchFaviconForBookmark(item, callback);
            } else {
                callback.onResult(drawable);
            }
        }));
    }

    /**
     * Fetches a favicon for the given bookmark.
     * @param item The bookmark to fetch the image for.
     * @param callback The callback to receive the favicon.
     */
    public void fetchFaviconForBookmark(BookmarkItem item, Callback<Drawable> callback) {
        mLargeIconBridge.getLargeIconForUrl(item.getUrl(), mFaviconFetchSize,
                (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) -> {
                    callback.onResult(FaviconUtils.getIconDrawableWithoutFilter(icon, item.getUrl(),
                            fallbackColor, mRoundedIconGenerator, mContext.getResources(),
                            mFaviconSize));
                });
    }

    /**
     * Fetch the given URL and fallback to {@link #fetchImageForBookmarkWithFaviconFallback}.
     * @param url The url to fetch the image for.
     * @param item The item to fallback on if the url fetch fails.
     * @param callback The callback to receive the favicon.
     */
    public void fetchImageUrlWithFallbacks(
            GURL url, BookmarkItem item, Callback<Drawable> callback) {
        fetchImageUrl(url, drawable -> {
            if (drawable == null) {
                fetchImageForBookmarkWithFaviconFallback(item, callback);
            } else {
                callback.onResult(drawable);
            }
        });
    }

    private void fetchImageForBookmark(BookmarkItem item, Callback<Drawable> callback) {
        final Callback<Bitmap> bookmarkImageCallback =
                mCallbackController.makeCancelable((image) -> {
                    if (image == null) {
                        callback.onResult(null);
                    } else {
                        callback.onResult(new BitmapDrawable(mContext.getResources(), image));
                    }
                });

        mPageImageServiceQueue.getSalientImageUrl(
                item.getUrl(), mCallbackController.makeCancelable((imageUrl) -> {
                    if (imageUrl == null) {
                        callback.onResult(null);
                        return;
                    }

                    mImageFetcher.fetchImage(ImageFetcher.Params.create(imageUrl,
                                                     ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                                                     mImageSize, mImageSize),
                            bookmarkImageCallback);
                }));
    }

    private void fetchImageUrl(GURL url, Callback<Drawable> callback) {
        mImageFetcher.fetchImage(
                ImageFetcher.Params.create(
                        url, ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME, mImageSize, mImageSize),
                (image) -> {
                    if (image == null) {
                        callback.onResult(null);
                    } else {
                        callback.onResult(new BitmapDrawable(mContext.getResources(), image));
                    }
                });
    }

    private void fetchFirstTwoImagesForFolderImpl(Iterator<BookmarkId> childIdIterator,
            Drawable firstDrawable, Drawable secondDrawable,
            Callback<Pair<Drawable, Drawable>> callback) {
        if (!childIdIterator.hasNext() || (firstDrawable != null && secondDrawable != null)) {
            callback.onResult(new Pair<>(firstDrawable, secondDrawable));
            return;
        }

        BookmarkId id = childIdIterator.next();
        BookmarkItem item = mBookmarkModel.getBookmarkById(id);

        // It's possible that a child was removed during fetching. In that case, just continue on
        // to the next child.
        if (item == null) {
            fetchFirstTwoImagesForFolderImpl(
                    childIdIterator, firstDrawable, secondDrawable, callback);
            return;
        }

        fetchImageForBookmark(item, mCallbackController.makeCancelable(drawable -> {
            Drawable newFirstDrawable = firstDrawable;
            Drawable newSecondDrawable = secondDrawable;
            if (newFirstDrawable == null) {
                newFirstDrawable = drawable;
            } else {
                newSecondDrawable = drawable;
            }
            fetchFirstTwoImagesForFolderImpl(
                    childIdIterator, newFirstDrawable, newSecondDrawable, callback);
        }));
    }
}
