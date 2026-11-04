// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.content.Context;
import android.graphics.Color;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.util.AttrUtils;
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
        update(
                window,
                intentDataProvider,
                context,
                isEdgeToEdge,
                systemBarColorHelper,
                /* edgeToEdgeContentColor= */ null);
    }

    /**
     * Sets the navigation bar color and navigation divider color according to intent extras, or
     * transparent when drawing edge-to-edge.
     *
     * @param edgeToEdgeContentColor The color of the page content rendered behind the transparent
     *     navigation bar when drawing edge-to-edge, e.g. the page theme color. Used to keep the
     *     navigation button appearance readable over the content; light buttons are used when null.
     */
    public static void update(
            Window window,
            BrowserServicesIntentDataProvider intentDataProvider,
            Context context,
            boolean isEdgeToEdge,
            @Nullable EdgeToEdgeSystemBarColorHelper systemBarColorHelper,
            @Nullable Integer edgeToEdgeContentColor) {
        // When drawing edge to edge, always use transparent color for the navigation bar. The
        // buttons render over page content, so their appearance follows the content color.
        if (isEdgeToEdge) {
            boolean needsDarkButtons =
                    edgeToEdgeContentColor != null
                            && !ColorUtils.shouldUseLightForegroundOnBackground(
                                    edgeToEdgeContentColor);
            // A divider applied while fitted (e.g. the implicit light-bar divider) must not
            // survive into edge-to-edge, where there is no bar to divide from.
            updateBarColor(
                    window,
                    Color.TRANSPARENT,
                    /* dividerColor= */ Color.TRANSPARENT,
                    /* supportsDarkButtons= */ true,
                    needsDarkButtons,
                    systemBarColorHelper);
            return;
        }

        Integer navigationBarColor = intentDataProvider.getColorProvider().getNavigationBarColor();
        Integer navigationBarDividerColor =
                intentDataProvider.getColorProvider().getNavigationBarDividerColor();
        // TODO(crbug.com/465719853): Pass the CCT Top Bar Color in AGSA intent after Google Bottom Bar is
        // launched
        if (GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(intentDataProvider)) {
            navigationBarColor = context.getColor(R.color.google_bottom_bar_background_color);
            navigationBarDividerColor =
                    context.getColor(R.color.google_bottom_bar_background_color);
        }
        if (navigationBarColor == null
                && systemBarColorHelper != null
                && systemBarColorHelper.getNavigationBarColor() == Color.TRANSPARENT) {
            // The app has no intent/manifest navigation bar color and the bar is still
            // transparent from the edge-to-edge branch above. Without a color updateBarColor()
            // bails out, leaving the bar transparent with edge-to-edge era button colors, so
            // restore the activity default that startup applied.
            navigationBarColor = defaultNavigationBarColor(context);
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
                getDividerColor(
                        context, navigationBarColor, navigationBarDividerColor, needsDarkButtons),
                supportsDarkButtons,
                needsDarkButtons,
                systemBarColorHelper);
    }

    /** Returns the navigation bar color the activity theme starts out with. */
    @VisibleForTesting
    public static @ColorInt int defaultNavigationBarColor(Context context) {
        @ColorInt
        int themeColor =
                AttrUtils.resolveColor(context.getTheme(), android.R.attr.navigationBarColor);
        return themeColor != 0 ? themeColor : SemanticColorUtils.getDefaultBgColor(context);
    }

    /** Sets the navigation bar and divider colors, then the matching button appearance. */
    private static void updateBarColor(
            Window window,
            @Nullable Integer navigationBarColor,
            @Nullable Integer dividerColor,
            boolean supportsDarkButtons,
            boolean needsDarkButtons,
            @Nullable EdgeToEdgeSystemBarColorHelper systemBarColorHelper) {
        if (navigationBarColor == null) {
            setDividerColor(window, dividerColor, systemBarColorHelper);
            return;
        }

        if (!supportsDarkButtons && needsDarkButtons) {
            // Can't make the buttons dark, darken the background instead with the same algorithm
            // as for the status bar.
            navigationBarColor = ColorUtils.getDarkenedColorForStatusBar(navigationBarColor);
        }

        if (systemBarColorHelper != null) {
            systemBarColorHelper.setNavigationBarColor(navigationBarColor);
        } else {
            window.setNavigationBarColor(navigationBarColor);
        }
        setDividerColor(window, dividerColor, systemBarColorHelper);

        // Applied last: a bar- or divider-color change makes
        // EdgeToEdgeSystemBarColorHelper#updateNavBarColors() re-derive the icon color from the
        // bar color using a luminance threshold rather than the contrast rule used here, which
        // would otherwise overwrite this.
        if (supportsDarkButtons) {
            UiUtils.setNavigationBarIconColor(
                    window.getDecorView().getRootView(), needsDarkButtons);
        }
    }

    private static void setDividerColor(
            Window window,
            @Nullable Integer dividerColor,
            @Nullable EdgeToEdgeSystemBarColorHelper systemBarColorHelper) {
        if (dividerColor == null) return;
        if (systemBarColorHelper != null) {
            systemBarColorHelper.setNavigationBarDividerColor(dividerColor);
        } else {
            window.setNavigationBarDividerColor(dividerColor);
        }
    }

    /**
     * Return the navigation bar divider color.
     *
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
