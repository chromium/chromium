// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.StyleRes;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.elevation.ElevationOverlayProvider;

/** Utility class that provides theme related attributes for price change UI. */
public class PriceChangeModuleViewUtils {
    static @ColorInt int getBackgroundColor(Context context) {
        int alpha =
                context.getResources().getInteger(R.integer.tab_thumbnail_placeholder_color_alpha);
        @StyleRes int styleRes = R.style.TabThumbnailPlaceholderStyle;
        TypedArray ta =
                context.obtainStyledAttributes(styleRes, R.styleable.TabThumbnailPlaceholder);
        @ColorInt
        int baseColor =
                ta.getColor(R.styleable.TabThumbnailPlaceholder_colorTileBase, Color.TRANSPARENT);
        float tileSurfaceElevation =
                ta.getDimension(R.styleable.TabThumbnailPlaceholder_elevationTileBase, 0);
        ta.recycle();
        if (tileSurfaceElevation != 0) {
            ElevationOverlayProvider eop = new ElevationOverlayProvider(context);
            baseColor = eop.compositeOverlay(baseColor, tileSurfaceElevation);
        }
        return MaterialColors.compositeARGBWithAlpha(baseColor, alpha);
    }

    static @ColorInt int getIconColor(Context context) {
        float tabElevation = context.getResources().getDimension(R.dimen.tab_bg_elevation);
        return new ElevationOverlayProvider(context)
                .compositeOverlayWithThemeSurfaceColorIfNeeded(tabElevation);
    }
}
