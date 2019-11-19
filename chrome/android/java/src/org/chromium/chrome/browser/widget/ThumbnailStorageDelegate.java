// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** The class to call after thumbnail is retrieved */
public interface ThumbnailStorageDelegate {
    /**
     * Called when thumbnail has been retrieved.
     * @param contentId Content ID of the thumbnail retrieved.
     * @param bitmap The thumbnail retrieved.
     */
    void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap bitmap);
}