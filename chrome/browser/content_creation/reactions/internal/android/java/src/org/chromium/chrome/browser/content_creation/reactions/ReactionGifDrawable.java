// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.graphics.Bitmap;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * An extension of the BaseGifDrawable class that allows stepping through the frames of the GIF
 * without updating the View, to facilitate decoding the GIF for exporting purposes.
 */
public class ReactionGifDrawable extends BaseGifDrawable {
    public ReactionGifDrawable(BaseGifImage gifImage, Bitmap.Config bitmapConfig) {
        super(gifImage, bitmapConfig);
    }
}
