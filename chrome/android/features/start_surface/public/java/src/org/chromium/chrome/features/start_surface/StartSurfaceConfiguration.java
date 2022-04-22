// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
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
    public static final BooleanCachedFieldTrialParameter START_SURFACE_EXCLUDE_QUERY_TILES =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "exclude_query_tiles", true);
    public static final BooleanCachedFieldTrialParameter
            START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB =
                    new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                            "hide_switch_when_no_incognito_tabs", false);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_LAST_ACTIVE_TAB_ONLY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_last_active_tab_only", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_NTP_INSTEAD_OF_START =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_ntp_instead_of_start", false);

    private static final String OMNIBOX_FOCUSED_ON_NEW_TAB_PARAM = "omnibox_focused_on_new_tab";
    public static final BooleanCachedFieldTrialParameter OMNIBOX_FOCUSED_ON_NEW_TAB =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    OMNIBOX_FOCUSED_ON_NEW_TAB_PARAM, false);

    private static final String SHOW_NTP_TILES_ON_OMNIBOX_PARAM = "show_ntp_tiles_on_omnibox";
    public static final BooleanCachedFieldTrialParameter SHOW_NTP_TILES_ON_OMNIBOX =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    SHOW_NTP_TILES_ON_OMNIBOX_PARAM, false);

    private static final String HOME_BUTTON_ON_GRID_TAB_SWITCHER_PARAM =
            "home_button_on_grid_tab_switcher";
    public static final BooleanCachedFieldTrialParameter HOME_BUTTON_ON_GRID_TAB_SWITCHER =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    HOME_BUTTON_ON_GRID_TAB_SWITCHER_PARAM, false);

    private static final String TAB_COUNT_BUTTON_ON_START_SURFACE_PARAM =
            "tab_count_button_on_start_surface";
    public static final BooleanCachedFieldTrialParameter TAB_COUNT_BUTTON_ON_START_SURFACE =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    TAB_COUNT_BUTTON_ON_START_SURFACE_PARAM, false);

    private static final String NEW_SURFACE_PARAM = "new_home_surface_from_home_button";
    public static final StringCachedFieldTrialParameter NEW_SURFACE_FROM_HOME_BUTTON =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, NEW_SURFACE_PARAM, "");

    private static final String SHOW_TABS_IN_MRU_ORDER_PARAM = "show_tabs_in_mru_order";
    public static final BooleanCachedFieldTrialParameter SHOW_TABS_IN_MRU_ORDER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SHOW_TABS_IN_MRU_ORDER_PARAM, false);

    private static final String SUPPORT_ACCESSIBILITY_PARAM = "support_accessibility";
    public static final BooleanCachedFieldTrialParameter SUPPORT_ACCESSIBILITY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SUPPORT_ACCESSIBILITY_PARAM, true);

    private static final String FINALE_ANIMATION_ENABLED_PARAM = "finale_animation_enabled";
    public static final BooleanCachedFieldTrialParameter FINALE_ANIMATION_ENABLED =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, FINALE_ANIMATION_ENABLED_PARAM, false);

    private static final String WARM_UP_RENDERER_PARAM = "warm_up_renderer";
    public static final BooleanCachedFieldTrialParameter WARM_UP_RENDERER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, WARM_UP_RENDERER_PARAM, false);

    private static final String SPARE_RENDERER_DELAY_MS_PARAM = "spare_renderer_delay_ms";
    public static final IntCachedFieldTrialParameter SPARE_RENDERER_DELAY_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SPARE_RENDERER_DELAY_MS_PARAM, 1000);

    private static final String CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP_PARAM =
            "check_sync_before_show_start_at_startup";
    public static final BooleanCachedFieldTrialParameter CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP_PARAM, false);

    private static final String BEHAVIOURAL_TARGETING_PARAM = "behavioural_targeting";
    public static final StringCachedFieldTrialParameter BEHAVIOURAL_TARGETING =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, BEHAVIOURAL_TARGETING_PARAM, "");

    private static final String USER_CLICK_THRESHOLD_PARAM = "user_clicks_threshold";
    public static final IntCachedFieldTrialParameter USER_CLICK_THRESHOLD =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    USER_CLICK_THRESHOLD_PARAM, Integer.MAX_VALUE);

    private static final String NUM_DAYS_KEEP_SHOW_START_AT_STARTUP_PARAM =
            "num_days_keep_show_start_at_startup";
    public static final IntCachedFieldTrialParameter NUM_DAYS_KEEP_SHOW_START_AT_STARTUP =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    NUM_DAYS_KEEP_SHOW_START_AT_STARTUP_PARAM, 7);

    private static final String NUM_DAYS_USER_CLICK_BELOW_THRESHOLD_PARAM =
            "num_days_user_click_below_threshold";
    public static final IntCachedFieldTrialParameter NUM_DAYS_USER_CLICK_BELOW_THRESHOLD =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    NUM_DAYS_USER_CLICK_BELOW_THRESHOLD_PARAM, 7);

    private static final String SIGNIN_PROMO_NTP_COUNT_LIMIT_PARAM = "signin_promo_NTP_count_limit";
    public static final IntCachedFieldTrialParameter SIGNIN_PROMO_NTP_COUNT_LIMIT =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    SIGNIN_PROMO_NTP_COUNT_LIMIT_PARAM, Integer.MAX_VALUE);

    private static final String SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS_PARAM =
            "signin_promo_NTP_since_first_time_shown_limit_hours";
    public static final IntCachedFieldTrialParameter
            SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS =
                    new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                            SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS_PARAM, -1);

    private static final String SIGNIN_PROMO_NTP_RESET_AFTER_HOURS_PARAM =
            "signin_promo_NTP_reset_after_hours";
    public static final IntCachedFieldTrialParameter SIGNIN_PROMO_NTP_RESET_AFTER_HOURS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    SIGNIN_PROMO_NTP_RESET_AFTER_HOURS_PARAM, -1);

    private static final String IS_DOODLE_SUPPORTED_PARAM = "is_doodle_supported";
    public static final BooleanCachedFieldTrialParameter IS_DOODLE_SUPPORTED =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, IS_DOODLE_SUPPORTED_PARAM, false);

    private static final String HIDE_START_WHEN_LAST_VISITED_TAB_IS_SRP_PARAM =
            "hide_start_when_last_visited_tab_is_srp";
    public static final BooleanCachedFieldTrialParameter HIDE_START_WHEN_LAST_VISITED_TAB_IS_SRP =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    HIDE_START_WHEN_LAST_VISITED_TAB_IS_SRP_PARAM, false);

    private static final String STARTUP_UMA_PREFIX = "Startup.Android.";
    private static final String INSTANT_START_SUBFIX = ".Instant";
    private static final String REGULAR_START_SUBFIX = ".NoInstant";

    /**
     * @return Whether the Start Surface feature flag is enabled.
     * @Deprecated Use {@link
     * org.chromium.chrome.browser.tasks.ReturnToChromeUtil#isStartSurfaceEnabled} instead.
     */
    public static boolean isStartSurfaceFlagEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.START_SURFACE_ANDROID)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Start Surface SinglePane is enabled.
     */
    public static boolean isStartSurfaceSinglePaneEnabled() {
        return isStartSurfaceFlagEnabled() && START_SURFACE_VARIATION.getValue().equals("single");
    }

    /**
     * @return the PageClassification type of the fake Omnibox on the Start surface homepage.
     */
    public static int getPageClassificationForHomepage() {
        // When NEW_SURFACE_FROM_HOME_BUTTON equals "hide_mv_tiles_and_tab_switcher", the MV (NTP)
        // tiles are removed from the Start surface homepage when tapping the home button. Thus, we
        // have to show MV (NTP) tiles when the fake omnibox get focusing, and there is no need to
        // check SHOW_NTP_TILES_ON_OMNIBOX anymore.
        return TextUtils.equals(
                       NEW_SURFACE_FROM_HOME_BUTTON.getValue(), "hide_mv_tiles_and_tab_switcher")
                ? PageClassification.START_SURFACE_HOMEPAGE_VALUE
                : PageClassification.NTP_VALUE;
    }

    /**
     * @return the PageClassification type of the new Tab.
     */
    public static int getPageClassificationForNewTab() {
        return SHOW_NTP_TILES_ON_OMNIBOX.getValue() ? PageClassification.START_SURFACE_NEW_TAB_VALUE
                                                    : PageClassification.NTP_VALUE;
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
     * Returns whether the given Tab has the flag of focusing on its Omnibox on the first time the
     * Tab is showing, and resets the flag in the Tab's UserData. This function returns true only
     * when {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled.
     */
    public static boolean consumeFocusOnOmnibox(Tab tab, @LayoutType int layout) {
        if (tab != null && tab.getUrl().isEmpty() && layout == LayoutType.BROWSING
                && StartSurfaceUserData.getFocusOnOmnibox(tab)) {
            assert OMNIBOX_FOCUSED_ON_NEW_TAB.getValue();
            StartSurfaceUserData.setFocusOnOmnibox(tab, false);
            return true;
        }
        return false;
    }

    /**
     * @return Whether the given tab should be treated as chrome://newTab. This function returns
     *         true only when {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled, the tab is newly
     *         created from the new Tab menu or "+" button, and it hasn't navigate to any URL yet.
     */
    public static boolean shouldHandleAsNtp(Tab tab) {
        // TODO(https://crbug.com/1305397): Rule out a null url here and assert that it's non-null.
        if (tab == null || tab.getUrl() == null) return false;

        return tab.getUrl().isEmpty() && StartSurfaceUserData.getCreatedAsNtp(tab);
    }

    /**
     * @return Whether the given tab with the given url should be treated as chrome://newTab. This
     *         function returns true only when {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled, the
     *         tab is newly created from the new Tab menu or "+" button, and it hasn't navigate to
     *         any URL yet.
     */
    // TODO(https://crbug.com/1305374): migrate to GURL.
    public static boolean shouldHandleAsNtp(Tab tab, String url) {
        if (tab == null || url == null) return false;

        return url.isEmpty() && StartSurfaceUserData.getCreatedAsNtp(tab);
    }

    /**
     * Sets the UserData if the Tab is a newly created empty Tab when
     * {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled.
     */
    public static void maySetUserDataForEmptyTab(Tab tab, String url) {
        if (!OMNIBOX_FOCUSED_ON_NEW_TAB.getValue() || tab == null || !TextUtils.isEmpty(url)
                || tab.getLaunchType() != TabLaunchType.FROM_START_SURFACE) {
            return;
        }
        StartSurfaceUserData.setFocusOnOmnibox(tab, true);
        StartSurfaceUserData.setCreatedAsNtp(tab);
    }

    /**
     * @return Whether new surface should show when home button is clicked.
     */
    public static boolean shouldShowNewSurfaceFromHomeButton() {
        return NEW_SURFACE_FROM_HOME_BUTTON.getValue().equals("hide_tab_switcher_only")
                || NEW_SURFACE_FROM_HOME_BUTTON.getValue().equals("hide_mv_tiles_and_tab_switcher");
    }

    /**
     * Returns whether to show the transition animations for the Finale version.
     */
    public static boolean shouldShowAnimationsForFinale() {
        return HOME_BUTTON_ON_GRID_TAB_SWITCHER.getValue() && FINALE_ANIMATION_ENABLED.getValue();
    }

    @CalledByNative
    private static boolean isBehaviouralTargetingEnabled() {
        return !TextUtils.isEmpty(BEHAVIOURAL_TARGETING.getValue());
    }

    @VisibleForTesting
    static void setFeedVisibilityForTesting(boolean isVisible) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, isVisible);
    }

    @NativeMethods
    interface Natives {
        // Native methods
        void warmupRenderer(Profile profile);
    }
}
