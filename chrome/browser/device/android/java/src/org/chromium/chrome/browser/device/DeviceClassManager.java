// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * This class is used to turn on and off certain features for different types of
 * devices.
 */
public class DeviceClassManager {
    // Params for controlling Grid Tab Switcher (GTS) rollout for accessibility and low-end device
    // users.
    // TODO(crbug/1466158): Remove and keep in true state for both.
    private static final String GTS_ACCESSIBILITY_SUPPORT_PARAM = "gts-accessibility-support";
    public static final BooleanCachedFieldTrialParameter GTS_ACCESSIBILITY_SUPPORT =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                    GTS_ACCESSIBILITY_SUPPORT_PARAM, true);

    private static final String GTS_LOW_END_SUPPORT_PARAM = "gts-low-end-support";
    public static final BooleanCachedFieldTrialParameter GTS_LOW_END_SUPPORT =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                    GTS_LOW_END_SUPPORT_PARAM, true);

    private static DeviceClassManager sInstance;

    // Set of features that can be enabled/disabled
    private boolean mEnableLayerDecorationCache;
    private boolean mEnableAccessibilityLayout;
    private boolean mEnableAnimations;
    private boolean mEnablePrerendering;
    private boolean mEnableToolbarSwipe;

    private final boolean mEnableFullscreen;

    private static DeviceClassManager getInstance() {
        if (sInstance == null) {
            sInstance = new DeviceClassManager();
        }
        return sInstance;
    }

    /**
     * The {@link DeviceClassManager} constructor should be self contained and
     * rely on system information and command line flags.
     */
    private DeviceClassManager() {
        // Device based configurations.
        if (SysUtils.isLowEndDevice()) {
            mEnableLayerDecorationCache = true;
            mEnableAccessibilityLayout = true;
            mEnableAnimations = false;
            mEnablePrerendering = false;
            mEnableToolbarSwipe = false;
        } else {
            mEnableLayerDecorationCache = true;
            mEnableAccessibilityLayout = false;
            mEnableAnimations = true;
            mEnablePrerendering = true;
            mEnableToolbarSwipe = true;
        }

        if (DeviceFormFactor.isTablet()) {
            mEnableAccessibilityLayout = false;
        }

        // Flag based configurations.
        CommandLine commandLine = CommandLine.getInstance();
        mEnableAccessibilityLayout |=
                commandLine.hasSwitch(ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER);
        mEnableFullscreen = !commandLine.hasSwitch(ChromeSwitches.DISABLE_FULLSCREEN);

        // Related features.
        if (mEnableAccessibilityLayout) {
            mEnableAnimations = false;
        }
    }

    /**
     * @return Whether or not we can use the layer decoration cache.
     */
    public static boolean enableLayerDecorationCache() {
        return getInstance().mEnableLayerDecorationCache;
    }

    /**
     * @return Whether or not should use the accessibility tab switcher.
     * @param context The activity context.
     */
    public static boolean enableAccessibilityLayout(Context context) {
        final boolean defaultBehavior = enableAccessibilityLayoutInternal();

        // TODO(crbug.com/1007598): Support TabGrid and TabGroup in Accessibility mode.
        if (ChromeFeatureList.sTabGroupsContinuationAndroid.isEnabled()
                && ChromeFeatureList.sTabGroupsAndroid.isEnabled()) {
            final boolean isLowEndDevice = SysUtils.isLowEndDevice();
            final boolean isAccessibilityEnabled =
                    ChromeAccessibilityUtil.get().isAccessibilityEnabled();
            if (isLowEndDevice && isAccessibilityEnabled) {
                if (!GTS_LOW_END_SUPPORT.getValue() || !GTS_ACCESSIBILITY_SUPPORT.getValue()) {
                    return defaultBehavior;
                }
            } else if (isLowEndDevice && !GTS_LOW_END_SUPPORT.getValue()) {
                return defaultBehavior;
            } else if (isAccessibilityEnabled && !GTS_ACCESSIBILITY_SUPPORT.getValue()) {
                return defaultBehavior;
            }
            return false;
        }

        return defaultBehavior;
    }

    private static boolean enableAccessibilityLayoutInternal() {
        if (getInstance().mEnableAccessibilityLayout) return true;
        if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) return false;
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.ACCESSIBILITY_TAB_SWITCHER, true);
    }

    /**
     * @return Whether or not full screen is enabled.
     */
    public static boolean enableFullscreen() {
        return getInstance().mEnableFullscreen;
    }

    /**
     * @return Whether or not we are showing animations.
     */
    public static boolean enableAnimations() {
        if (!getInstance().mEnableAnimations) return false;
        if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) return true;
        return !SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.ACCESSIBILITY_TAB_SWITCHER, true);
    }

    /**
     * @return Whether or not prerendering is enabled.
     */
    public static boolean enablePrerendering() {
        return getInstance().mEnablePrerendering;
    }

    /**
     * @return Whether or not we can use the toolbar swipe.
     */
    public static boolean enableToolbarSwipe() {
        return getInstance().mEnableToolbarSwipe;
    }

    private static boolean isPhone(Context context) {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Reset the instance for testing.
     */
    public static void resetForTesting() {
        sInstance = null;
    }
}
