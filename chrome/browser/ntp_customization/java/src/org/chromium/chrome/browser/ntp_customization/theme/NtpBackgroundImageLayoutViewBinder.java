// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageProperties.BACKGROUND_IMAGE;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageProperties.DENSITY;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageProperties.IMAGE_MATRIX;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageProperties.IMAGE_SCALE_TYPE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the NTP background image layout. */
@NullMarked
public class NtpBackgroundImageLayoutViewBinder {
    /** Binds the NTP background image properties to the view. */
    public static void bind(
            PropertyModel model, NtpBackgroundImageLayout layout, PropertyKey propertyKey) {
        if (propertyKey == BACKGROUND_IMAGE) {
            layout.setBitmap(model.get(BACKGROUND_IMAGE));
        } else if (propertyKey == IMAGE_MATRIX) {
            layout.setImageMatrix(model.get(IMAGE_MATRIX));
        } else if (propertyKey == IMAGE_SCALE_TYPE) {
            layout.setScaleType(model.get(IMAGE_SCALE_TYPE));
        } else if (propertyKey == BACKGROUND_COLOR) {
            layout.setBackgroundColor(model.get(BACKGROUND_COLOR));
        } else if (propertyKey == DENSITY) {
            layout.setDensity(model.get(DENSITY));
        }
    }
}
