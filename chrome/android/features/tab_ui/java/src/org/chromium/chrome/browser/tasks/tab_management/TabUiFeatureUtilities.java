// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.device.DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT;
import static org.chromium.chrome.browser.device.DeviceClassManager.GTS_LOW_END_SUPPORT;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

    // TODO(crbug/1466158): Remove and keep in false state.
    private static final String GTS_ACCESSIBILITY_LIST_MODE_PARAM = "gts-accessibility-list-mode";
    public static final BooleanCachedFieldTrialParameter GTS_ACCESSIBILITY_LIST_MODE =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                    GTS_ACCESSIBILITY_LIST_MODE_PARAM, false);

    // Field trial parameter for the minimum physical memory size to enable zooming animation.
    private static final String MIN_MEMORY_MB_PARAM = "zooming-min-memory-mb";
    public static final IntCachedFieldTrialParameter ZOOMING_MIN_MEMORY =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_MEMORY_MB_PARAM, 2048);

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
        var oldValue = sTabSelectionEditorLongPressEntryEnabled;
        sTabSelectionEditorLongPressEntryEnabled = enabled;
        ResettersForTesting.register(() -> sTabSelectionEditorLongPressEntryEnabled = oldValue);
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
}
