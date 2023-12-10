// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Bitmap;
import android.webkit.WebHistoryItem;

import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.url.GURL;

/**
 * WebView Chromium implementation of WebHistoryItem. Simple immutable wrapper
 * around NavigationEntry
 */
@SuppressWarnings("deprecation")
public class WebHistoryItemChromium extends WebHistoryItem {
    private final GURL mUrl;
    private final GURL mOriginalUrl;
    private final String mTitle;
    private final Bitmap mFavicon;

    /* package */ WebHistoryItemChromium(NavigationEntry entry) {
        mUrl = entry.getUrl();
        mOriginalUrl = entry.getOriginalUrl();
        mTitle = entry.getTitle();
        mFavicon = entry.getFavicon();
    }

    /** See {@link android.webkit.WebHistoryItem#getId}. */
    @Override
    public int getId() {
        // This method is deprecated in superclass. Returning constant -1 now.
        return -1;
    }

    /** See {@link android.webkit.WebHistoryItem#getUrl}. */
    @Override
    public String getUrl() {
        return mUrl.getSpec();
    }

    /** See {@link android.webkit.WebHistoryItem#getOriginalUrl}. */
    @Override
    public String getOriginalUrl() {
        return mOriginalUrl.getSpec();
    }

    /** See {@link android.webkit.WebHistoryItem#getTitle}. */
    @Override
    public String getTitle() {
        return mTitle;
    }

    /** See {@link android.webkit.WebHistoryItem#getFavicon}. */
    @Override
    public Bitmap getFavicon() {
        return mFavicon;
    }

    // Clone constructor.
    private WebHistoryItemChromium(GURL url, GURL originalUrl, String title, Bitmap favicon) {
        mUrl = url;
        mOriginalUrl = originalUrl;
        mTitle = title;
        mFavicon = favicon;
    }

    /** See {@link android.webkit.WebHistoryItem#clone}. */
    @SuppressWarnings("NoSynchronizedMethodCheck")
    @Override
    public synchronized WebHistoryItemChromium clone() {
        return new WebHistoryItemChromium(mUrl, mOriginalUrl, mTitle, mFavicon);
    }
}
