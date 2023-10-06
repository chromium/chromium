// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.graphics.Bitmap;
import android.util.Size;

import com.ark.browser.tab.core.ITab;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;

/**
 * An interface to get the thumbnails to be shown inside the tab grid cards.
 */
public interface ThumbnailProvider {
    /**
     * @see TabContentManager#getPageThumbnailWithCallback
     */
    void getTabThumbnailWithCallback(ITab tab, Size thumbnailSize,
                                     Callback<Bitmap> callback,
                                     boolean forceUpdate, boolean writeToCache, boolean isSelected);

    void getPageThumbnailWithCallback(PageInfo pageInfo, Size thumbnailSize,
                                      Callback<Bitmap> callback,
                                      boolean forceUpdate, boolean writeToCache, boolean isSelected);
}
