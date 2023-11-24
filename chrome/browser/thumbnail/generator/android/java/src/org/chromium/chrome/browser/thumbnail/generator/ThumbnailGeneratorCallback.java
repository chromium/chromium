// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** The class to call back to after thumbnail has been generated */
public interface ThumbnailGeneratorCallback {
    /**
     * Called when thumbnail has been generated.
     * @param contentId Content ID of the thumbnail.
     * @param bitmap The thumbnail.
     * @param iconSizePx The requested size (maximum dimension (pixel) of the smaller side) of the
     * thumbnail to be retrieved.
     */
    void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap bitmap, int iconSizePx);
}
