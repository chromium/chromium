// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the NTP background image. */
@NullMarked
public class NtpBackgroundImageProperties {
    /** The background image bitmap. */
    public static final WritableObjectPropertyKey<Bitmap> BACKGROUND_IMAGE =
            new WritableObjectPropertyKey<>();

    /** The matrix to apply on the background image layout. */
    public static final WritableObjectPropertyKey<Matrix> IMAGE_MATRIX =
            new WritableObjectPropertyKey<>();

    /** The background image scale type. */
    public static final WritableObjectPropertyKey<ImageView.ScaleType> IMAGE_SCALE_TYPE =
            new WritableObjectPropertyKey<>();

    /** The background color. */
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        BACKGROUND_IMAGE, IMAGE_MATRIX, IMAGE_SCALE_TYPE, BACKGROUND_COLOR,
    };
}
