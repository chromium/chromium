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
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** A class to handle the state of flags for tab_management. */
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

    // Field trial parameter for animation start timeout for new Android based shrink and expand
    // animations in TabSwitcherLayout.
    private static final String ANIMATION_START_TIMEOUT_MS_PARAM = "animation_start_timeout_ms";
    public static final IntCachedFieldTrialParameter ANIMATION_START_TIMEOUT_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.GRID_TAB_SWITCHER_ANDROID_ANIMATIONS,
                    ANIMATION_START_TIMEOUT_MS_PARAM,
                    300);

    // Cached and fixed values.
    private static boolean sTabListEditorLongPressEntryEnabled;
    private static Boolean sIsTabToGtsAnimationEnabled;

    /** Set whether the longpress entry for TabListEditor is enabled. Currently only in tests. */
    public static void setTabListEditorLongPressEntryEnabledForTesting(boolean enabled) {
        var oldValue = sTabListEditorLongPressEntryEnabled;
        sTabListEditorLongPressEntryEnabled = enabled;
        ResettersForTesting.register(() -> sTabListEditorLongPressEntryEnabled = oldValue);
    }

    /** Whether the longpress entry for TabListEditor is enabled. Currently only in tests. */
    public static boolean isTabListEditorLongPressEntryEnabled() {
        return sTabListEditorLongPressEntryEnabled;
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
                sIsTabToGtsAnimationEnabled =
                        ChromeFeatureList.sTabToGTSAnimation.isEnabled()
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
        return ChromeFeatureList.sInstantStart.isEnabled()
                && !isTablet
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return whether tab drag is enabled (either via drag as window or drag as tab).
     * TODO(crbug.com/1485628) - merge both flags and use device property instead to differentiate.
     */
    public static boolean isTabDragEnabled() {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return false;
        }
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
