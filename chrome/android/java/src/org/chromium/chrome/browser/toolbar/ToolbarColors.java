// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.support.v7.content.res.AppCompatResources;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.util.ColorUtils;

/**
 * Helpers to determine colors in toolbars.
 */
public class ToolbarColors {
    private static final float LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 0.2f;

    /**
     * Determine the text box background color given the current tab.
     * @param res {@link Resources} used to retrieve colors.
     * @param tab The current {@link Tab}
     * @param color The color of the toolbar background.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static int getTextBoxColorForToolbarBackground(
            Resources res, @Nullable Tab tab, int color) {
        boolean isIncognito = tab != null && tab.isIncognito();
        if (tab != null && tab.getNativePage() instanceof NewTabPage) {
            NewTabPage page = (NewTabPage) tab.getNativePage();
            if (page.isLocationBarShownInNTP()) {
                return page.isLocationBarScrolledToTopInNtp()
                        ? ApiCompatibilityUtils.getColor(res, R.color.toolbar_text_box_background)
                        : ChromeColors.getPrimaryBackgroundColor(res, false);
            }
        }

        return ToolbarColors.getTextBoxColorForToolbarBackgroundInNonNativePage(
                res, color, isIncognito);
    }

    /**
     * Determine the text box background color given a toolbar background color
     * @param res {@link Resources} used to retrieve colors.
     * @param color The color of the toolbar background.
     * @param isIncognito Whether or not the color is used for incognito mode.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static int getTextBoxColorForToolbarBackgroundInNonNativePage(
            Resources res, int color, boolean isIncognito) {
        // Text box color on default toolbar background in incognito mode is a pre-defined
        // color. We calculate the equivalent opaque color from the pre-defined translucent color.
        if (isIncognito) {
            final int overlayColor = ApiCompatibilityUtils.getColor(
                    res, R.color.toolbar_text_box_background_incognito);
            final float overlayColorAlpha = Color.alpha(overlayColor) / 255f;
            final int overlayColorOpaque = overlayColor & 0xFF000000;
            return ColorUtils.getColorWithOverlay(color, overlayColorOpaque, overlayColorAlpha);
        }

        // Text box color on default toolbar background in standard mode is a pre-defined
        // color instead of a calculated color.
        if (isUsingDefaultToolbarColor(res, false, color)) {
            return ApiCompatibilityUtils.getColor(res, R.color.toolbar_text_box_background);
        }

        // TODO(mdjones): Clean up shouldUseOpaqueTextboxBackground logic.
        if (ColorUtils.shouldUseOpaqueTextboxBackground(color)) return Color.WHITE;

        return ColorUtils.getColorWithOverlay(
                color, Color.WHITE, LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA);
    }

    /**
     * @param tab The {@link Tab} on which the toolbar scene layer color is used.
     * @return The toolbar (or browser controls) color used in the compositor scene layer. Note that
     *         this is primarily used for compositor animation, and doesn't affect the Android view.
     */
    public static @ColorInt int getToolbarSceneLayerBackground(Tab tab) {
        // On NTP, the toolbar background is tinted as the NTP background color, so return NTP
        // background color here to make animation smoother.
        if (tab.getNativePage() instanceof NewTabPage) {
            if (((NewTabPage) tab.getNativePage()).isLocationBarShownInNTP()) {
                return tab.getNativePage().getBackgroundColor();
            }
        }

        return TabThemeColorHelper.getColor(tab);
    }

    /**
     * @return Alpha for the textbox given a Tab.
     */
    public static float getTextBoxAlphaForToolbarBackground(Tab tab) {
        if (tab.getNativePage() instanceof NewTabPage) {
            if (((NewTabPage) tab.getNativePage()).isLocationBarShownInNTP()) return 0f;
        }
        int color = TabThemeColorHelper.getColor(tab);
        return ColorUtils.shouldUseOpaqueTextboxBackground(color)
                ? 1f
                : LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
    }

    /**
     * Test if the toolbar is using the default color.
     * @param resources The resources to get the toolbar primary color.
     * @param isIncognito Whether to retrieve the default theme color for incognito mode.
     * @param color The color that the toolbar is using.
     * @return If the color is the default toolbar color.
     */
    public static boolean isUsingDefaultToolbarColor(
            Resources resources, boolean isIncognito, int color) {
        return color == ChromeColors.getDefaultThemeColor(resources, isIncognito);
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorRes} for the icon tint of themed toolbar.
     */
    public static @ColorRes int getThemedToolbarIconTintRes(boolean useLight) {
        // Light toolbar theme colors may be used in night mode, so use toolbar_icon_tint_dark which
        // is not overridden in night- resources.
        return useLight ? R.color.tint_on_dark_bg : R.color.toolbar_icon_tint_dark;
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param context The {@link Context} used to retrieve colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorStateList} for the icon tint of themed toolbar.
     */
    public static ColorStateList getThemedToolbarIconTint(Context context, boolean useLight) {
        return AppCompatResources.getColorStateList(context, getThemedToolbarIconTintRes(useLight));
    }
}
