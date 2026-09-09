// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.ColorUtils;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.components.browser_ui.util.DimensionCompat;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

/**
 * Utility methods for providing colors and styles specifically for tab cards in the grid tab
 * switcher. For any new, general-purpose tab UI theming utilities, please add them to
 * TabUiThemeUtil.java or TabUiThemeProvider.java instead.
 */
@NullMarked
public class TabCardThemeUtil {
    private static final String TAG = "TabCardThemeUtil";

    @VisibleForTesting public static final float PORTRAIT_THUMBNAIL_ASPECT_RATIO = 0.85f;

    /**
     * Returns the tint color for Chrome owned favicon based on the incognito mode or selected.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The tint color for Chrome owned favicon.
     */
    public static @ColorInt int getChromeOwnedFaviconTintColor(
            Context context, boolean isIncognito, boolean isSelected) {
        return getTitleTextColor(context, isIncognito, isSelected, null);
    }

    /**
     * Returns the ColorStateList for alert indicator based on the incognito mode or selected.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     */
    public static ColorStateList getAlertIndicatorColorStateList(
            Context context, boolean isIncognito, boolean isSelected) {
        return ColorStateList.valueOf(
                getChromeOwnedFaviconTintColor(context, isIncognito, isSelected));
    }

    /**
     * Returns the title text appearance for the tab grid card based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The text appearance for the tab grid card title.
     */
    public static @ColorInt int getTitleTextColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_title_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG);
        } else {
            if (colorId != null) {
                return TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        context, colorId, isIncognito);
            }
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_title_color)
                    : SemanticColorUtils.getDefaultTextColor(context);
        }
    }

    /**
     * Returns the mini-thumbnail placeholder color for the multi-thumbnail tab grid card based on
     * the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The mini-thumbnail placeholder color.
     */
    public static @ColorInt int getMiniThumbnailPlaceholderColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            if (isIncognito) {
                return context.getColor(R.color.incognito_tab_thumbnail_placeholder_selected_color);
            }
            int alpha =
                    context.getResources()
                            .getInteger(R.integer.tab_thumbnail_placeholder_selected_color_alpha);
            @ColorInt int baseColor = SemanticColorUtils.getColorOnPrimary(context);
            return MaterialColors.compositeARGBWithAlpha(baseColor, alpha);
        }
        if (colorId != null) {
            return TabGroupColorPickerUtils.getTabGroupCardMiniThumbnailPlaceholderColor(
                    context, colorId, isIncognito);
        }
        if (isIncognito) {
            return context.getColor(R.color.incognito_tab_thumbnail_placeholder_color);
        }
        return SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    /**
     * Returns the color to use for the tab grid card view background based on incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The {@link ColorInt} for tab grid card view background.
     */
    public static @ColorInt int getCardViewBackgroundColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            // Incognito does not use dynamic colors, so it can use colors from resources.
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_bg_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorPrimary, TAG);
        } else {
            if (colorId != null) {
                return TabGroupColorPickerUtils.getTabGroupCardColor(context, colorId, isIncognito);
            }
            return isIncognito
                    ? ContextCompat.getColor(
                            context, R.color.gm3_baseline_surface_container_highest_dark)
                    : SemanticColorUtils.getColorSurfaceContainerHighest(context);
        }
    }

    /**
     * Returns the color used for an empty thumbnail placeholder illustration.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The {@link ColorInt} for the empty thumbnail placeholder illustration.
     */
    public static @ColorInt int getEmptyThumbnailColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        return getCardViewBackgroundColor(context, isIncognito, isSelected, colorId);
    }

    /**
     * Returns the color to use for the tab grid card hover view background.
     *
     * @param context {@link Context} used to retrieve color.
     * @param backgroundColor current background color of a tab.
     * @return The {@link ColorInt} for tab grid card view background.
     */
    public static ColorStateList getCardViewBackgroundColorStateList(
            Context context, boolean isIncognito, @ColorInt int backgroundColor) {
        @ColorRes
        int overlayColorRes =
                isIncognito
                        ? R.color.incognito_tab_card_hover_color_overlay
                        : R.color.color_primary_with_alpha_16;

        // Calculate the final hovered color by blending the overlay with the base color.
        @ColorInt
        int hoverColor =
                ColorUtils.compositeColors(context.getColor(overlayColorRes), backgroundColor);

        return new ColorStateList(
                new int[][] {new int[] {android.R.attr.state_hovered}, new int[] {}},
                new int[] {hoverColor, backgroundColor});
    }

    /**
     * Returns the text color for the number used on the tab group cards based on the incognito
     * mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     * @return The text color for the number used on the tab group cards.
     */
    public static @ColorInt int getTabGroupNumberTextColor(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            return isIncognito
                    ? context.getColor(R.color.incognito_tab_tile_number_selected_color)
                    : MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG);
        }
        if (colorId != null) {
            return TabGroupColorPickerUtils.getTabGroupCardTextColor(context, colorId, isIncognito);
        }
        return isIncognito
                ? context.getColor(R.color.incognito_tab_tile_number_color)
                : SemanticColorUtils.getDefaultTextColor(context);
    }

    /**
     * Returns the {@link ColorStateList} to use for the tab grid card action button based on
     * incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorStateList} for tab grid card action button.
     */
    public static ColorStateList getActionButtonTintList(
            Context context,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        if (isSelected) {
            return isIncognito
                    ? context.getColorStateList(R.color.incognito_tab_action_button_selected_color)
                    : ColorStateList.valueOf(
                            MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG));
        }
        if (colorId != null) {
            return ColorStateList.valueOf(
                    TabGroupColorPickerUtils.getTabGroupCardTextColor(
                            context, colorId, isIncognito));
        }
        return isIncognito
                ? context.getColorStateList(R.color.incognito_tab_action_button_color)
                : ColorStateList.valueOf(
                        MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG));
    }

    /**
     * Returns the {@link ColorStateList} to use for the selectable tab grid card toggle button
     * based on incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isSelected Whether the tab is currently selected.
     * @return The {@link ColorStateList} for selectable tab grid card toggle button.
     */
    public static ColorStateList getToggleActionButtonBackgroundTintList(
            Context context, boolean isIncognito, boolean isSelected) {
        return getActionButtonTintList(context, isIncognito, isSelected, /* colorId= */ null);
    }

    /**
     * Returns the mini-thumbnail frame color based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The mini-thumbnail frame color.
     */
    public static @ColorInt int getMiniThumbnailFrameColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.tab_grid_card_divider_tint_color_incognito)
                : SemanticColorUtils.getTabGridCardDividerTintColor(context);
    }

    /**
     * Returns the favicon background color based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The favicon background color.
     */
    public static @ColorInt int getFaviconBackgroundColor(Context context, boolean isIncognito) {
        return isIncognito
                ? context.getColor(R.color.favicon_background_color_incognito)
                : SemanticColorUtils.getColorSurfaceBright(context);
    }

    /**
     * Returns the space represented by dimension for spaces between mini thumbnails in a group tab.
     *
     * @param context {@link Context} to retrieve dimension.
     * @return The padding between mini thumbnails in float number.
     */
    public static float getTabMiniThumbnailPaddingDimension(Context context) {
        return context.getResources().getDimension(R.dimen.tab_grid_card_thumbnail_margin);
    }

    /**
     * Returns the aspect ratio for grid tab card based on form factor and orientation.
     *
     * @param context Context of the application.
     * @param browserControlsStateProvider For getting browser controls height.
     * @return Aspect ratio for the grid tab card.
     */
    public static float getTabThumbnailAspectRatio(
            Context context, BrowserControlsStateProvider browserControlsStateProvider) {
        if (context.getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE) {
            assert browserControlsStateProvider != null;
            int browserControlsHeightDp =
                    (browserControlsStateProvider == null)
                            ? 0
                            : Math.round(
                                    (float) browserControlsStateProvider.getTopControlsHeight()
                                            / context.getResources().getDisplayMetrics().density);
            int horizontalAutomotiveToolbarHeightDp =
                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(context);
            int verticalAutomotiveToolbarWidthDp =
                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(context);
            DimensionCompat dimensionCompat = getDimensionCompat(context);
            float windowWidthDp = getWindowWidthDp(dimensionCompat, context);
            float windowHeightDp = getWindowHeightExcludingSystemBarsDp(dimensionCompat, context);
            // This should match the aspect ratio of a Tab's content area.
            return (windowWidthDp - verticalAutomotiveToolbarWidthDp)
                    / (windowHeightDp
                            - browserControlsHeightDp
                            - horizontalAutomotiveToolbarHeightDp);
        }
        // This is an experimentally determined value.
        return PORTRAIT_THUMBNAIL_ASPECT_RATIO;
    }

    private static float getWindowWidthDp(DimensionCompat compat, Context context) {
        return compat.getWindowWidth() / context.getResources().getDisplayMetrics().density;
    }

    private static float getWindowHeightExcludingSystemBarsDp(
            DimensionCompat compat, Context context) {
        return (compat.getWindowHeight() - compat.getNavbarHeight() - compat.getStatusBarHeight())
                / context.getResources().getDisplayMetrics().density;
    }

    private static DimensionCompat getDimensionCompat(Context context) {
        // (TODO: crbug.com/351854698) Pass activity context instead.
        Activity activity = ContextUtils.activityFromContext(context);
        assert activity != null : "Activity from context should not be null for this class.";
        return DimensionCompat.create(activity, null);
    }
}
