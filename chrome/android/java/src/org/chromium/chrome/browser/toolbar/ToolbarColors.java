// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/**
 * Helpers to determine colors in toolbars.
 */
public class ToolbarColors {
    private static final float LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 0.2f;

    /**
     * Determine the text box background color given the current tab.
     * @param res {@link Resources} used to retrieve colors.
     * @param tab The current {@link Tab}
     * @param backgroundColor The color of the toolbar background.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static @ColorInt int getTextBoxColorForToolbarBackground(
            Resources res, @Nullable Tab tab, @ColorInt int backgroundColor) {
        boolean isIncognito = tab != null && tab.isIncognito();
        @ColorInt
        int defaultColor = getTextBoxColorForToolbarBackgroundInNonNativePage(
                res, backgroundColor, isIncognito);
        NativePage nativePage = tab != null ? tab.getNativePage() : null;
        return nativePage != null ? nativePage.getToolbarTextBoxBackgroundColor(defaultColor)
                                  : defaultColor;
    }

    /**
     * Determine the text box background color given a toolbar background color
     * @param res {@link Resources} used to retrieve colors.
     * @param color The color of the toolbar background.
     * @param isIncognito Whether or not the color is used for incognito mode.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static @ColorInt int getTextBoxColorForToolbarBackgroundInNonNativePage(
            Resources res, @ColorInt int color, boolean isIncognito) {
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
     * Returns whether the incognito toolbar theme color can be used in overview mode.
     */
    public static boolean canUseIncognitoToolbarThemeColorInOverview() {
        final boolean isAccessibilityEnabled = DeviceClassManager.enableAccessibilityLayout();
        final boolean isHorizontalTabSwitcherEnabled = ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
        final boolean isTabGridEnabled = TabUiFeatureUtilities.isGridTabSwitcherEnabled();
        final boolean isStartSurfaceEnabled = StartSurfaceConfiguration.isStartSurfaceEnabled();
        return (isAccessibilityEnabled || isHorizontalTabSwitcherEnabled || isTabGridEnabled
                || isStartSurfaceEnabled);
    }
}
