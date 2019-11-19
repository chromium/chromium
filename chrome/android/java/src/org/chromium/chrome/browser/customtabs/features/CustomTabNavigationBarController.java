// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.app.Activity;
import android.os.Build;
import android.view.Window;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.UiUtils;

/**
 * Configures the system navigation bar while a Custom Tab activity is running.
 *
 * Lifecycle: currently is stateless, so it has only static methods.
 */
public class CustomTabNavigationBarController {

    private CustomTabNavigationBarController() {}

    /**
     * Sets the navigation bar color according to intent extras.
     */
    public static void updateNavigationBarColor(Activity activity,
            CustomTabIntentDataProvider intentDataProvider) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return; // Window#setNavigationBar not available.
        }

        Integer color = intentDataProvider.getNavigationBarColor();
        if (color == null) return;

        Window window = activity.getWindow();

        boolean needsDarkButtons = !ColorUtils.shouldUseLightForegroundOnBackground(color);
        boolean supportsDarkButtons = Build.VERSION.SDK_INT > Build.VERSION_CODES.O;

        if (supportsDarkButtons) {
            UiUtils.setNavigationBarIconColor(window.getDecorView().getRootView(),
                    needsDarkButtons);
        } else if (needsDarkButtons) {
            // Can't make the buttons dark, darken the background instead with the same algorithm
            // as for the status bar.
            color = ColorUtils.getDarkenedColorForStatusBar(color);
        }

        window.setNavigationBarColor(color);

        if (needsDarkButtons && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Add grey divider color if the background is light (similar to
            // TabbedNavigationBarColorController#setNavigationBarColor).
            // bottom_system_nav_divider_color is overridden to black on Q+, so using it's pre-Q
            // value, black_alpha_12, directly.
            window.setNavigationBarDividerColor(ApiCompatibilityUtils.getColor(
                    activity.getResources(), R.color.black_alpha_12));
        }
    }
}
