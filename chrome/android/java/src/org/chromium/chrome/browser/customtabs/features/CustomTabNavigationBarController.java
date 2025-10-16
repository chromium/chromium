// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.content.Context;
import android.graphics.Color;
import android.view.Window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.ui.UiUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.util.ColorUtils;

/**
 * Configures the system navigation bar while a Custom Tab activity is running.
 *
 * Lifecycle: currently is stateless, so it has only static methods.
 */
@NullMarked
public class CustomTabNavigationBarController {

    private CustomTabNavigationBarController() {}

    /**
     * Sets the navigation bar color and navigation divider color according to intent extras, or
     * whether CCT is drawing edge to edge
     *
     * @param window The activity window.
     * @param intentDataProvider The {@link BrowserServicesIntentDataProvider} used in CCT.
     * @param context The current Android context.
     * @param isEdgeToEdge Whether CCT is drawing edge to edge.
     * @param systemBarColorHelper The {@link EdgeToEdgeSystemBarColorHelper} to set the system bar
     *     colors.
     */
    public static void update(
            Window window,
            BrowserServicesIntentDataProvider intentDataProvider,
            Context context,
            boolean isEdgeToEdge,
            @Nullable EdgeToEdgeSystemBarColorHelper systemBarColorHelper) {
        // When drawing edge to edge, always use transparent color for the navigation bar.
        if (isEdgeToEdge) {
            updateBarColor(window, Color.TRANSPARENT, false, false, systemBarColorHelper);
            return;
        }

        Integer navigationBarColor = intentDataProvider.getColorProvider().getNavigationBarColor();
        Integer navigationBarDividerColor =
                intentDataProvider.getColorProvider().getNavigationBarDividerColor();
        // TODO(b/300419189): Pass the CCT Top Bar Color in AGSA intent after Page Insights Hub is
        // launched
        if (GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(intentDataProvider)) {
            navigationBarColor = context.getColor(R.color.google_bottom_bar_background_color);
            navigationBarDividerColor =
                    context.getColor(R.color.google_bottom_bar_background_color);
        }
        // PCCT is deemed incapable of system dark button support due to the way it implements
        // partial height (window coordinate translation). We do the darkening ourselves.
        boolean supportsDarkButtons = !intentDataProvider.isPartialCustomTab();
        boolean needsDarkButtons =
                navigationBarColor != null
                        && !ColorUtils.shouldUseLightForegroundOnBackground(navigationBarColor);

        updateBarColor(
                window,
                navigationBarColor,
                supportsDarkButtons,
                needsDarkButtons,
                systemBarColorHelper);

        Integer dividerColor =
                getDividerColor(
                        context, navigationBarColor, navigationBarDividerColor, needsDarkButtons);

        if (dividerColor != null) {
            if (systemBarColorHelper != null) {
                systemBarColorHelper.setNavigationBarDividerColor(dividerColor);
            } else {
                window.setNavigationBarDividerColor(dividerColor);
            }
        }
    }

    /** Sets the navigation bar color according to intent extras. */
    private static void updateBarColor(
            Window window,
            @Nullable Integer navigationBarColor,
            boolean supportsDarkButtons,
            boolean needsDarkButtons,
            @Nullable EdgeToEdgeSystemBarColorHelper systemBarColorHelper) {
        if (navigationBarColor == null) return;

        if (supportsDarkButtons) {
            UiUtils.setNavigationBarIconColor(
                    window.getDecorView().getRootView(), needsDarkButtons);
        } else if (needsDarkButtons) {
            // Can't make the buttons dark, darken the background instead with the same algorithm
            // as for the status bar.
            navigationBarColor = ColorUtils.getDarkenedColorForStatusBar(navigationBarColor);
        }

        if (systemBarColorHelper != null) {
            systemBarColorHelper.setNavigationBarColor(navigationBarColor);
        } else {
            window.setNavigationBarColor(navigationBarColor);
        }
    }

    /**
     * Return the navigation bar divider color.
     * @param context {@link Context} to get color resource from.
     * @param navigationBarColor Color of the navigation bar.
     * @param navigationBarDividerColor Color of the divider.
     * @param needsDarkButtons Whether the buttons and the bar has a low contrast.
     */
    public static @Nullable Integer getDividerColor(
            Context context,
            @Nullable Integer navigationBarColor,
            @Nullable Integer navigationBarDividerColor,
            boolean needsDarkButtons) {
        if (navigationBarDividerColor == null && navigationBarColor != null && needsDarkButtons) {
            // Add grey divider color if the background is light (similar to
            // TabbedNavigationBarColorController#setNavigationBarColor).
            // bottom_system_nav_divider_color is overridden to black on Q+, so using it's pre-Q
            // value, black_alpha_12, directly.
            return context.getColor(R.color.black_alpha_12);
        }
        return navigationBarDividerColor;
    }
}
