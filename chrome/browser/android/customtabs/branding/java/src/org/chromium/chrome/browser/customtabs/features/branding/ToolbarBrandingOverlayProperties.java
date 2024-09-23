// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.ColorInt;

import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties of the toolbar branding overlay. */
public class ToolbarBrandingOverlayProperties {
    /** Color data used to determine the background, text and icon tint. */
    public static class ColorData {
        private final @ColorInt int mBackground;
        private final @BrandedColorScheme int mBrandedColorScheme;

        public ColorData(@ColorInt int background, @BrandedColorScheme int brandedColorScheme) {
            mBackground = background;
            mBrandedColorScheme = brandedColorScheme;
        }

        public @ColorInt int getBackground() {
            return mBackground;
        }

        public @BrandedColorScheme int getBrandedColorScheme() {
            return mBrandedColorScheme;
        }
    }

    /** The color data including background color and color scheme. */
    public static final PropertyModel.WritableObjectPropertyKey<ColorData> COLOR_DATA =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The ratio of the hiding animation progress of the overlay, float in the range [0, 1]. */
    public static final PropertyModel.WritableFloatPropertyKey HIDING_PROGRESS =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {COLOR_DATA, HIDING_PROGRESS};
}
