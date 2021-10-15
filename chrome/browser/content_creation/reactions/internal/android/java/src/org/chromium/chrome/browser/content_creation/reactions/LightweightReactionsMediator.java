// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.components.image_fetcher.ImageFetcher;

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
}
