// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST;

import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.DoubleCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Random;

/**
 * A class to handle the state of flags for tab_management.
 */
public class TabUiFeatureUtilities {
    private static final String TAG = "TabFeatureUtilities";

    // Field trial parameters:
    private static final String SKIP_SLOW_ZOOMING_PARAM = "skip-slow-zooming";
    public static final BooleanCachedFieldTrialParameter SKIP_SLOW_ZOOMING =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, SKIP_SLOW_ZOOMING_PARAM, true);

    private static final String TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE_PARAM =
            "tab_grid_layout_android_new_tab_tile";
    public static final StringCachedFieldTrialParameter TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE =
            new StringCachedFieldTrialParameter(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                    TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE_PARAM, "");

    public static final String THUMBNAIL_ASPECT_RATIO_PARAM = "thumbnail_aspect_ratio";
    public static final DoubleCachedFieldTrialParameter THUMBNAIL_ASPECT_RATIO =
            new DoubleCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, THUMBNAIL_ASPECT_RATIO_PARAM, 0.85);

    private static final String SEARCH_CHIP_PARAM = "enable_search_term_chip";
    public static final BooleanCachedFieldTrialParameter ENABLE_SEARCH_CHIP =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, SEARCH_CHIP_PARAM, false);

    private static final String PRICE_TRACKING_PARAM = "enable_price_tracking";
    public static final BooleanCachedFieldTrialParameter ENABLE_PRICE_TRACKING =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, PRICE_TRACKING_PARAM, false);

    private static final String PRICE_NOTIFICATION_PARAM = "enable_price_notification";
    public static final BooleanCachedFieldTrialParameter ENABLE_PRICE_NOTIFICATION =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, PRICE_NOTIFICATION_PARAM, false);

    private static final String SEARCH_CHIP_ADAPTIVE_PARAM =
            "enable_search_term_chip_adaptive_icon";
    public static final BooleanCachedFieldTrialParameter ENABLE_SEARCH_CHIP_ADAPTIVE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, SEARCH_CHIP_ADAPTIVE_PARAM, false);

    private static final String LAUNCH_BUG_FIX_PARAM = "enable_launch_bug_fix";
    public static final BooleanCachedFieldTrialParameter ENABLE_LAUNCH_BUG_FIX =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, LAUNCH_BUG_FIX_PARAM, false);

    private static final String LAUNCH_POLISH_PARAM = "enable_launch_polish";
    public static final BooleanCachedFieldTrialParameter ENABLE_LAUNCH_POLISH =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, LAUNCH_POLISH_PARAM, false);

    // Field trial parameter for the minimum Android SDK version to enable zooming animation.
    private static final String MIN_SDK_PARAM = "zooming-min-sdk-version";
    public static final IntCachedFieldTrialParameter ZOOMING_MIN_SDK =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_SDK_PARAM, Build.VERSION_CODES.M);
    // Field trial parameter for the minimum physical memory size to enable zooming animation.
    private static final String MIN_MEMORY_MB_PARAM = "zooming-min-memory-mb";
    public static final IntCachedFieldTrialParameter ZOOMING_MIN_MEMORY =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_MEMORY_MB_PARAM, 2048);

    // Field trial parameter for removing tab group auto creation from target-blank links and adding
    // both "Open in new tab" and "Open in new tab in group" as context menu items.
    private static final String TAB_GROUP_AUTO_CREATION_PARAM = "enable_tab_group_auto_creation";

    public static final BooleanCachedFieldTrialParameter ENABLE_TAB_GROUP_AUTO_CREATION =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, TAB_GROUP_AUTO_CREATION_PARAM, true);

    private static Boolean sTabManagementModuleSupportedForTesting;

    /**
     * Set whether the tab management module is supported for testing.
     */
    public static void setTabManagementModuleSupportedForTesting(@Nullable Boolean enabled) {
        sTabManagementModuleSupportedForTesting = enabled;
    }

    /**
     * @return Whether the tab management module is supported.
     */
    private static boolean isTabManagementModuleSupported() {
        if (sTabManagementModuleSupportedForTesting != null) {
            return sTabManagementModuleSupportedForTesting;
        }

        return TabManagementModuleProvider.isTabManagementModuleSupported();
    }

    /**
     * @return Whether the Grid Tab Switcher UI is enabled and available for use.
     */
    public static boolean isGridTabSwitcherEnabled() {
        // Disable grid tab switcher for tablet.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                    ContextUtils.getApplicationContext())) {
            return false;
        }

        // Having Tab Groups or Start implies Grid Tab Switcher.
        return isTabManagementModuleSupported() || isTabGroupsAndroidEnabled()
                || StartSurfaceConfiguration.isStartSurfaceEnabled();
    }

    /**
     * @return Whether the tab group feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidEnabled() {
        // Disable tab group for tablet.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                    ContextUtils.getApplicationContext())) {
            return false;
        }

        return !DeviceClassManager.enableAccessibilityLayout()
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)
                && isTabManagementModuleSupported();
    }

    /**
     * @return Whether the tab group continuation feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidContinuationEnabled() {
        return isTabGroupsAndroidEnabled()
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
    }

    /**
     * @return Whether the conditional tab strip feature is enabled and available for use.
     */
    public static boolean isConditionalTabStripEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
                && !isGridTabSwitcherEnabled() && isTabManagementModuleSupported()
                && !ConditionalTabStripUtils.getOptOutIndicator();
    }

    /**
     * @return Whether the thumbnail_aspect_ratio field trail is set.
     */
    public static boolean isTabThumbnailAspectRatioNotOne() {
        return Double.compare(1.0, THUMBNAIL_ASPECT_RATIO.getValue()) != 0;
    }

    public static boolean isTabGridLayoutAndroidNewTabTileEnabled() {
        return TextUtils.equals(TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE.getValue(), "NewTabTile");
    }

    /**
     * @return Whether the Tab-to-Grid (and Grid-to-Tab) transition animation is enabled.
     */
    public static boolean isTabToGtsAnimationEnabled() {
        Log.d(TAG, "GTS.MinSdkVersion = " + ZOOMING_MIN_SDK.getValue());
        Log.d(TAG, "GTS.MinMemoryMB = " + ZOOMING_MIN_MEMORY.getValue());
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                && Build.VERSION.SDK_INT >= ZOOMING_MIN_SDK.getValue()
                && SysUtils.amountOfPhysicalMemoryKB() / 1024 >= ZOOMING_MIN_MEMORY.getValue()
                && !StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled();
    }

    /**
     * @return Whether the instant start is supported.
     */
    public static boolean supportInstantStart(boolean isTablet) {
        return !DeviceClassManager.enableAccessibilityLayout()
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START) && !isTablet
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Grid/Group launch polish is enabled.
     */
    public static boolean isLaunchPolishEnabled() {
        return ENABLE_LAUNCH_POLISH.getValue();
    }

    /**
     * @return Whether the Grid/Group launch bug fix is enabled.
     */
    public static boolean isLaunchBugFixEnabled() {
        return ENABLE_LAUNCH_BUG_FIX.getValue();
    }

    /**
     * @return Whether the price tracking feature is enabled and available for use.
     */
    public static boolean isPriceTrackingEnabled() {
        // TODO(crbug.com/1152925): Now PriceTracking feature is broken if StartSurface is enabled,
        // we need to remove !StartSurfaceConfiguration.isStartSurfaceEnabled() when the bug is
        // fixed.
        return (ENABLE_PRICE_TRACKING.getValue() || ENABLE_PRICE_NOTIFICATION.getValue())
                && !StartSurfaceConfiguration.isStartSurfaceEnabled();
    }

    /**
     * @return Whether the "Open in new tab in group" context menu item should show before the
     * "Open in new tab" item.
     */
    public static boolean showContextMenuOpenNewTabInGroupItemFirst() {
        assert !ENABLE_TAB_GROUP_AUTO_CREATION.getValue();

        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();

        if (!sharedPreferencesManager.contains(CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST)) {
            Random random = new Random();
            sharedPreferencesManager.writeBoolean(
                    CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST, random.nextBoolean());
        }

        return sharedPreferencesManager.readBoolean(
                CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST, false);
    }
}
