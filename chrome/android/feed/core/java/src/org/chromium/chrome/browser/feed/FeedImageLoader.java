// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.annotation.DrawableRes;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.imageloader.BundledAssets;
import com.google.android.libraries.feed.host.imageloader.ImageLoaderApi;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ThumbnailGradient;

import java.util.Iterator;
import java.util.List;

/**
 * Provides image loading and other host-specific asset fetches for Feed.
 */
public class FeedImageLoader implements ImageLoaderApi {
    private static final String ASSET_PREFIX = "asset://";
    private static final String DRAWABLE_RESOURCE_TYPE = "drawable";
    private static final String OVERLAY_IMAGE_PREFIX = "overlay-image://";
    private static final String OVERLAY_IMAGE_URL_PARAM = "url";
    private static final String OVERLAY_IMAGE_DIRECTION_PARAM = "direction";
    private static final String OVERLAY_IMAGE_DIRECTION_START = "start";
    private static final String OVERLAY_IMAGE_DIRECTION_END = "end";

    private FeedImageLoaderBridge mFeedImageLoaderBridge;
    private Context mActivityContext;

    /**
     * Creates a FeedImageLoader for fetching image for the current user.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     * @param activityContext Context of the user we are rendering the Feed for.
     */
    public FeedImageLoader(Profile profile, Context activityContext) {
        this(profile, activityContext, new FeedImageLoaderBridge());
    }

    /**
     * Creates a FeedImageLoader for fetching image for the current user.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     * @param activityContext Context of the user we are rendering the Feed for.
     * @param bridge The FeedImageLoaderBridge implementation can handle fetching image request.
     */
    public FeedImageLoader(Profile profile, Context activityContext, FeedImageLoaderBridge bridge) {
        mFeedImageLoaderBridge = bridge;
        mFeedImageLoaderBridge.init(profile);
        mActivityContext = activityContext;
    }

    // TODO(gangwu): Handle widthPx and heightPx.
    @Override
    public void loadDrawable(
            List<String> urls, int widthPx, int heightPx, Consumer<Drawable> consumer) {
        assert mFeedImageLoaderBridge != null;
        loadDrawableWithIter(urls.iterator(), widthPx, heightPx, consumer);
    }

    /** Cleans up FeedImageLoaderBridge. */
    public void destroy() {
        assert mFeedImageLoaderBridge != null;
        mFeedImageLoaderBridge.destroy();
        mFeedImageLoaderBridge = null;
    }

    /**
     * Tries to load the next value in urlsIter, and recursively calls itself on failure to
     * continue processing. Being recursive allows resuming after an async call across the bridge.
     *
     * @param urlsIter The stateful iterator of all urls to load. Each call removes one value.
     * @param widthPx The width of the image in pixels. Will be {@link #DIMENSION_UNKNOWN} if
     * unknown.
     * @param heightPx The height of the image in pixels. Will be {@link #DIMENSION_UNKNOWN} if
     * unknown.
     * @param consumer The callback to be given the first successful image.
     */
    private void loadDrawableWithIter(
            Iterator<String> urlsIter, int widthPx, int heightPx, Consumer<Drawable> consumer) {
        assert mFeedImageLoaderBridge != null;
        if (!urlsIter.hasNext()) {
            // Post to ensure callback is not run synchronously.
            ThreadUtils.postOnUiThread(() -> consumer.accept(null));
            return;
        }

        String url = urlsIter.next();
        if (url.startsWith(ASSET_PREFIX)) {
            Drawable drawable = getAssetDrawable(url);
            if (drawable == null) {
                loadDrawableWithIter(urlsIter, widthPx, heightPx, consumer);
            } else {
                // Post to ensure callback is not run synchronously.
                ThreadUtils.postOnUiThread(() -> consumer.accept(drawable));
            }
        } else if (url.startsWith(OVERLAY_IMAGE_PREFIX)) {
            Uri uri = Uri.parse(url);
            int direction = overlayDirection(uri);
            String sourceUrl = uri.getQueryParameter(OVERLAY_IMAGE_URL_PARAM);
            assert !TextUtils.isEmpty(sourceUrl) : "Overlay image source URL empty";
            mFeedImageLoaderBridge.fetchImage(sourceUrl, widthPx, heightPx, (Bitmap bitmap) -> {
                if (bitmap == null) {
                    loadDrawableWithIter(urlsIter, widthPx, heightPx, consumer);
                } else {
                    consumer.accept(ThumbnailGradient.createDrawableWithGradientIfNeeded(
                            bitmap, direction, mActivityContext.getResources()));
                }
            });
        } else {
            mFeedImageLoaderBridge.fetchImage(url, widthPx, heightPx, (Bitmap bitmap) -> {
                if (bitmap == null) {
                    loadDrawableWithIter(urlsIter, widthPx, heightPx, consumer);
                } else {
                    consumer.accept(new BitmapDrawable(mActivityContext.getResources(), bitmap));
                }
            });
        }
    }

    /**
     * @param url The fully qualified name of the resource.
     * @return The resource as a Drawable on success, null otherwise.
     */
    private Drawable getAssetDrawable(String url) {
        String resourceName = url.substring(ASSET_PREFIX.length());
        @DrawableRes
        int id = lookupDrawableIdenfier(resourceName);
        return id == 0 ? null : AppCompatResources.getDrawable(mActivityContext, id);
    }

    /**
     * Translate {@Link BundledAssets} to android drawable resource. This method only translate
     * resource name defined in {@Link BundledAssets}.
     *
     * @param resourceName The name of the drawable asset.
     * @return The id of the drawable asset. May be 0 if it could not be found.
     */
    private @DrawableRes int lookupDrawableIdenfier(String resourceName) {
        switch (resourceName) {
            case BundledAssets.OFFLINE_INDICATOR_BADGE:
                return R.drawable.offline_pin_round;
            case BundledAssets.VIDEO_INDICATOR_BADGE:
                return R.drawable.ic_play_circle_filled_grey;
        }

        return 0;
    }

    /**
     * Returns where the thumbnail is located in the card using the "direction" query param.
     * @param overlayImageUri The URI for the overlay image.
     * @return The direction in which the thumbnail is located relative to the card.
     */
    private int overlayDirection(Uri overlayImageUri) {
        String direction = overlayImageUri.getQueryParameter(OVERLAY_IMAGE_DIRECTION_PARAM);
        assert TextUtils.equals(direction, OVERLAY_IMAGE_DIRECTION_START)
                || TextUtils.equals(direction, OVERLAY_IMAGE_DIRECTION_END)
            : "Overlay image direction must be either start or end";
        return TextUtils.equals(direction, OVERLAY_IMAGE_DIRECTION_START)
                ? ThumbnailGradient.ThumbnailLocation.START
                : ThumbnailGradient.ThumbnailLocation.END;
    }
}
