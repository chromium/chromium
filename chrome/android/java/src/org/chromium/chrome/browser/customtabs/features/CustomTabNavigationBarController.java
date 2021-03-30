// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.content.res.Resources;
import android.os.Build;
import android.view.Window;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;

/**
 * Configures the system navigation bar while a Custom Tab activity is running.
 *
 * Lifecycle: currently is stateless, so it has only static methods.
 */
public class CustomTabNavigationBarController {

    private CustomTabNavigationBarController() {}

    /**
     * Sets the navigation bar color and navigation divider color according to intent extras.
     */
    public static void update(Window window, BrowserServicesIntentDataProvider intentDataProvider,
            Resources resources) {
        Integer navigationBarColor = intentDataProvider.getNavigationBarColor();
        Integer navigationBarDividerColor = intentDataProvider.getNavigationBarDividerColor();

        int lightBackgroundDividerColor = ApiCompatibilityUtils.getColor(
                resources, org.chromium.chrome.R.color.black_alpha_12);

        boolean needsDarkButtons = navigationBarColor != null
                && !ColorUtils.shouldUseLightForegroundOnBackground(navigationBarColor);

        updateBarColor(window, navigationBarColor, needsDarkButtons);
        updateDividerColor(window, navigationBarColor, navigationBarDividerColor,
                lightBackgroundDividerColor, needsDarkButtons);
    }

    /**
     * Sets the navigation bar color according to intent extras.
     */
    private static void updateBarColor(
            Window window, Integer navigationBarColor, boolean needsDarkButtons) {
        if (navigationBarColor == null) return;

        boolean supportsDarkButtons = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;

        if (supportsDarkButtons) {
            UiUtils.setNavigationBarIconColor(window.getDecorView().getRootView(),
                    needsDarkButtons);
        } else if (needsDarkButtons) {
            // Can't make the buttons dark, darken the background instead with the same algorithm
            // as for the status bar.
            navigationBarColor = ColorUtils.getDarkenedColorForStatusBar(navigationBarColor);
        }

        window.setNavigationBarColor(navigationBarColor);
    }

    /**
     * Sets the navigation bar divider color according to intent extras.
     */
    private static void updateDividerColor(Window window, Integer navigationBarColor,
            Integer navigationBarDividerColor, int lightBackgroundDividerColor,
            boolean needsDarkButtons) {
        // navigationBarDividerColor can only be set in Android P+
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        if (navigationBarDividerColor != null) {
            window.setNavigationBarDividerColor(navigationBarDividerColor);
        } else if (navigationBarColor != null && needsDarkButtons) {
            // Add grey divider color if the background is light (similar to
            // TabbedNavigationBarColorController#setNavigationBarColor).
            // bottom_system_nav_divider_color is overridden to black on Q+, so using it's pre-Q
            // value, black_alpha_12, directly.
            window.setNavigationBarDividerColor(lightBackgroundDividerColor);
        }
    }
}
