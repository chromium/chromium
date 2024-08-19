// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;
import android.util.Size;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;

/**
 * The object to set to {@link TabProperties#THUMBNAIL_FETCHER} for the TabGridViewBinder to obtain
 * the thumbnail asynchronously.
 */
public class ThumbnailFetcher {
    private final ThumbnailProvider mThumbnailProvider;
    private final int mTabId;
    private @Nullable CallbackController mCurrentCallbackController;

    /**
     * @param thumbnailProvider The mechanism to send callbacks to to provide thumbnails.
     * @param tabId The ID of the tab to fetch a thumbnail for.
     */
    ThumbnailFetcher(ThumbnailProvider thumbnailProvider, int tabId) {
        mThumbnailProvider = thumbnailProvider;
        mTabId = tabId;
    }

    /**
     * Fetches a thumbnail.
     *
     * @param thumbnailSize The size of the thumbnail that will be rendered on screen.
     * @param isSelected Whether the tab is currently selected.
     * @param callback The callback to invoke with the resultant drawable.
     */
    void fetch(Size thumbnailSize, boolean isSelected, Callback<Drawable> callback) {
        mThumbnailProvider.getTabThumbnailWithCallback(
                mTabId, thumbnailSize, isSelected, createCancelableCallback(callback));
    }

    /** Cancel any ongoing fetches. */
    void cancel() {
        if (mCurrentCallbackController != null) {
            mCurrentCallbackController.destroy();
            mCurrentCallbackController = null;
        }
    }

    private Callback<Drawable> createCancelableCallback(Callback<Drawable> callback) {
        cancel();
        mCurrentCallbackController = new CallbackController();
        return mCurrentCallbackController.makeCancelable(callback);
    }
}
