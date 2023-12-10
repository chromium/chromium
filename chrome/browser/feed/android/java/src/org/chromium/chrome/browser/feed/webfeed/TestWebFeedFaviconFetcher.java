// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

/** A `WebFeedFaviconFetcher` test fake. */
public class TestWebFeedFaviconFetcher extends WebFeedFaviconFetcher {
    private Callback<Bitmap> mCallback;
    private Bitmap mTestBitmap;

    public TestWebFeedFaviconFetcher() {
        super(null, null);
    }

    @Override
    public void beginFetch(
            int iconSizePx,
            int textSizePx,
            GURL siteUrl,
            GURL faviconUrl,
            Callback<Bitmap> callback) {
        mCallback = callback;
    }

    public void answerWithBitmap() {
        mCallback.onResult(getTestBitmap());
        mCallback = null;
    }

    public void answerWithNull() {
        mCallback.onResult(null);
        mCallback = null;
    }

    public Bitmap getTestBitmap() {
        if (mTestBitmap == null) {
            mTestBitmap =
                    Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ARGB_8888);
        }
        return mTestBitmap;
    }
}
