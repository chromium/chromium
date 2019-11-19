// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill_assistant.AssistantDimension;

/** Java equivalent to {@code OverlayImageProto}. */
public class AssistantOverlayImage {
    /** The url of the image to display. */
    public final String mImageUrl;
    /** The image to display, after {@code mImageUrl} has been resolved. */
    public @Nullable Bitmap mImageBitmap;
    /** The size of the image to display. */
    public final @Nullable AssistantDimension mImageSize;
    /** The margin between the top of the page (anchor) and the image. */
    public final @Nullable AssistantDimension mImageTopMargin;
    /** The margin between image and text. */
    public final @Nullable AssistantDimension mImageBottomMargin;
    /** The text to display beneath the image. */
    public final String mText;
    /** The color of the text to draw */
    public final @ColorInt int mTextColor;
    /** The size of the text to display. */
    public final @Nullable AssistantDimension mTextSize;

    public AssistantOverlayImage(String imageUrl, @Nullable AssistantDimension imageSize,
            @Nullable AssistantDimension imageTopMargin,
            @Nullable AssistantDimension imageBottomMargin, String text, @ColorInt int textColor,
            @Nullable AssistantDimension textSize) {
        mImageUrl = imageUrl;
        mImageSize = imageSize;
        mImageTopMargin = imageTopMargin;
        mImageBottomMargin = imageBottomMargin;
        mText = text;
        mTextColor = textColor;
        mTextSize = textSize;
    }
}
