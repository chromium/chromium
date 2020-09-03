// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The variation manager helps figure out the current variation and the visibility of buttons on
 * bottom toolbar. Every operation related to the variation, e.g. getting variation value, should be
 * through {@link BottomToolbarVariationManager} rather than calling {@link CachedFeatureFlags}.
 */
public class BottomToolbarVariationManager {
    @StringDef({Variations.NONE, Variations.HOME_SEARCH_TAB_SWITCHER, Variations.HOME_SEARCH_SHARE,
            Variations.NEW_TAB_SEARCH_SHARE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Variations {
        String NONE = "";
        String HOME_SEARCH_TAB_SWITCHER = "HomeSearchTabSwitcher";
        String HOME_SEARCH_SHARE = "HomeSearchShare";
        String NEW_TAB_SEARCH_SHARE = "NewTabSearchShare";
    }

    private static String sVariation;

    /**
     * @return The currently enabled bottom toolbar variation.
     *         Should be called after {@link BottomToolbarConfiguration#isBottomToolbarEnabled()}.
     */
    private static @Variations String getVariation() {
        if (sVariation != null) return sVariation;
        if (!BottomToolbarConfiguration.isBottomToolbarEnabled()) {
            return Variations.HOME_SEARCH_TAB_SWITCHER;
        }
        sVariation = Variations.NONE;
        return sVariation;
    }

    /**
     * @return Whether or not share button should be visible on the top toolbar in portrait mode
     *         in the current variation.
     */
    public static boolean isShareButtonOnBottom() {
        return BottomToolbarConfiguration.isBottomToolbarEnabled()
                && !getVariation().equals(Variations.HOME_SEARCH_TAB_SWITCHER);
    }

    /**
     * @return Whether or not new tab button should be visible on the bottom toolbar
     *         in portrait mode in the current variation.
     */
    public static boolean isNewTabButtonOnBottom() {
        return BottomToolbarConfiguration.isBottomToolbarEnabled()
                && getVariation().equals(Variations.NEW_TAB_SEARCH_SHARE);
    }

    /**
     * @return Whether or not menu button should be visible on the top toolbar
     *         in portrait mode in the current variation.
     */
    public static boolean isMenuButtonOnBottom() {
        // If we don't have variations that put menu on bottom in the future,
        // then this method can be removed.
        return false;
    }

    /**
     * @return Whether or not bottom toolbar should be visible in overview mode of portrait mode
     *         in the current variation.
     */
    public static boolean shouldBottomToolbarBeVisibleInOverviewMode() {
        return (getVariation().equals(Variations.NEW_TAB_SEARCH_SHARE)
                       && !StartSurfaceConfiguration.isStartSurfaceEnabled())
                || ((!TabUiFeatureUtilities.isGridTabSwitcherEnabled()
                            || !IncognitoUtils.isIncognitoModeEnabled())
                        && getVariation().equals(Variations.HOME_SEARCH_TAB_SWITCHER));
    }

    /**
     * @return Whether or not home button should be visible in top toolbar of portrait mode
     *         in current variation.
     */
    public static boolean isHomeButtonOnBottom() {
        return BottomToolbarConfiguration.isBottomToolbarEnabled()
                && !getVariation().equals(Variations.NEW_TAB_SEARCH_SHARE);
    }

    /**
     * @return Whether or not tab switcher button should be visible in bottom toolbar
     *         of portrait mode in current variation.
     */
    public static boolean isTabSwitcherOnBottom() {
        return BottomToolbarConfiguration.isBottomToolbarEnabled()
                && getVariation().equals(Variations.HOME_SEARCH_TAB_SWITCHER);
    }

    /**
     * @return Name of the variation parameter of bottom toolbar.
     */
    public static String getVariationParamName() {
        return "chrome_duet_variation";
    }

    @VisibleForTesting
    public static void setVariation(String variation) {
        sVariation = variation;
    }
}
