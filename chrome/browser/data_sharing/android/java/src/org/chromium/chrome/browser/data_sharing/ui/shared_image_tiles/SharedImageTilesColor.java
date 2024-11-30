// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A class to define the color scheme of a {@link SharedImageTilesCoordinator} component. */
public class SharedImageTilesColor {
    @IntDef({Style.DEFAULT, Style.DYNAMIC, Style.TAB_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Style {
        /**
         * Standard SharedImageTiles style: Monotone colored tiles and text colors and transparent
         * border.
         */
        int DEFAULT = 0;

        /** Dynamic color style: Dynamic colored tiles, text colors and transparent border. */
        int DYNAMIC = 1;

        /**
         * Tab group color style: Custom tab group colored tiles, text colors and tab group colored
         * border.
         */
        int TAB_GROUP = 2;
    }

    public final @Style int currentStyle;
    public final @ColorInt int tabGroupColor;

    public SharedImageTilesColor(@Style int style) {
        this(style, 0);
    }

    /**
     * Constructor for {@link SharedImageTilesColor} component. This is post color harmonization
     * styling.
     *
     * @param style The color style to use.
     * @param color The tab group color of a tab group.
     */
    public SharedImageTilesColor(@Style int style, @ColorInt int color) {
        currentStyle = style;
        tabGroupColor = color;
        if (style == Style.TAB_GROUP) {
            assert tabGroupColor != 0;
        }
    }
}
