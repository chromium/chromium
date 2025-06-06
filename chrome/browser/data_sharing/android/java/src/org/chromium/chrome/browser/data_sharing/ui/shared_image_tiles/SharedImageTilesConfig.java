// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;
import android.content.res.Resources;
import android.util.Pair;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

/**
 * Config class for the SharedImageTiles UI. By default this component is dynamically colored with a
 * size of 28dp for each image tiles.
 */
@NullMarked
public class SharedImageTilesConfig {
    // --- Sizes ---
    public final @DimenRes int iconSizeDp;
    public final @DimenRes int borderSizeDp;
    public final @DimenRes int textPaddingDp;

    // --- Colors ---
    public final @ColorInt int textColor;
    public final @ColorInt int borderColor;
    public final @ColorInt int backgroundColor;

    // --- Other ---
    public final @StyleRes int textStyle;

    private SharedImageTilesConfig(Builder builder) {
        this.iconSizeDp = builder.mIconSizeDp;
        this.borderSizeDp = builder.mBorderSizeDp;
        this.textPaddingDp = builder.mTextPaddingDp;
        this.textColor = builder.mTextColor;
        this.borderColor = builder.mBorderColor;
        this.backgroundColor = builder.mBackgroundColor;
        this.textStyle = builder.mTextStyle;
    }

    /** Returns the border size and total icon size in {@link Px} units. */
    public Pair<Integer, Integer> getBorderAndTotalIconSizes(Resources res) {
        @Px int borderSize = res.getDimensionPixelSize(this.borderSizeDp);
        @Px int iconTotalSize = res.getDimensionPixelSize(this.iconSizeDp) + 2 * borderSize;
        return Pair.create(borderSize, iconTotalSize);
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;

        if (other instanceof SharedImageTilesConfig otherConfig) {
            return this.iconSizeDp == otherConfig.iconSizeDp
                    && this.borderSizeDp == otherConfig.borderSizeDp
                    && this.textPaddingDp == otherConfig.textPaddingDp
                    && this.textColor == otherConfig.textColor
                    && this.borderColor == otherConfig.borderColor
                    && this.backgroundColor == otherConfig.backgroundColor
                    && this.textStyle == otherConfig.textStyle;
        }
        return false;
    }

    /** Builder class for a {@link SharedImageTilesConfig}. */
    public static class Builder {
        // --- Sizes ---
        private @DimenRes int mIconSizeDp;
        private @DimenRes int mBorderSizeDp;
        private @DimenRes int mTextPaddingDp;

        // --- Colors ---
        private @ColorInt int mTextColor;
        private @ColorInt int mBorderColor;
        private @ColorInt int mBackgroundColor;

        // --- Other ---
        private @StyleRes int mTextStyle;

        /**
         * Default constructor for the builder. Returns a dynamically colored configuration with the
         * most commonly used size.
         *
         * @param context The Android Context.
         */
        public Builder(Context context) {
            // Set default values in the constructor.
            this.mIconSizeDp = R.dimen.shared_image_tiles_icon_height;
            this.mBorderSizeDp = R.dimen.shared_image_tiles_icon_border;
            this.mTextPaddingDp = R.dimen.shared_image_tiles_text_padding;
            this.mTextColor = SemanticColorUtils.getDefaultIconColorOnAccent1Container(context);
            this.mBorderColor = SemanticColorUtils.getDefaultBgColor(context);
            this.mBackgroundColor = SemanticColorUtils.getColorPrimaryContainer(context);
            this.mTextStyle = R.style.TextAppearance_TextAccentMediumThick;
        }

        /**
         * Creates a builder with preset values for a button.
         *
         * @param context The Android context.
         * @return A builder instance configured for a button.
         */
        public static Builder createForButton(Context context) {
            return new Builder(context)
                    .setTextStyle(R.style.TextAppearance_TextAccentMediumThick_Primary);
        }

        /**
         * Creates a builder with preset values for being drawn on a tab group color.
         *
         * @param context The Android context.
         * @param tabGroupColorId The color id of the tab group.
         * @return A builder instance configured for a tab group favicon.
         */
        public static Builder createForTabGroupColorContext(
                Context context, @TabGroupColorId int tabGroupColorId) {
            return new Builder(context)
                    .setIconSizeDp(R.dimen.small_shared_image_tiles_icon_height)
                    .setBorderSizeDp(R.dimen.shared_image_tiles_icon_border)
                    .setTextPaddingDp(R.dimen.small_shared_image_tiles_text_padding)
                    .setTextStyle(R.style.TextAppearance_SharedImageTilesSmall)
                    .setTabGroupColor(context, tabGroupColorId);
        }

        /**
         * Sets the icon size dimension resource.
         *
         * @param iconSizeDp The dimension resource ID for the icon size.
         * @return This builder instance for chaining.
         */
        public Builder setIconSizeDp(@DimenRes int iconSizeDp) {
            this.mIconSizeDp = iconSizeDp;
            return this;
        }

        /**
         * Sets the border size dimension resource.
         *
         * @param borderSizeDp The dimension resource ID for the border size.
         * @return This builder instance for chaining.
         */
        public Builder setBorderSizeDp(@DimenRes int borderSizeDp) {
            this.mBorderSizeDp = borderSizeDp;
            return this;
        }

        /**
         * Sets the text padding dimension resource.
         *
         * @param textPaddingDp The dimension resource ID for the text padding.
         * @return This builder instance for chaining.
         */
        public Builder setTextPaddingDp(@DimenRes int textPaddingDp) {
            this.mTextPaddingDp = textPaddingDp;
            return this;
        }

        /**
         * Sets the text color.
         *
         * @param textColor The color integer for the text.
         * @return This builder instance for chaining.
         */
        public Builder setTextColor(@ColorInt int textColor) {
            this.mTextColor = textColor;
            return this;
        }

        /**
         * Sets the border color.
         *
         * @param borderColor The color integer for the border.
         * @return This builder instance for chaining.
         */
        public Builder setBorderColor(@ColorInt int borderColor) {
            this.mBorderColor = borderColor;
            return this;
        }

        /**
         * Sets the background color.
         *
         * @param backgroundColor The color integer for the background.
         * @return This builder instance for chaining.
         */
        public Builder setBackgroundColor(@ColorInt int backgroundColor) {
            this.mBackgroundColor = backgroundColor;
            return this;
        }

        /**
         * Sets the text style resource.
         *
         * @param textStyle The style resource ID for the text.
         * @return This builder instance for chaining.
         */
        public Builder setTextStyle(@StyleRes int textStyle) {
            this.mTextStyle = textStyle;
            return this;
        }

        /**
         * Sets the a new tab group color and updates all relevant colors to match.
         *
         * @param context The Android context.
         * @param tabGroupColorId The color associated with the tab group.
         * @return This builder instance for chaining.
         */
        public Builder setTabGroupColor(Context context, @TabGroupColorId int tabGroupColorId) {
            @ColorInt
            int tabGroupColor =
                    TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                            context, tabGroupColorId, false);
            setBorderColor(tabGroupColor);
            setBackgroundColor(tabGroupColor);
            @ColorRes
            int textColorRes =
                    TabGroupColorPickerUtils.shouldUseDarkTextColorOnTabGroupColor(tabGroupColorId)
                            ? R.color.small_shared_image_tiles_text_color_dark
                            : R.color.small_shared_image_tiles_text_color_light;
            setTextColor(ContextCompat.getColor(context, textColorRes));

            return this;
        }

        /**
         * Builds a {@link SharedImageTilesConfig} object with the current configuration.
         *
         * @return A new SharedImageTilesConfig object.
         */
        public SharedImageTilesConfig build() {
            return new SharedImageTilesConfig(this);
        }
    }
}
