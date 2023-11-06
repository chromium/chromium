// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
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

    // Field trial parameter for the minimum physical memory size to enable zooming animation.
    private static final String MIN_MEMORY_MB_PARAM = "zooming-min-memory-mb";
    public static final IntCachedFieldTrialParameter ZOOMING_MIN_MEMORY =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_MEMORY_MB_PARAM, 2048);

    // Field trial parameter for disabling new tab button anchor for tab strip redesign.
    private static final String TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR_PARAM = "disable_ntb_anchor";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR_PARAM,
                    true);

    // Field trial parameter for disabling button style for tab strip redesign. This includes
    // disabling NTB anchor and button bg style.
    private static final String TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE_PARAM = "disable_btn_style";
    public static final BooleanCachedFieldTrialParameter TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_STRIP_REDESIGN,
                    TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE_PARAM,
                    true);

    // Field trial parameter for animation start timeout for new Android based shrink and expand
    // animations in TabSwitcherLayout.
    private static final String ANIMATION_START_TIMEOUT_MS_PARAM = "animation_start_timeout_ms";
    public static final IntCachedFieldTrialParameter ANIMATION_START_TIMEOUT_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS,
                    ANIMATION_START_TIMEOUT_MS_PARAM,
                    300);

    public static final MutableFlagWithSafeDefault sThumbnailPlaceholder =
            new MutableFlagWithSafeDefault(ChromeFeatureList.THUMBNAIL_PLACEHOLDER, false);

    public static final MutableFlagWithSafeDefault sAdvancedPeripheralsSupportTabStrip =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP, false);

    // Cached and fixed values.
    private static boolean sTabSelectionEditorLongPressEntryEnabled;
    private static Boolean sIsTabToGtsAnimationEnabled;

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
     * @return Whether the Grid Tab Switcher UI should use list mode.
     * @param context The activity context.
     */
    public static boolean shouldUseListMode(Context context) {
        // Low-end forces list mode.
        return SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Tab-to-Grid (and Grid-to-Tab) transition animation is enabled.
     */
    public static boolean isTabToGtsAnimationEnabled(Context context) {
        if (sIsTabToGtsAnimationEnabled == null || BuildConfig.IS_FOR_TEST) {
            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
                sIsTabToGtsAnimationEnabled = false;
            } else {
                Log.d(TAG, "GTS.MinMemoryMB = " + ZOOMING_MIN_MEMORY.getValue());
                sIsTabToGtsAnimationEnabled = ChromeFeatureList.sTabToGTSAnimation.isEnabled()
                        && SysUtils.amountOfPhysicalMemoryKB() / 1024
                                >= ZOOMING_MIN_MEMORY.getValue()
                        && !shouldUseListMode(context);
            }
        }
        return sIsTabToGtsAnimationEnabled;
    }

    /**
     * @return Whether the instant start is supported.
     */
    public static boolean supportInstantStart(boolean isTablet, Context context) {
        return ChromeFeatureList.sInstantStart.isEnabled() && !isTablet
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return whether tab drag is enabled (either via drag as window or drag as tab).
     * TODO(crbug.com/1485628) - merge both flags and use device property instead to differentiate.
     */
    public static boolean isTabDragEnabled() {
        // Both flags should not be enabled together.
        assert !(ChromeFeatureList.sTabLinkDragDropAndroid.isEnabled()
                && isTabDragAsWindowEnabled());
        return isTabDragAsWindowEnabled() || ChromeFeatureList.sTabLinkDragDropAndroid.isEnabled();
    }

    /**
     * @return whether tab drag as window is enabled.
     */
    public static boolean isTabDragAsWindowEnabled() {
        return ChromeFeatureList.sTabDragDropAsWindowAndroid.isEnabled();
    }
}
