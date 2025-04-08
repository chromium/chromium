// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.content.Context;

import androidx.annotation.ColorInt;

import com.google.android.material.elevation.ElevationOverlayProvider;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Utility class that provides theme related attributes for price change UI. */
public class PriceChangeModuleViewUtils {
    static @ColorInt int getBackgroundColor(Context context) {
        return SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    static @ColorInt int getIconColor(Context context) {
        float tabElevation = context.getResources().getDimension(R.dimen.tab_bg_elevation);
        return new ElevationOverlayProvider(context)
                .compositeOverlayWithThemeSurfaceColorIfNeeded(tabElevation);
    }
}
