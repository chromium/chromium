// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The class to call after thumbnail is retrieved */
@NullMarked
public interface ThumbnailStorageDelegate {
    /**
     * Called when thumbnail has been retrieved.
     *
     * @param contentId Content ID of the thumbnail retrieved.
     * @param bitmap The thumbnail retrieved.
     * @param iconSizePx Icon size for the thumbnail retrieved.
     */
    default void onThumbnailRetrieved(String contentId, @Nullable Bitmap bitmap, int iconSizePx) {}
}
