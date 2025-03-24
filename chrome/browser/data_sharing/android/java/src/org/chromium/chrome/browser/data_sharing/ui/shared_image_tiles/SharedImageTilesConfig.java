// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

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
            this.mTextColor = SemanticColorUtils.getDefaultTextColorAccent1(context);
            this.mBorderColor = SemanticColorUtils.getDefaultBgColor(context);
            this.mBackgroundColor = SemanticColorUtils.getColorPrimaryContainer(context);
            this.mTextStyle = R.style.TextAppearance_TextAccentMediumThick_Primary;
        }

        /**
         * Creates a builder with preset values for a tab group thumbnail.
         *
         * @param context The Android context.
         * @param tabGroupColor The color of the tab group.
         * @return A builder instance configured for a tab group thumbnail.
         */
        public static Builder createThumbnail(Context context, @ColorInt int tabGroupColor) {
            return new Builder(context)
                    .setIconSizeDp(R.dimen.small_shared_image_tiles_icon_height)
                    .setBorderSizeDp(R.dimen.shared_image_tiles_icon_border)
                    .setTextPaddingDp(R.dimen.small_shared_image_tiles_text_padding)
                    .setTextColor(
                            ChromeColors.getSurfaceColor(context, R.dimen.default_bg_elevation))
                    .setBorderColor(tabGroupColor)
                    .setBackgroundColor(tabGroupColor)
                    .setTextStyle(R.style.TextAppearance_SharedImageTilesSmall);
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
         * @param tabGroupColor The color associated with the tab group.
         * @return This builder instance for chaining.
         */
        public Builder setTabGroupColor(@ColorInt int tabGroupColor) {
            setBorderColor(tabGroupColor);
            setBackgroundColor(tabGroupColor);
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
