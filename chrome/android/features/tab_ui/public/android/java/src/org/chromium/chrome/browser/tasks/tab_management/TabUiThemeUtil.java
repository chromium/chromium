// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/**
 * Common tab UI theme utils for public use. Internal themes are provided via @{@link
 * TabUiThemeProvider}
 */
@NullMarked
public class TabUiThemeUtil {
    public static final float FOLIO_FOOT_LENGTH_DP = 16.f;
    private static final float MAX_TAB_STRIP_TAB_WIDTH_DP = 265.f;
    private static final float DIVIDER_FOLIO_LIGHT_OPACITY = 0.3f;

    /**
     * Returns the tab strip background color based on the windowing mode and activity focus state.
     * To get the default strip background color that is not affected by the activity focus state,
     * use {@link #getTabStripBackgroundColor(Context, boolean)}.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param isInDesktopWindow Whether the app is in a desktop window.
     * @param isActivityFocused Whether the activity containing the tab strip is focused.
     * @return The {@link ColorInt} for the tab strip background.
     */
    public static @ColorInt int getTabStripBackgroundColor(
            Context context,
            boolean isIncognito,
            boolean isInDesktopWindow,
            boolean isActivityFocused) {
        return isInDesktopWindow && !isActivityFocused
                ? getTabStripBackgroundColorUnfocused(context, isIncognito)
                : getTabStripBackgroundColorDefault(context, isIncognito);
    }

    /**
     * Returns the default color for the tab strip background, that does not take the activity focus
     * state into account.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorInt} for the tab strip background.
     */
    public static @ColorInt int getTabStripBackgroundColor(Context context, boolean isIncognito) {
        return getTabStripBackgroundColorDefault(context, isIncognito);
    }

    private static @ColorInt int getTabStripBackgroundColorDefault(
            Context context, boolean isIncognito) {
        // TODO(https://crbug.com/413067043): Update for incognito.
        if (isIncognito) {
            return ContextCompat.getColor(context, R.color.tab_strip_tablet_bg_incognito);
        }
        return SurfaceColorUpdateUtils.getTabStripBackgroundColorDefault(context);
    }

    private static @ColorInt int getTabStripBackgroundColorUnfocused(
            Context context, boolean isIncognito) {
        // TODO(https://crbug.com/413067043): Update for incognito.
        if (isIncognito) {
            return ContextCompat.getColor(context, R.color.tab_strip_tablet_bg_unfocused_incognito);
        }
        return SurfaceColorUpdateUtils.getTabStripBackgroundColorUnfocused(context);
    }

    /** Returns the tab strip selected tab color. */
    public static @ColorInt int getTabStripSelectedTabColor(Context context, boolean isIncognito) {
        return SurfaceColorUpdateUtils.getDefaultThemeColor(context, isIncognito);
    }

    /** Returns the tab strip title text color. */
    public static @ColorInt int getTabTextColor(Context context, boolean isIncognito) {
        return context.getColor(
                isIncognito
                        ? R.color.compositor_tab_title_bar_text_incognito
                        : R.color.compositor_tab_title_bar_text);
    }

    /**
     * Returns the mini thumbnail placeholder color for the given group color.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param groupColor The group color that will be composited with the mini thumbnail placeholder
     *     overlay color.
     */
    public static @ColorInt int getMiniThumbnailPlaceholderColorForGroup(
            Context context, boolean isIncognito, @ColorInt int groupColor) {
        @ColorRes
        int foregroundRes =
                isIncognito
                        ? R.color.mini_thumbnail_placeholder_overlay_color_baseline
                        : R.color.mini_thumbnail_placeholder_overlay_color;
        @ColorInt int foregroundColor = context.getColor(foregroundRes);
        return androidx.core.graphics.ColorUtils.compositeColors(foregroundColor, groupColor);
    }

    /**
     * Returns the color used for the shared group notification bubble.
     *
     * @param context {@link Context} used to retrieve color.
     * @return The color for the notification bubble.
     */
    public static @ColorInt int getGroupTitleBubbleColor(Context context) {
        return ChromeColors.getDefaultBgColor(context, /* isIncognito= */ false);
    }

    public static @ColorInt int getReorderBackgroundColor(Context context, boolean isIncognito) {
        if (isIncognito) return context.getColor(R.color.gm3_baseline_surface_container_high_dark);
        return ColorUtils.inNightMode(context)
                ? SemanticColorUtils.getColorSurfaceContainerHigh(context)
                : SemanticColorUtils.getColorSurfaceContainerLow(context);
    }

    /** Returns the color for the hovered tab container. */
    public static @ColorInt int getHoveredTabContainerColor(Context context, boolean isIncognito) {
        int baseColor =
                isIncognito
                        ? context.getColor(R.color.baseline_primary_80)
                        : ChromeSemanticColorUtils.getTabInactiveHoverColor(context);
        float alpha =
                ResourcesCompat.getFloat(
                        context.getResources(), R.dimen.tsr_folio_tab_inactive_hover_alpha);
        return ColorUtils.setAlphaComponentWithFloat(baseColor, alpha);
    }

    /** Returns the color for the tab strip startup "ghost" containers. */
    public static @ColorInt int getTabStripStartupContainerColor(Context context) {
        return context.getColor(R.color.bg_tabstrip_tab_folio_startup_tint);
    }

    public static @DrawableRes int getTabResource() {
        return R.drawable.bg_tabstrip_tab_folio;
    }

    public static @DrawableRes int getDetachedResource() {
        return R.drawable.bg_tabstrip_tab_detached;
    }

    public static float getMaxTabStripTabWidthDp() {
        return MAX_TAB_STRIP_TAB_WIDTH_DP;
    }

    /**
     * @return The tint color resource for the tab divider.
     */
    public static @ColorInt int getDividerTint(Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.tab_strip_tablet_divider_bg_incognito);
        }

        if (!ColorUtils.inNightMode(context)) {
            // This color will not be used at full opacity. We can't set this using the alpha
            // component of the {@code @ColorInt}, since it is ignored when loading resources
            // with a specified tint in the CC layer (instead retaining the alpha of the original
            // image). Instead, this is reflected by setting the opacity of the divider itself.
            // See https://crbug.com/1373634.
            return ColorUtils.setAlphaComponentWithFloat(
                    SemanticColorUtils.getDefaultIconColorAccent1(context),
                    DIVIDER_FOLIO_LIGHT_OPACITY);
        }

        return SemanticColorUtils.getDividerLineBgColor(context);
    }

    /** {@return The {@link DrawableRes} for the keyboard focus ring for tabs} */
    public static @DrawableRes int getTabKeyboardFocusDrawableRes() {
        return R.drawable.tabstrip_keyfocus_8dp;
    }

    /** {@return The {@link DrawableRes} for the keyboard focus ring for selected tab w/ outline} */
    public static @DrawableRes int getSelectedTabInTabGroupKeyboardFocusDrawableRes() {
        return R.drawable.tabstrip_keyfocus_10dp;
    }

    /** {@return The {@link DrawableRes} for the tab group indicator keyboard focus ring} */
    public static @DrawableRes int getTabGroupIndicatorKeyboardFocusDrawableRes() {
        return R.drawable.tabstrip_keyfocus_11dp;
    }

    /** {@return The {@link DrawableRes} for the close button keyboard focus ring} */
    public static @DrawableRes int getCircularButtonKeyboardFocusDrawableRes() {
        return R.drawable.circular_button_keyfocus;
    }

    /** {@return The keyboard focus ring's offset in px} */
    public static int getFocusRingOffset(Context context) {
        return context.getResources().getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset);
    }

    /** {@return The width of the keyboard focus ring stroke and tab group color line in px} */
    public static int getLineWidth(Context context) {
        return context.getResources().getDimensionPixelSize(R.dimen.tabstrip_strokewidth);
    }
}
