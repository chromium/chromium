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
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.page_image_service.mojom.ClientId;
import org.chromium.url.GURL;

import java.util.Iterator;

/** Class which encapsulates fetching images for bookmarks. */
public class BookmarkImageFetcher {
    private final Profile mProfile;
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final CallbackController mCallbackController = new CallbackController();
    private final ImageFetcher mImageFetcher;
    private final RoundedIconGenerator mRoundedIconGenerator;
    private final ImageServiceBridge mImageServiceBridge;
    private final FaviconHelper mFaviconHelper;
    private final DefaultFaviconHelper mDefaultFaviconHelper;

    /**
     * @param profile The current profile to use.
     * @param context The context used to create drawables.
     * @param bookmarkModel The bookmark model used to query information on bookmarks.
     * @param imageFetcher The image fetcher used to fetch images.
     * @param roundedIconGenerator The rounded icon generator.
     */
    public BookmarkImageFetcher(
            Profile profile,
            Context context,
            BookmarkModel bookmarkModel,
            ImageFetcher imageFetcher,
            RoundedIconGenerator roundedIconGenerator) {
        this(
                profile,
                context,
                bookmarkModel,
                imageFetcher,
                roundedIconGenerator,
                new ImageServiceBridge(
                        ClientId.BOOKMARKS,
                        ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                        profile.getOriginalProfile(),
                        imageFetcher),
                new FaviconHelper(),
                new DefaultFaviconHelper());
    }

    BookmarkImageFetcher(
            Profile profile,
            Context context,
            BookmarkModel bookmarkModel,
            ImageFetcher imageFetcher,
            RoundedIconGenerator roundedIconGenerator,
            ImageServiceBridge imageServiceBridge,
            FaviconHelper faviconHelper,
            DefaultFaviconHelper defaultFaviconHelper) {
        mProfile = profile;
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mImageFetcher = imageFetcher;
        mRoundedIconGenerator = roundedIconGenerator;
        mImageServiceBridge = imageServiceBridge;
        mFaviconHelper = faviconHelper;
        mDefaultFaviconHelper = defaultFaviconHelper;
    }

    /** Destroys this object. */
    public void destroy() {
        mCallbackController.destroy();
        mImageServiceBridge.destroy();
        mFaviconHelper.destroy();
    }

    /**
     * Returns the first two images for the given folder.
     *
     * @param folder The folder to fetch the images for.
     * @param imageSize The size of the image.
     * @param callback The callback to receive the images.
     */
    public void fetchFirstTwoImagesForFolder(
            BookmarkItem folder, int imageSize, Callback<Pair<Drawable, Drawable>> callback) {
        fetchFirstTwoImagesForFolderImpl(
                mBookmarkModel.getChildIds(folder.getId()).iterator(),
                /* firstDrawable= */ null,
                /* secondDrawable= */ null,
                imageSize,
                callback);
    }

    /**
     * Returns a drawable with the image for the given bookmark. If none is found, then it falls
     * back to the favicon
     *
     * @param item The bookmark to fetch the image for.
     * @param imageSize The size for the image.
     * @param callback The callback to receive the image.
     */
    public void fetchImageForBookmarkWithFaviconFallback(
            BookmarkItem item, int imageSize, Callback<Drawable> callback) {
        fetchImageForBookmark(
                item,
                imageSize,
                mCallbackController.makeCancelable(
                        drawable -> {
                            if (drawable == null) {
                                fetchFaviconForBookmark(item, callback);
                            } else {
                                callback.onResult(drawable);
                            }
                        }));
    }

    /**
     * Fetches a favicon for the given bookmark.
     *
     * @param item The bookmark to fetch the image for.
     * @param callback The callback to receive the favicon.
     */
    public void fetchFaviconForBookmark(BookmarkItem item, Callback<Drawable> callback) {
        GURL faviconUrl = item.getUrl();
        int faviconDisplaySize = BookmarkUtils.getFaviconDisplaySize(mContext.getResources());
        FaviconHelper.FaviconImageCallback faviconCallback =
                (image, iconUrl) -> {
                    callback.onResult(
                            FaviconUtils.getIconDrawableWithFilter(
                                    image,
                                    faviconUrl,
                                    mRoundedIconGenerator,
                                    mDefaultFaviconHelper,
                                    mContext,
                                    faviconDisplaySize));
                };

        if (mImageServiceBridge.hasConsentToFetchImages(item.isAccountBookmark())) {
            mFaviconHelper.getForeignFaviconImageForURL(
                    mProfile, faviconUrl, faviconDisplaySize, faviconCallback);
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile, faviconUrl, faviconDisplaySize, faviconCallback);
        }
    }

    private void fetchImageForBookmark(
            BookmarkItem item, int imageSize, Callback<Drawable> callback) {
        final Callback<Bitmap> imageCallback =
                mCallbackController.makeCancelable(
                        (image) -> {
                            if (image == null) {
                                callback.onResult(null);
                            } else {
                                callback.onResult(
                                        new BitmapDrawable(mContext.getResources(), image));
                            }
                        });

        // Price-tracable bookmarks already have image URLs in their metadata. Prioritize that meta
        // when it's available because the coverage is much higher.
        PowerBookmarkMeta meta = mBookmarkModel.getPowerBookmarkMeta(item.getId());
        if (meta != null && meta.hasShoppingSpecifics() && meta.hasLeadImage()) {
            mImageFetcher.fetchImage(
                    ImageFetcher.Params.create(
                            meta.getLeadImage().getUrl(),
                            ImageFetcher.POWER_BOOKMARKS_CLIENT_NAME,
                            imageSize,
                            imageSize),
                    imageCallback);
            return;
        }

        // This call may invoke the callback immediately if the url is cached.
        mImageServiceBridge.fetchImageFor(
                item.isAccountBookmark(), item.getUrl(), imageSize, imageCallback);
    }

    private void fetchFirstTwoImagesForFolderImpl(
            Iterator<BookmarkId> childIdIterator,
            Drawable firstDrawable,
            Drawable secondDrawable,
            int imageSize,
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
                    childIdIterator, firstDrawable, secondDrawable, imageSize, callback);
            return;
        }

        fetchImageForBookmark(
                item,
                imageSize,
                mCallbackController.makeCancelable(
                        drawable -> {
                            Drawable newFirstDrawable = firstDrawable;
                            Drawable newSecondDrawable = secondDrawable;
                            if (newFirstDrawable == null) {
                                newFirstDrawable = drawable;
                            } else {
                                newSecondDrawable = drawable;
                            }
                            fetchFirstTwoImagesForFolderImpl(
                                    childIdIterator,
                                    newFirstDrawable,
                                    newSecondDrawable,
                                    imageSize,
                                    callback);
                        }));
    }
}
