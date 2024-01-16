// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Util class to handle various color operations shared between hub classes. */
public final class HubColors {
    private HubColors() {}

    public static @HubColorScheme int getColorSchemeSafe(@Nullable Pane pane) {
        return pane == null ? HubColorScheme.DEFAULT : pane.getColorScheme();
    }

    public static @ColorInt int getBackgroundColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultBgColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_bg_color_dark);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }
}
