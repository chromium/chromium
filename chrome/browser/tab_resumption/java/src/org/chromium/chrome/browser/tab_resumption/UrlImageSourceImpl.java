// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;

import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;

/** UrlImageProvider.UrlImageSource implementation for production. */
public class UrlImageSourceImpl implements UrlImageProvider.UrlImageSource {
    private final Context mContext;
    private final TabContentManagerThumbnailProvider mTabContentManagerThumbnailProvider;

    UrlImageSourceImpl(Context context, TabContentManager tabContentManager) {
        mContext = context;
        mTabContentManagerThumbnailProvider =
                new TabContentManagerThumbnailProvider(tabContentManager);
    }

    @Override
    public ThumbnailProvider createThumbnailProvider() {
        return mTabContentManagerThumbnailProvider;
    }

    @Override
    public RoundedIconGenerator createIconGenerator() {
        return FaviconUtils.createRoundedRectangleIconGenerator(mContext);
    }
}
