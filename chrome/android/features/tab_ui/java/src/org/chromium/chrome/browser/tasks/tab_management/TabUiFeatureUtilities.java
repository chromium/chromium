// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.device.DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT;
import static org.chromium.chrome.browser.device.DeviceClassManager.GTS_LOW_END_SUPPORT;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.DoubleCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
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
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                    TAB_GROUP_AUTO_CREATION_PARAM, false);

    // Field trial parameter for configuring the "Open in new tab" and "Open in new tab in group"
    // item order in the context menu.
    private static final String SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST_PARAM =
            "show_open_in_tab_group_menu_item_first";

    public static final BooleanCachedFieldTrialParameter SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                    SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST_PARAM, true);

    // Field trial parameter for disabling new tab button anchor for tab strip redesign.
    private static final String TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR_PARAM = "disable_ntb_anchor";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR_PARAM, false);

    // Field trial parameter for disabling button style for tab strip redesign. This includes
    // disabling NTB anchor and button bg style.
    private static final String TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE_PARAM = "disable_btn_style";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE_PARAM, false);

    private static boolean sTabSelectionEditorLongPressEntryEnabled;

    public static final MutableFlagWithSafeDefault sThumbnailPlaceholder =
            new MutableFlagWithSafeDefault(ChromeFeatureList.THUMBNAIL_PLACEHOLDER, false);

    /**
     * Set whether the longpress entry for TabSelectionEditor is enabled. Currently only in tests.
     */
    public static void setTabSelectionEditorLongPressEntryEnabledForTesting(boolean enabled) {
        sTabSelectionEditorLongPressEntryEnabled = enabled;
    }

    /**
     * @return Whether New tab button anchor for tab strip redesign is disabled.
     */
    public static boolean isTabStripNtbAnchorDisabled() {
        return TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR.getValue();
    }

    /**
     * @return Whether button style for tab strip redesign is disabled.
     */
    public static boolean isTabStripButtonStyleDisabled() {
        return TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE.getValue();
    }

    /**
     * Whether the longpress entry for TabSelectionEditor is enabled. Currently only in tests.
     */
    public static boolean isTabSelectionEditorLongPressEntryEnabled() {
        return sTabSelectionEditorLongPressEntryEnabled;
    }

    /**
     * @return Whether we should delay the placeholder tab strip removal on startup.
     * @param context The activity context.
     */
    public static boolean isDelayTempStripRemovalEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && ChromeFeatureList.sDelayTempStripRemoval.isEnabled();
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
        return isTabGroupsAndroidEnabled(context)
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
     * @return Whether tab groups are enabled for tablet.
     * @param context The activity context.
     */
    public static boolean isTabletTabGroupsEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
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
                && ChromeFeatureList.sTabGroupsAndroid.isEnabled();
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

    public static Float sTabMinWidthForTesting;

    /**
     * @return Whether the "Open in new tab in group" context menu item should show before the
     * "Open in new tab" item.
     */
    public static boolean showContextMenuOpenNewTabInGroupItemFirst() {
        assert !ENABLE_TAB_GROUP_AUTO_CREATION.getValue();

        return SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST.getValue();
    }
}
