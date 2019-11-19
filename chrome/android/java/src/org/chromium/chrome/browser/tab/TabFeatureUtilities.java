// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.os.Build;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Contains logic that decides whether to enable features related to tabs.
 */
public class TabFeatureUtilities {
    private static final String TAG = "TabFeatureUtilities";

    private static Boolean sIsTabToGtsAnimationEnabled;

    /**
     * Toggles whether the Tab-to-GTS animation is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setIsTabToGtsAnimationEnabledForTesting(@Nullable Boolean enabled) {
        sIsTabToGtsAnimationEnabled = enabled;
    }

    /**
     * @return Whether the Tab-to-Grid (and Grid-to-Tab) transition animation is enabled.
     */
    public static boolean isTabToGtsAnimationEnabled() {
        if (sIsTabToGtsAnimationEnabled != null) {
            Log.d(TAG, "IsTabToGtsAnimationEnabled forced to " + sIsTabToGtsAnimationEnabled);
            return sIsTabToGtsAnimationEnabled;
        }
        Log.d(TAG, "GTS.MinSdkVersion = " + GridTabSwitcherUtil.getMinSdkVersion());
        Log.d(TAG, "GTS.MinMemoryMB = " + GridTabSwitcherUtil.getMinMemoryMB());
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                && Build.VERSION.SDK_INT >= GridTabSwitcherUtil.getMinSdkVersion()
                && SysUtils.amountOfPhysicalMemoryKB() / 1024
                >= GridTabSwitcherUtil.getMinMemoryMB();
    }

    private static class GridTabSwitcherUtil {
        // Field trial parameter for the minimum Android SDK version to enable zooming animation.
        private static final String MIN_SDK_PARAM = "zooming-min-sdk-version";
        private static final int DEFAULT_MIN_SDK = Build.VERSION_CODES.O;

        // Field trial parameter for the minimum physical memory size to enable zooming animation.
        private static final String MIN_MEMORY_MB_PARAM = "zooming-min-memory-mb";
        private static final int DEFAULT_MIN_MEMORY_MB = 2048;

        private static int getMinSdkVersion() {
            String sdkVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_SDK_PARAM);
            try {
                return Integer.valueOf(sdkVersion);
            } catch (NumberFormatException e) {
                return DEFAULT_MIN_SDK;
            }
        }

        private static int getMinMemoryMB() {
            String sdkVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_MEMORY_MB_PARAM);
            try {
                return Integer.valueOf(sdkVersion);
            } catch (NumberFormatException e) {
                return DEFAULT_MIN_MEMORY_MB;
            }
        }
    }
}
