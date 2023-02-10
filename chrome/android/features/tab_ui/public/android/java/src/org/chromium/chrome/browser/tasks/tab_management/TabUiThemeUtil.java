// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/**
 * Common tab UI theme utils for public use.
 * Internal themes are provided via @{@link TabUiThemeProvider}
 */
public class TabUiThemeUtil {
    private static final String TAG = "TabUiThemeProvider";
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
        if (TabManagementFieldTrial.isTabStripFolioEnabled()) {
            // Use black color for incognito and night mode for folio.
            if (isIncognito || ColorUtils.inNightMode(context)) {
                return Color.BLACK;
            }

            int elevationDimenId = ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                    ? R.dimen.default_elevation_3
                    : R.dimen.default_elevation_2;
            return ChromeColors.getSurfaceColor(context, elevationDimenId);
        } else if (TabManagementFieldTrial.isTabStripDetachedEnabled()) {
            if (isIncognito) {
                // Use a non-dynamic dark background color for incognito, slightly greyer than
                // Color.BLACK
                return ChromeColors.getPrimaryBackgroundColor(context, isIncognito);
            }
            return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_0);
        }
        return Color.BLACK;
    }

    /**
     * Returns the color for the tab container based on experiment arm, incognito mode, and
     * foreground status.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @param foreground Whether the tab is in the foreground.
     * @return The color for the tab container.
     */
    public static int getTabStripContainerColor(
            Context context, boolean isIncognito, boolean foreground, boolean isReordering) {
        if (foreground) {
            if (TabManagementFieldTrial.isTabStripFolioEnabled()) {
                return ChromeColors.getDefaultThemeColor(context, isIncognito);
            } else if (TabManagementFieldTrial.isTabStripDetachedEnabled()) {
                return getTabStripDetachedTabColor(context, isIncognito, isReordering);
            }
        } else {
            if (TabManagementFieldTrial.isTabStripFolioEnabled()) {
                return getSurfaceColorElev0(context, isIncognito);
            } else if (TabManagementFieldTrial.isTabStripDetachedEnabled()) {
                return getSurfaceColorElev5(context, isIncognito);
            }
        }

        // Should be unreachable as TSR should never be enabled without the folio or detached arm.
        return Color.TRANSPARENT;
    }

    /**
     * Returns the color for the detached tab container based on the incognito mode.
     *
     * @param context {@link Context} used to retrieve color.
     * @param isIncognito Whether the color is used for incognito mode.
     * @return The color for the detached tab container.
     */
    private static int getTabStripDetachedTabColor(
            Context context, boolean isIncognito, boolean isReordering) {
        assert TabManagementFieldTrial.isTabStripDetachedEnabled();

        if (isReordering) {
            if (isIncognito) {
                return context.getColor(R.color.default_bg_color_dark_elev_4_baseline);
            } else {
                final int baseColor = getTabStripBackgroundColor(context, isIncognito);
                final int overlayColor = SemanticColorUtils.getDefaultControlColorActive(context);

                return ColorUtils.getColorWithOverlay(
                        baseColor, overlayColor, DETACHED_TAB_OVERLAY_ALPHA_EDIT_MODE);
            }
        }

        if (isIncognito) return Color.BLACK;

        if (!ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                && ColorUtils.inNightMode(context)) {
            final int baseColor = SemanticColorUtils.getDefaultControlColorActive(context);
            final int overlayColor =
                    ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_0);

            return ColorUtils.getColorWithOverlay(
                    baseColor, overlayColor, DETACHED_TAB_OVERLAY_ALPHA);
        } else {
            return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_5);
        }
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
            return context.getColor(ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                            ? R.color.default_bg_color_dark_elev_5_gm3_baseline
                            : R.color.default_bg_color_dark_elev_5_baseline);
        }

        return ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_5);
    }

    public static int getTSRTabResource() {
        if (TabManagementFieldTrial.isTabStripFolioEnabled()) {
            return getTSRFolioResource();
        }
        return getTSRDetachedResource();
    }

    public static int getTSRFolioResource() {
        return R.drawable.bg_tabstrip_tab_folio;
    }

    public static int getTSRDetachedResource() {
        return R.drawable.bg_tabstrip_tab_detached;
    }

    public static float getMaxTabStripTabWidthDp() {
        return MAX_TAB_STRIP_TAB_WIDTH_DP;
    }
}
