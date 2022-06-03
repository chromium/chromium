// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.graphics.Bitmap;
import android.util.LruCache;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;

/** Implementation for {@link FaviconHelper}. */
public class FaviconProviderImpl implements FaviconProvider {
    private static final int MAX_NUM_FAVICONS_TO_CACHE = 128;

    private final Profile mProfile;
    private final FaviconHelper mFaviconHelper;
    private final FaviconCache mFaviconCache;

    /** Constructor. */
    public FaviconProviderImpl(Profile profile) {
        mProfile = profile;
        mFaviconHelper = new FaviconHelper();
        mFaviconCache = new FaviconCache(MAX_NUM_FAVICONS_TO_CACHE);
    }

    // FaviconProvider implementation.
    @Override
    public void getFavicon(String url, int faviconSizePx, Callback<Bitmap> callback) {
        Bitmap image = mFaviconCache.getFaviconImage(url);
        if (image != null) {
            callback.onResult(image);
            return;
        }

        FaviconHelper.FaviconImageCallback imageCallback = (bitmap, iconUrl) -> {
            // TODO(shaktisahu): Handle no favicon case.
            if (bitmap != null) mFaviconCache.putFaviconImage(url, bitmap);
            callback.onResult(bitmap);
        };

        mFaviconHelper.getLocalFaviconImageForURL(mProfile, url, faviconSizePx, imageCallback);
    }

    /** An LRU cache for caching the favicons.*/
    private static class FaviconCache {
        private final LruCache<String, Bitmap> mMemoryCache;

        public FaviconCache(int size) {
            mMemoryCache = new LruCache<>(size);
        }

        Bitmap getFaviconImage(String url) {
            return mMemoryCache.get(url);
        }

        public void putFaviconImage(String url, Bitmap image) {
            mMemoryCache.put(url, image);
        }
    }
}
