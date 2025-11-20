// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the NTP background image layout. */
@NullMarked
public class NtpBackgroundImageLayoutViewBinder {
    /** Binds the NTP background image properties to the view. */
    public static void bind(PropertyModel model, FrameLayout view, PropertyKey propertyKey) {
        ImageView imageView = view.findViewById(R.id.image_view);
        View gradientView = view.findViewById(R.id.gradient_view);

        if (propertyKey == NtpBackgroundImageProperties.BACKGROUND_IMAGE) {
            Bitmap bitmap = model.get(NtpBackgroundImageProperties.BACKGROUND_IMAGE);
            imageView.setImageBitmap(bitmap);
            gradientView.setVisibility(bitmap != null ? View.VISIBLE : View.GONE);
        } else if (propertyKey == NtpBackgroundImageProperties.IMAGE_MATRIX) {
            imageView.setImageMatrix(model.get(NtpBackgroundImageProperties.IMAGE_MATRIX));
        } else if (propertyKey == NtpBackgroundImageProperties.IMAGE_SCALE_TYPE) {
            imageView.setScaleType(model.get(NtpBackgroundImageProperties.IMAGE_SCALE_TYPE));
        } else if (propertyKey == NtpBackgroundImageProperties.BACKGROUND_COLOR) {
            imageView.setBackgroundColor(model.get(NtpBackgroundImageProperties.BACKGROUND_COLOR));
        }
    }
}
