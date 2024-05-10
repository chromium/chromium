// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.base.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoUtils.LogoSizeForLogoPolish;

/**
 * Flag configuration for Start Surface. Source of truth for whether it should be enabled and which
 * variation should be used.
 */
public class StartSurfaceConfiguration {
    private static final String TAG = "StartSurfaceConfig";
    public static final BooleanCachedFieldTrialParameter
            START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB =
                    ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                            ChromeFeatureList.START_SURFACE_ANDROID,
                            "hide_switch_when_no_incognito_tabs",
                            true);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_NTP_INSTEAD_OF_START =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_ntp_instead_of_start", true);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_START_AS_HOMEPAGE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_start_as_homepage", false);

    private static final String SIGNIN_PROMO_NTP_COUNT_LIMIT_PARAM = "signin_promo_NTP_count_limit";
    public static final IntCachedFieldTrialParameter SIGNIN_PROMO_NTP_COUNT_LIMIT =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SIGNIN_PROMO_NTP_COUNT_LIMIT_PARAM, 5);

    private static final String SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS_PARAM =
            "signin_promo_NTP_since_first_time_shown_limit_hours";
    public static final IntCachedFieldTrialParameter
            SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS =
                    ChromeFeatureList.newIntCachedFieldTrialParameter(
                            ChromeFeatureList.START_SURFACE_ANDROID,
                            SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS_PARAM,
                            336);

    private static final String SIGNIN_PROMO_NTP_RESET_AFTER_HOURS_PARAM =
            "signin_promo_NTP_reset_after_hours";
    public static final IntCachedFieldTrialParameter SIGNIN_PROMO_NTP_RESET_AFTER_HOURS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID,
                    SIGNIN_PROMO_NTP_RESET_AFTER_HOURS_PARAM,
                    672);

    private static final String IS_DOODLE_SUPPORTED_PARAM = "is_doodle_supported";
    public static final BooleanCachedFieldTrialParameter IS_DOODLE_SUPPORTED =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, IS_DOODLE_SUPPORTED_PARAM, false);

    // Start return time experiment:
    @VisibleForTesting
    public static final String START_SURFACE_RETURN_TIME_SECONDS_PARAM =
            "start_surface_return_time_seconds";

    public static final IntCachedFieldTrialParameter START_SURFACE_RETURN_TIME_SECONDS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    START_SURFACE_RETURN_TIME_SECONDS_PARAM,
                    28800); // 8 hours

    // Equivalent to the START_SURFACE_RETURN_TIME_SECONDS, but allows a different default value
    // other than 8 hours. This parameter isn't just used on tablets anymore.
    public static final String START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS_PARAM =
            "start_surface_return_time_on_tablet_seconds";
    public static final IntCachedFieldTrialParameter START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS_PARAM,
                    28800); // 8 hours
    private static final String START_SURFACE_RETURN_TIME_USE_MODEL_PARAM =
            "start_surface_return_time_use_model";
    public static final BooleanCachedFieldTrialParameter START_SURFACE_RETURN_TIME_USE_MODEL =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_RETURN_TIME,
                    START_SURFACE_RETURN_TIME_USE_MODEL_PARAM,
                    false);

    private static final String SURFACE_POLISH_SCROLLABLE_MVT_PARAM = "scrollable_mvt";
    public static final BooleanCachedFieldTrialParameter SURFACE_POLISH_SCROLLABLE_MVT =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SURFACE_POLISH, SURFACE_POLISH_SCROLLABLE_MVT_PARAM, true);

    private static final String LOGO_POLISH_LARGE_SIZE_PARAM = "polish_logo_size_large";
    public static final BooleanCachedFieldTrialParameter LOGO_POLISH_LARGE_SIZE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LOGO_POLISH, LOGO_POLISH_LARGE_SIZE_PARAM, false);

    private static final String LOGO_POLISH_MEDIUM_SIZE_PARAM = "polish_logo_size_medium";
    public static final BooleanCachedFieldTrialParameter LOGO_POLISH_MEDIUM_SIZE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.LOGO_POLISH, LOGO_POLISH_MEDIUM_SIZE_PARAM, false);

    private static final String STARTUP_UMA_PREFIX = "Startup.Android.";

    /**
     * @return Whether the Start Surface feature flag is enabled.
     * @deprecated Use {@link
     *     org.chromium.chrome.browser.tasks.ReturnToChromeUtil#isStartSurfaceEnabled} instead.
     */
    public static boolean isStartSurfaceFlagEnabled() {
        return ChromeFeatureList.sStartSurfaceAndroid.isEnabled() && !SysUtils.isLowEndDevice();
    }

    /** Returns whether showing a NTP as the home surface is enabled in the given context. */
    public static boolean isNtpAsHomeSurfaceEnabled(boolean isTablet) {
        // ReturnToChromeUtil#isStartSurfaceEnabled() will return false when
        // ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID is enabled.
        return isTablet || !isTablet && ChromeFeatureList.sShowNtpAtStartupAndroid.isEnabled();
    }

    /** Returns whether a magic stack is enabled on Start surface. */
    public static boolean useMagicStack() {
        return ChromeFeatureList.sMagicStackAndroid.isEnabled();
    }

    /** Returns whether logo polish flag is enabled in the given context. */
    public static boolean isLogoPolishEnabled() {
        return ChromeFeatureList.sLogoPolish.isEnabled();
    }

    /**
     * Returns whether logo is Google doodle and logo polish is enabled in the given context.
     *
     * @param isLogoDoodle True if the current logo is Google doodle.
     */
    public static boolean isLogoPolishEnabledWithGoogleDoodle(boolean isLogoDoodle) {
        return isLogoDoodle && isLogoPolishEnabled();
    }

    /**
     * Returns the logo size to use when logo polish is enabled. When logo polish is disabled, the
     * return value should be invalid.
     */
    public static @LogoSizeForLogoPolish int getLogoSizeForLogoPolish() {
        if (StartSurfaceConfiguration.LOGO_POLISH_LARGE_SIZE.getValue()) {
            return LogoSizeForLogoPolish.LARGE;
        }

        if (StartSurfaceConfiguration.LOGO_POLISH_MEDIUM_SIZE.getValue()) {
            return LogoSizeForLogoPolish.MEDIUM;
        }

        return LogoSizeForLogoPolish.SMALL;
    }

    /**
     * Records histograms of showing the StartSurface. Nothing will be recorded if timeDurationMs
     * isn't valid.
     */
    public static void recordHistogram(String name, long timeDurationMs) {
        if (timeDurationMs < 0) return;

        String histogramName = getHistogramName(name);
        Log.i(TAG, "Recorded %s = %d ms", histogramName, timeDurationMs);
        RecordHistogram.recordTimesHistogram(histogramName, timeDurationMs);
    }

    @VisibleForTesting
    public static String getHistogramName(String name) {
        return STARTUP_UMA_PREFIX + name;
    }

    @CalledByNative
    private static boolean isBehaviouralTargetingEnabled() {
        return false;
    }
}
