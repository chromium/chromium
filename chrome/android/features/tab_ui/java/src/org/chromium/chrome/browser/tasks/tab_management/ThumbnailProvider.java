// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;

/**
 * An interface to get the thumbnails to be shown inside the tab grid cards.
 */
public interface ThumbnailProvider {
    /**
     * @see TabContentManager#getTabThumbnailWithCallback
     */
    void getTabThumbnailWithCallback(int tabId, Size thumbnailSize, Callback<Bitmap> callback,
            boolean forceUpdate, boolean writeToCache, boolean isSelected);
}
