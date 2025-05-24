// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Utility class that provides theme related attributes for price change UI. */
@NullMarked
public class PriceChangeModuleViewUtils {
    static @ColorInt int getBackgroundColor(Context context) {
        return SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    static @ColorInt int getIconColor(Context context) {
        return SemanticColorUtils.getColorSurfaceContainerHighest(context);
    }
}
