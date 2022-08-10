// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.DoubleCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.ui.base.DeviceFormFactor;

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

    private static final String SEARCH_CHIP_ADAPTIVE_PARAM =
            "enable_search_term_chip_adaptive_icon";
    public static final BooleanCachedFieldTrialParameter ENABLE_SEARCH_CHIP_ADAPTIVE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, SEARCH_CHIP_ADAPTIVE_PARAM, false);

    private static final String LAUNCH_BUG_FIX_PARAM = "enable_launch_bug_fix";
    public static final BooleanCachedFieldTrialParameter ENABLE_LAUNCH_BUG_FIX =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID, LAUNCH_BUG_FIX_PARAM, false);

    private static final String LAUNCH_POLISH_PARAM = "enable_launch_polish";
    public static final BooleanCachedFieldTrialParameter ENABLE_LAUNCH_POLISH =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID, LAUNCH_POLISH_PARAM, false);

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

    // Field trial parameter for configuring the "Open in new tab" and "Open in new tab in group"
    // item order in the context menu.
    private static final String SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST_PARAM =
            "show_open_in_tab_group_menu_item_first";

    public static final BooleanCachedFieldTrialParameter SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                    SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST_PARAM, false);

    private static final String TAB_GROUP_SHARING_PARAM = "enable_tab_group_sharing";
    public static final BooleanCachedFieldTrialParameter ENABLE_TAB_GROUP_SHARING =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                    TAB_GROUP_SHARING_PARAM, false);

    // Field trial parameter for enabling launch polish for the grid tab switcher for tablets.
    private static final String GRID_TAB_SWITCHER_FOR_TABLETS_POLISH_PARAM = "enable_launch_polish";
    public static final BooleanCachedFieldTrialParameter GRID_TAB_SWITCHER_FOR_TABLETS_POLISH =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS,
                    GRID_TAB_SWITCHER_FOR_TABLETS_POLISH_PARAM, false);

    // Field trial parameter for controlling delay grid tab switcher creation for tablets.
    private static final String DELAY_GTS_CREATION_PARAM = "delay_creation";
    public static final BooleanCachedFieldTrialParameter DELAY_GTS_CREATION =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS,
                    DELAY_GTS_CREATION_PARAM, false);

    // Field trial parameter for defining tab width for tab strip improvements.
    private static final String TAB_STRIP_IMPROVEMENTS_TAB_WIDTH_PARAM = "min_tab_width";
    public static final DoubleCachedFieldTrialParameter TAB_STRIP_TAB_WIDTH =
            new DoubleCachedFieldTrialParameter(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS,
                    TAB_STRIP_IMPROVEMENTS_TAB_WIDTH_PARAM, 190.f);

    private static Boolean sTabManagementModuleSupportedForTesting;
    private static Boolean sGridTabSwitcherPolishEnabledForTesting;

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
     * @param context The activity context.
     */
    public static boolean isGridTabSwitcherEnabled(Context context) {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            return isTabletGridTabSwitcherEnabled(context);
        }

        // Having Tab Groups or Start implies Grid Tab Switcher.
        return isTabManagementModuleSupported() || isTabGroupsAndroidEnabled(context)
                || ReturnToChromeUtil.isStartSurfaceEnabled(context);
    }

    /**
     * @return Whether the tablet Grid Tab Switcher UI is enabled and available for use.
     * @param context The activity context.
     */
    public static boolean isTabletGridTabSwitcherEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS);
    }

    /**
     * Set whether the tablet grid tab switcher polish is enabled for testing.
     */
    public static void setTabletGridTabSwitcherPolishEnabledForTesting(@Nullable Boolean enabled) {
        sGridTabSwitcherPolishEnabledForTesting = enabled;
    }

    /**
     * @return Whether the tablet Grid Tab Switcher Polish is enabled.
     * @param context The activity context.
     */
    public static boolean isTabletGridTabSwitcherPolishEnabled(Context context) {
        if (sGridTabSwitcherPolishEnabledForTesting != null) {
            return sGridTabSwitcherPolishEnabledForTesting;
        }
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && GRID_TAB_SWITCHER_FOR_TABLETS_POLISH.getValue();
    }

    /**
     * @return Whether the tablet Grid Tab Switcher creation should be delayed to on GTS load
     *         instead of on startup.
     */
    public static boolean isTabletGridTabSwitcherDelayCreationEnabled() {
        return DELAY_GTS_CREATION.getValue();
    }

    /**
     * @return Whether the tab group feature is enabled and available for use.
     * @param context The activity context.
     */
    public static boolean isTabGroupsAndroidEnabled(Context context) {
        // Disable tab group for tablet.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            return isTabletTabGroupsEnabled(context);
        }

        return !DeviceClassManager.enableAccessibilityLayout(context)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)
                && isTabManagementModuleSupported();
    }
  
    /**
     * @return Whether tab groups are enabled for tablet.
     * @param context The activity context.
     */
    public static boolean isTabletTabGroupsEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_FOR_TABLETS);
    }

    /**
     * @return Whether the tab group continuation feature is enabled and available for use.
     * @param context The activity context.
     */
    public static boolean isTabGroupsAndroidContinuationEnabled(Context context) {
        return isTabGroupsAndroidEnabled(context)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
    }

    /**
     * @return Whether the conditional tab strip feature is enabled and available for use.
     */
    public static boolean isConditionalTabStripEnabled() {
        // TODO(crbug.com/1222946): Deprecate this feature.
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
                && isTabManagementModuleSupported()
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
                && SysUtils.amountOfPhysicalMemoryKB() / 1024 >= ZOOMING_MIN_MEMORY.getValue();
    }

    /**
     * @return Whether the instant start is supported.
     */
    public static boolean supportInstantStart(boolean isTablet, Context context) {
        return !DeviceClassManager.enableAccessibilityLayout(context)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START) && !isTablet
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Grid/Group launch polish is enabled.
     */
    public static boolean isLaunchPolishEnabled() {
        return ENABLE_LAUNCH_POLISH.getValue();
    }

    private static Float sTabMinWidthForTesting;

    /**
     * Set the min tab width for testing.
     */
    public static void setTabMinWidthForTesting(@Nullable Float minWidth) {
        sTabMinWidthForTesting = minWidth;
    }

    /**
     * @return The min tab width.
     */
    public static float getTabMinWidth() {
        if (sTabMinWidthForTesting != null) {
            return sTabMinWidthForTesting;
        }

        return (float) TAB_STRIP_TAB_WIDTH.getValue();
    }

    /**
     * @return Whether the Grid/Group launch bug fix is enabled.
     */
    public static boolean isLaunchBugFixEnabled() {
        return ENABLE_LAUNCH_BUG_FIX.getValue();
    }

    /**
     * @return Whether the "Open in new tab in group" context menu item should show before the
     * "Open in new tab" item.
     */
    public static boolean showContextMenuOpenNewTabInGroupItemFirst() {
        assert !ENABLE_TAB_GROUP_AUTO_CREATION.getValue();

        return SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST.getValue();
    }
}
