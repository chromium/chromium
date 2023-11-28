// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/**
 * Common tab UI theme utils for public use.
 * Internal themes are provided via @{@link TabUiThemeProvider}
 */
public class TabUiThemeUtil {
    private static final float MAX_TAB_STRIP_TAB_WIDTH_DP = 265.f;
    private static final float DETACHED_TAB_OVERLAY_ALPHA = 0.85f;
    private static final float DETACHED_TAB_OVERLAY_ALPHA_EDIT_MODE = 0.2f;

    /**
     * Returns the color for the tab strip background.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The {@link ColorInt} for tab strip redesign background.
     */
    public static @ColorInt int getTabStripBackgroundColor(Context context, boolean isIncognito) {
        // Use black color for incognito and night mode for folio.
        if (isIncognito) {
            return context.getColor(R.color.default_bg_color_dark_elev_2_baseline);
        }

        if (ColorUtils.inNightMode(context)) {
            return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_2);
        }

        return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_3);
    }

    /**
     * Returns the color for the tab container based on experiment arm, incognito mode, foreground,
     * reordering, placeholder, and hover state.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param foreground Whether the tab is in the foreground.
     * @param isReordering Whether the tab is being reordered.
     * @param isPlaceholder Whether the tab is a placeholder "ghost" tab.
     * @param isHovered Whether the tab is hovered on.
     * @return The color for the tab container.
     */
    // TODO (crbug.com/1469465): Encapsulate tab properties in a state object.
    public static int getTabStripContainerColor(
            Context context,
            boolean isIncognito,
            boolean foreground,
            boolean isReordering,
            boolean isPlaceholder,
            boolean isHovered) {
        if (foreground) {
            return ChromeColors.getDefaultThemeColor(context, isIncognito);
        } else if (isHovered) {
            return getHoveredTabContainerColor(context, isIncognito);
        } else if (isPlaceholder) {
            return getTabStripStartupContainerColor(context);
        } else {
            return getSurfaceColorElev0(context, isIncognito);
        }
    }

    /** Returns the color for the hovered tab container. */
    private static @ColorInt int getHoveredTabContainerColor(Context context, boolean isIncognito) {
        int baseColor =
                isIncognito
                        ? context.getColor(R.color.baseline_primary_80)
                        : ChromeSemanticColorUtils.getTabInactiveHoverColor(context);
        float alpha =
                ResourcesCompat.getFloat(
                        context.getResources(), R.dimen.tsr_folio_tab_inactive_hover_alpha);
        return ColorUtils.setAlphaComponent(baseColor, (int) (alpha * 255));
    }

    /** Returns the color for the tab strip startup "ghost" containers. */
    private static @ColorInt int getTabStripStartupContainerColor(Context context) {
        return context.getColor(R.color.bg_tabstrip_tab_folio_startup_tint);
    }

    /**
     * Returns the value that corresponds to Surface-0 based on incognito status.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The value that corresponds to Surface-0.
     */
    private static int getSurfaceColorElev0(Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.default_bg_color_dark);
        }

        return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_0);
    }

    /**
     * Returns the value that corresponds to Surface-5 based on incognito status.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The value that corresponds to Surface-5.
     */
    private static int getSurfaceColorElev5(Context context, boolean isIncognito) {
        if (isIncognito) {
            return context.getColor(R.color.default_bg_color_dark_elev_5_baseline);
        }

        return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_5);
    }

    public static int getTabResource() {
        return R.drawable.bg_tabstrip_tab_folio;
    }

    public static int getDetachedResource() {
        return R.drawable.bg_tabstrip_tab_detached;
    }

    public static float getMaxTabStripTabWidthDp() {
        return MAX_TAB_STRIP_TAB_WIDTH_DP;
    }
}
