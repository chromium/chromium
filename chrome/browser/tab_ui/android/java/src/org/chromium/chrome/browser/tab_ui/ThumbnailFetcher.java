// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.drawable.Drawable;
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;

/** An object to obtain thumbnails asynchronously. */
@NullMarked
public class ThumbnailFetcher {
    private final ThumbnailProvider mThumbnailProvider;
    private final MultiThumbnailMetadata mMultiThumbnailMetadata;
    private @Nullable CallbackController mCurrentCallbackController;

    /**
     * Constructs a {@link ThumbnailFetcher}.
     *
     * @param thumbnailProvider The mechanism to send callbacks to to provide thumbnails.
     * @param metadata The metadata of the tab or group to fetch a thumbnail for.
     */
    public ThumbnailFetcher(ThumbnailProvider thumbnailProvider, MultiThumbnailMetadata metadata) {
        mThumbnailProvider = thumbnailProvider;
        mMultiThumbnailMetadata = metadata;
    }

    /**
     * Fetches a thumbnail.
     *
     * @param thumbnailSize The size of the thumbnail that will be rendered on screen.
     * @param isSelected Whether the tab is currently selected.
     * @param callback The callback to invoke with the resultant drawable.
     */
    public void fetch(
            Size thumbnailSize, boolean isSelected, Callback<@Nullable Drawable> callback) {
        mThumbnailProvider.getTabThumbnailWithCallback(
                mMultiThumbnailMetadata,
                thumbnailSize,
                isSelected,
                createCancelableCallback(callback));
    }

    /** Cancels any ongoing fetches. */
    public void cancel() {
        if (mCurrentCallbackController != null) {
            mCurrentCallbackController.destroy();
            mCurrentCallbackController = null;
        }
    }

    @SuppressWarnings("NullAway")
    private Callback<@Nullable Drawable> createCancelableCallback(
            Callback<@Nullable Drawable> callback) {
        cancel();
        mCurrentCallbackController = new CallbackController();
        return mCurrentCallbackController.makeCancelable(callback);
    }
}
