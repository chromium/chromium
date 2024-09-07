// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;

import org.chromium.base.Callback;

/** {@link ThumbnailProvider} adapter for {@link TabContentManager}. */
public class TabContentManagerThumbnailProvider implements ThumbnailProvider {
    private final TabContentManager mTabContentManager;

    /**
     * @param tabContentManager The {@link TabContentManager} to use.
     */
    public TabContentManagerThumbnailProvider(TabContentManager tabContentManager) {
        mTabContentManager = tabContentManager;
    }

    @Override
    public void getTabThumbnailWithCallback(
            int tabId, Size thumbnailSize, boolean isSelected, Callback<Drawable> callback) {

        mTabContentManager.getTabThumbnailWithCallback(
                tabId, thumbnailSize, adaptCallback(callback));
    }

    private static Callback<Bitmap> adaptCallback(Callback<Drawable> callback) {
        return (Bitmap bitmap) -> {
            Drawable drawable = null;
            if (bitmap != null) {
                drawable = new BitmapDrawable(bitmap);
            }
            callback.onResult(drawable);
        };
    }
}
