// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.Bitmap;
import android.util.Size;

import org.chromium.base.Callback;

/** An interface to get the thumbnails to be shown inside the tab grid cards. */
public interface ThumbnailProvider {
    /**
     * @see TabContentManager#getTabThumbnailWithCallback
     */
    void getTabThumbnailWithCallback(
            int tabId,
            Size thumbnailSize,
            Callback<Bitmap> callback,
            boolean forceUpdate,
            boolean writeToCache,
            boolean isSelected);
}
