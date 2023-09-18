// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Flag configuration for Start Surface. Source of truth for whether it should be enabled and
 * which variation should be used.
 */
public class StartSurfaceConfiguration {
    private static final String TAG = "StartSurfaceConfig";
    public static final BooleanCachedFieldTrialParameter
            START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB =
                    new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                            "hide_switch_when_no_incognito_tabs", true);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_LAST_ACTIVE_TAB_ONLY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_last_active_tab_only", true);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_NTP_INSTEAD_OF_START =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_ntp_instead_of_start", true);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_START_AS_HOMEPAGE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_start_as_homepage", false);

    private static final String SHOW_TABS_IN_MRU_ORDER_PARAM = "show_tabs_in_mru_order";
    public static final BooleanCachedFieldTrialParameter SHOW_TABS_IN_MRU_ORDER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SHOW_TABS_IN_MRU_ORDER_PARAM, false);

    private static final String SIGNIN_PROMO_NTP_COUNT_LIMIT_PARAM = "signin_promo_NTP_count_limit";
    public static final IntCachedFieldTrialParameter SIGNIN_PROMO_NTP_COUNT_LIMIT =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SIGNIN_PROMO_NTP_COUNT_LIMIT_PARAM, 5);

    private static final String SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS_PARAM =
            "signin_promo_NTP_since_first_time_shown_limit_hours";
    public static final IntCachedFieldTrialParameter
            SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS =
                    new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                            SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS_PARAM, 336);

    private static final String SIGNIN_PROMO_NTP_RESET_AFTER_HOURS_PARAM =
            "signin_promo_NTP_reset_after_hours";
    public static final IntCachedFieldTrialParameter SIGNIN_PROMO_NTP_RESET_AFTER_HOURS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    SIGNIN_PROMO_NTP_RESET_AFTER_HOURS_PARAM, 672);

    private static final String IS_DOODLE_SUPPORTED_PARAM = "is_doodle_supported";
    public static final BooleanCachedFieldTrialParameter IS_DOODLE_SUPPORTED =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, IS_DOODLE_SUPPORTED_PARAM, false);

    // Start return time experiment:
    @VisibleForTesting
    public static final String START_SURFACE_RETURN_TIME_SECONDS_PARAM =
            "start_surface_return_time_seconds";
    public static final IntCachedFieldTrialParameter START_SURFACE_RETURN_TIME_SECONDS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    START_SURFACE_RETURN_TIME_SECONDS_PARAM, 28800); // 8 hours

    public static final String START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS_PARAM =
            "start_surface_return_time_on_tablet_seconds";
    public static final IntCachedFieldTrialParameter START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS_PARAM, 28800); // 8 hours
    private static final String START_SURFACE_RETURN_TIME_USE_MODEL_PARAM =
            "start_surface_return_time_use_model";
    public static final BooleanCachedFieldTrialParameter START_SURFACE_RETURN_TIME_USE_MODEL =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    START_SURFACE_RETURN_TIME_USE_MODEL_PARAM, false);

    public static final BooleanCachedFieldTrialParameter SURFACE_POLISH_OMNIBOX_COLOR =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SURFACE_POLISH, "polish_omnibox_color", false);

    private static final String SURFACE_POLISH_MOVE_DOWN_LOGO_PARAM = "move_down_logo";
    public static final BooleanCachedFieldTrialParameter SURFACE_POLISH_MOVE_DOWN_LOGO =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SURFACE_POLISH, SURFACE_POLISH_MOVE_DOWN_LOGO_PARAM, false);

    private static final String SURFACE_POLISH_LESS_BRAND_SPACE_PARAM = "less_brand_space";
    public static final BooleanCachedFieldTrialParameter SURFACE_POLISH_LESS_BRAND_SPACE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SURFACE_POLISH, SURFACE_POLISH_LESS_BRAND_SPACE_PARAM, false);

    private static final String SURFACE_POLISH_SCROLLABLE_MVT_PARAM = "scrollable_mvt";
    public static final BooleanCachedFieldTrialParameter SURFACE_POLISH_SCROLLABLE_MVT =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SURFACE_POLISH, SURFACE_POLISH_SCROLLABLE_MVT_PARAM, false);

    public static final BooleanCachedFieldTrialParameter SURFACE_POLISH_USE_MAGIC_SPACE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SURFACE_POLISH, "use_magic_space", false);

    private static final String STARTUP_UMA_PREFIX = "Startup.Android.";
    private static final String INSTANT_START_SUBFIX = ".Instant";
    private static final String REGULAR_START_SUBFIX = ".NoInstant";

    /**
     * @return Whether the Start Surface feature flag is enabled.
     * @Deprecated Use {@link
     * org.chromium.chrome.browser.tasks.ReturnToChromeUtil#isStartSurfaceEnabled} instead.
     */
    public static boolean isStartSurfaceFlagEnabled() {
        return ChromeFeatureList.sStartSurfaceAndroid.isEnabled() && !SysUtils.isLowEndDevice();
    }

    /**
     * Returns whether showing a NTP as the home surface is enabled in the given context.
     */
    public static boolean isNtpAsHomeSurfaceEnabled(boolean isTablet) {
        return isTablet && ChromeFeatureList.sStartSurfaceOnTablet.isEnabled();
    }

    /**
     * Returns whether a magic space is enabled on Start surface.
     */
    public static boolean useMagicSpace() {
        return ChromeFeatureList.sSurfacePolish.isEnabled()
                && SURFACE_POLISH_USE_MAGIC_SPACE.getValue()
                && ChromeFeatureList.sStartSurfaceRefactor.isEnabled();
    }

    /**
     * Records histograms of showing the StartSurface. Nothing will be recorded if timeDurationMs
     * isn't valid.
     */
    public static void recordHistogram(String name, long timeDurationMs, boolean isInstantStart) {
        if (timeDurationMs < 0) return;
        Log.i(TAG, "Recorded %s = %d ms", getHistogramName(name, isInstantStart), timeDurationMs);
        RecordHistogram.recordTimesHistogram(
                getHistogramName(name, isInstantStart), timeDurationMs);
    }

    @VisibleForTesting
    public static String getHistogramName(String name, boolean isInstantStart) {
        return STARTUP_UMA_PREFIX + name
                + (isInstantStart ? INSTANT_START_SUBFIX : REGULAR_START_SUBFIX);
    }

    @CalledByNative
    private static boolean isBehaviouralTargetingEnabled() {
        return false;
    }

    static void setFeedVisibilityForTesting(boolean isVisible) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, isVisible);
    }
}
