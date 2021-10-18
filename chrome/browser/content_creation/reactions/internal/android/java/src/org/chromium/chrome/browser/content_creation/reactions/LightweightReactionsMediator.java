// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.List;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Mediator for the Lightweight Reactions component.
 */
public class LightweightReactionsMediator {
    private final ImageFetcher mImageFetcher;

    public LightweightReactionsMediator(ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }

    /**
     * Fetches the image at the given URL and invokes the given callback with a {@link Bitmap}
     * representing it.
     */
    public void getBitmapForUrl(String url, Callback<Bitmap> callback) {
        mImageFetcher.fetchImage(
                ImageFetcher.Params.create(url, ImageFetcher.LIGHTWEIGHT_REACTIONS_UMA_CLIENT_NAME),
                bitmap -> { callback.onResult(bitmap); });
    }

    /**
     * Fetches the GIF at the given URL and invokes the given callback with the fetched asset
     * as a {@link BaseGifImage}.
     */
    public void getGifForUrl(String url, Callback<BaseGifImage> callback) {
        mImageFetcher.fetchGif(
                ImageFetcher.Params.create(url, ImageFetcher.LIGHTWEIGHT_REACTIONS_UMA_CLIENT_NAME),
                gifImage -> { callback.onResult(gifImage); });
    }

    /**
     * Fetches the thumbnail and GIF payload for each given reaction. When done, the given callback
     * is invoked with a list of thumbnails with the same order as the given reaction list. The GIF
     * assets are not returned, but the image fetcher will cache them on disk for instant access
     * when needed by the UI.
     */
    public void fetchAssetsAndGetThumbnails(
            List<ReactionMetadata> reactions, Callback<Bitmap[]> callback) {
        if (callback == null) {
            return;
        }

        if (reactions == null || reactions.isEmpty()) {
            callback.onResult(null);
            return;
        }

        // Keep track of the number of callbacks received (two per reaction expected). Need a
        // final instance because the counter is updated from within a callback.
        final Counter counter = new Counter(reactions.size() * 2);

        // Also use a final array to keep track of the thumbnails fetched so far. Initialize it with
        // null refs so the fetched bitmaps can be inserted at the right index.
        final Bitmap[] thumbnails = new Bitmap[reactions.size()];

        for (int i = 0; i < reactions.size(); ++i) {
            // Capture the loop index to a final instance for use in the callback.
            final int index = i;

            ReactionMetadata reaction = reactions.get(i);
            getBitmapForUrl(reaction.thumbnailUrl, bitmap -> {
                thumbnails[index] = bitmap;
                counter.increment();

                if (counter.isDone()) {
                    callback.onResult(thumbnails);
                }
            });
            getGifForUrl(reaction.thumbnailUrl, gif -> {
                counter.increment();

                if (counter.isDone()) {
                    callback.onResult(thumbnails);
                }
            });
        }
    }

    /**
     * Simple counter class used to keep track of the number of images being
     * asynchronously loaded.
     */
    private class Counter {
        private int mRemainingCalls;

        Counter(int expectedCalls) {
            mRemainingCalls = expectedCalls;
        }

        void increment() {
            --mRemainingCalls;
        }

        boolean isDone() {
            return mRemainingCalls == 0;
        }
    }
}
