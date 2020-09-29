// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Flag configuration for Start Surface. Source of truth for whether it should be enabled and
 * which variation should be used.
 */
public class StartSurfaceConfiguration {
    private static final String TAG = "StartSurfaceConfig";
    public static final StringCachedFieldTrialParameter START_SURFACE_VARIATION =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "start_surface_variation", "");
    public static final BooleanCachedFieldTrialParameter START_SURFACE_EXCLUDE_MV_TILES =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "exclude_mv_tiles", false);
    public static final BooleanCachedFieldTrialParameter
            START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB =
                    new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                            "hide_switch_when_no_incognito_tabs", false);

    // This parameter hides the incognito switch on non-incognito Start Surface homepage but still
    // shows the switch in Grid tab switcher. It is different from
    // {@link START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB} which hides the incognito switch in both
    // Start Surface homepage and Grid tab switcher whenever there is no incognito tab.
    public static final BooleanCachedFieldTrialParameter START_SURFACE_HIDE_INCOGNITO_SWITCH =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "hide_incognito_switch", false);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_LAST_ACTIVE_TAB_ONLY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_last_active_tab_only", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_SHOW_STACK_TAB_SWITCHER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_stack_tab_switcher", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_NTP_INSTEAD_OF_START =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_ntp_instead_of_start", false);
    public static final StringCachedFieldTrialParameter START_SURFACE_OMNIBOX_SCROLL_MODE =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "omnibox_scroll_mode", "");

    private static final String TRENDY_ENABLED_PARAM = "trendy_enabled";
    public static final BooleanCachedFieldTrialParameter TRENDY_ENABLED =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, TRENDY_ENABLED_PARAM, false);

    private static final String SUCCESS_MIN_PERIOD_MS_PARAM = "trendy_success_min_period_ms";
    public static final IntCachedFieldTrialParameter TRENDY_SUCCESS_MIN_PERIOD_MS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    SUCCESS_MIN_PERIOD_MS_PARAM, 86400_000);

    private static final String FAILURE_MIN_PERIOD_MS_PARAM = "trendy_failure_min_period_ms";
    public static final IntCachedFieldTrialParameter TRENDY_FAILURE_MIN_PERIOD_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, FAILURE_MIN_PERIOD_MS_PARAM, 7200_000);

    private static final String TRENDY_ENDPOINT_PARAM = "trendy_endpoint";
    public static final StringCachedFieldTrialParameter TRENDY_ENDPOINT =
            new StringCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    TRENDY_ENDPOINT_PARAM,
                    "https://trends.google.com/trends/trendingsearches/daily/rss"
                            + "?lite=true&safe=true&geo=");

    private static final String STARTUP_UMA_PREFIX = "Startup.Android.";
    private static final String INSTANT_START_SUBFIX = ".Instant";
    private static final String REGULAR_START_SUBFIX = ".NoInstant";

    /**
     * @return Whether the Start Surface is enabled.
     */
    public static boolean isStartSurfaceEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.START_SURFACE_ANDROID)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Start Surface SinglePane is enabled.
     */
    public static boolean isStartSurfaceSinglePaneEnabled() {
        return isStartSurfaceEnabled() && START_SURFACE_VARIATION.getValue().equals("single");
    }

    /**
     *@return Whether the Start Surface Stack Tab Switcher is enabled.
     */
    public static boolean isStartSurfaceStackTabSwitcherEnabled() {
        return isStartSurfaceSinglePaneEnabled()
                && START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue();
    }

    /**
     * Add an observer to keep {@link ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE} consistent
     * with {@link Pref.ARTICLES_LIST_VISIBLE}.
     */
    public static void addFeedVisibilityObserver() {
        updateFeedVisibility();
        PrefChangeRegistrar prefChangeRegistrar = new PrefChangeRegistrar();
        prefChangeRegistrar.addObserver(
                Pref.ARTICLES_LIST_VISIBLE, StartSurfaceConfiguration::updateFeedVisibility);
    }

    private static void updateFeedVisibility() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE,
                UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getBoolean(Pref.ARTICLES_LIST_VISIBLE));
    }

    /**
     * @return Whether the Feed articles are visible.
     */
    public static boolean getFeedArticlesVisibility() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, true);
    }

    @VisibleForTesting
    static void setFeedVisibilityForTesting(boolean isVisible) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, isVisible);
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
    /**
     * @param isDense Whether the placeholder of Feed is dense. This depends on whether the first
     *         article card of Feed is dense.
     */
    public static void setFeedPlaceholderDense(boolean isDense) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_PLACEHOLDER_DENSE, isDense);
    }

    /**
     * @return Whether the placeholder of Feed is dense.
     */
    public static boolean isFeedPlaceholderDense() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FEED_PLACEHOLDER_DENSE, false);
    }
}
