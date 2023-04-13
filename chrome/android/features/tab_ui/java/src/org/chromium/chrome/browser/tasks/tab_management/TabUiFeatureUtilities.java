// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.device.DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT;
import static org.chromium.chrome.browser.device.DeviceClassManager.GTS_LOW_END_SUPPORT;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.DoubleCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
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

    private static final String GTS_ACCESSIBILITY_LIST_MODE_PARAM = "gts-accessibility-list-mode";
    public static final BooleanCachedFieldTrialParameter GTS_ACCESSIBILITY_LIST_MODE =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                    GTS_ACCESSIBILITY_LIST_MODE_PARAM, false);

    public static final String THUMBNAIL_ASPECT_RATIO_PARAM = "thumbnail_aspect_ratio";
    public static final DoubleCachedFieldTrialParameter THUMBNAIL_ASPECT_RATIO =
            new DoubleCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, THUMBNAIL_ASPECT_RATIO_PARAM, 0.85);

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

    // Field trial parameter for defining tab width for tab strip improvements.
    private static final String TAB_STRIP_IMPROVEMENTS_TAB_WIDTH_PARAM = "min_tab_width";
    public static final DoubleCachedFieldTrialParameter TAB_STRIP_TAB_WIDTH =
            new DoubleCachedFieldTrialParameter(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS,
                    TAB_STRIP_IMPROVEMENTS_TAB_WIDTH_PARAM, 108.f);

    // Field trial parameter for controlling share tabs in TabSelectionEditorV2.
    private static final String TAB_SELECTION_EDITOR_V2_SHARE_PARAM = "enable_share";
    public static final BooleanCachedFieldTrialParameter ENABLE_TAB_SELECTION_EDITOR_V2_SHARE =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_SELECTION_EDITOR_V2,
                    TAB_SELECTION_EDITOR_V2_SHARE_PARAM, true);

    // Field trial parameter for controlling longpress entry into TabSelectionEditorV2 from
    // TabGridDialog and TabSwitcher.
    private static final String TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY_PARAM =
            "enable_longpress_entrypoint";
    public static final BooleanCachedFieldTrialParameter
            ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY =
                    new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_SELECTION_EDITOR_V2,
                            TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY_PARAM, false);

    // Field trial parameter for controlling bookmark tabs in TabSelectionEditorV2.
    private static final String TAB_SELECTION_EDITOR_V2_BOOKMARKS_PARAM = "enable_bookmarks";
    public static final BooleanCachedFieldTrialParameter ENABLE_TAB_SELECTION_EDITOR_V2_BOOKMARKS =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_SELECTION_EDITOR_V2,
                    TAB_SELECTION_EDITOR_V2_BOOKMARKS_PARAM, true);

    // Field trial parameter for disabling new tab button anchor for tab strip redesign.
    private static final String TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR_PARAM = "disable_ntb_anchor";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR_PARAM, false);

    private static Boolean sTabManagementModuleSupportedForTesting;

    /**
     * Set whether the tab management module is supported for testing.
     */
    public static void setTabManagementModuleSupportedForTesting(@Nullable Boolean enabled) {
        sTabManagementModuleSupportedForTesting = enabled;
    }

    /**
     * @return Whether New tab button anchor for tab strip redesign is disabled.
     */
    public static boolean isTabStripNtbAnchorDisabled() {
        return TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR.getValue();
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
            return true;
        }

        // Having Tab Groups or Start implies Grid Tab Switcher.
        return isTabManagementModuleSupported() || isTabGroupsAndroidEnabled(context)
                || ReturnToChromeUtil.isStartSurfaceEnabled(context);
    }

    /**
     * @return Whether the Grid Tab Switcher UI should use list mode.
     * @param context The activity context.
     */
    public static boolean shouldUseListMode(Context context) {
        if (!isTabGroupsAndroidContinuationEnabled(context)) {
            return false;
        }
        // Low-end forces list mode regardless of accessibility behavior.
        if (GTS_LOW_END_SUPPORT.getValue() && SysUtils.isLowEndDevice()) {
            return true;
        }
        if (GTS_ACCESSIBILITY_SUPPORT.getValue()
                && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            return GTS_ACCESSIBILITY_LIST_MODE.getValue();
        }
        return false;
    }

    /**
     * @return Whether the tab strip improvements are enabled.
     * @param context The activity context.
     */
    public static boolean isTabStripImprovementsEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && ChromeFeatureList.sTabStripImprovements.isEnabled();
    }

    /**
     * @return Whether tab groups are enabled for tablet.
     * @param context The activity context.
     */
    public static boolean isTabletTabGroupsEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && ChromeFeatureList.sTabStripImprovements.isEnabled()
                && ChromeFeatureList.sTabGroupsForTablets.isEnabled()
                && !DeviceClassManager.enableAccessibilityLayout(context);
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
                && ChromeFeatureList.sTabGroupsAndroid.isEnabled()
                && isTabManagementModuleSupported();
    }

    /**
     * @return Whether the tab group continuation feature is enabled and available for use.
     * @param context The activity context.
     */
    public static boolean isTabGroupsAndroidContinuationEnabled(Context context) {
        return isTabGroupsAndroidEnabled(context)
                && ChromeFeatureList.sTabGroupsContinuationAndroid.isEnabled();
    }

    /**
     * @return Whether the tab selection editor v2 is enabled and available for use.
     * @param context The activity context.
     */
    public static boolean isTabSelectionEditorV2Enabled(Context context) {
        return isTabGroupsAndroidEnabled(context)
                && ChromeFeatureList.sTabSelectionEditorV2.isEnabled();
    }

    /**
     * @return Whether the thumbnail_aspect_ratio field trail is set.
     */
    public static boolean isTabThumbnailAspectRatioNotOne() {
        return Double.compare(1.0, THUMBNAIL_ASPECT_RATIO.getValue()) != 0;
    }

    /**
     * @return Whether the Tab-to-Grid (and Grid-to-Tab) transition animation is enabled.
     */
    public static boolean isTabToGtsAnimationEnabled(Context context) {
        Log.d(TAG, "GTS.MinMemoryMB = " + ZOOMING_MIN_MEMORY.getValue());
        return ChromeFeatureList.sTabToGTSAnimation.isEnabled()
                && SysUtils.amountOfPhysicalMemoryKB() / 1024 >= ZOOMING_MIN_MEMORY.getValue()
                && !shouldUseListMode(context);
    }

    /**
     * @return Whether the instant start is supported.
     */
    public static boolean supportInstantStart(boolean isTablet, Context context) {
        return !DeviceClassManager.enableAccessibilityLayout(context)
                && ChromeFeatureList.sInstantStart.isEnabled() && !isTablet
                && !SysUtils.isLowEndDevice();
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
     * @return Whether the "Open in new tab in group" context menu item should show before the
     * "Open in new tab" item.
     */
    public static boolean showContextMenuOpenNewTabInGroupItemFirst() {
        assert !ENABLE_TAB_GROUP_AUTO_CREATION.getValue();

        return SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST.getValue();
    }
}
