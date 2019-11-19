// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.annotations.CheckDiscard;

import java.util.Arrays;
import java.util.List;

/**
 * Contains String constants with the SharedPreferences keys used by Chrome.
 *
 * All Chrome layer SharedPreferences keys should be:
 * - Added here as a constants
 * - Follow the format "Chrome.[Feature].[Key]"
 * - Added to the list of used keys in {@link ChromePreferenceKeys#createUsedKeys()}:
 *
 * Tests in ChromePreferenceKeysTest ensure the sanity of this file.
 */
public final class ChromePreferenceKeys {
    /** An all-time counter of taps that triggered the Contextual Search peeking panel. */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT =
            "contextual_search_all_time_tap_count";
    /** An all-time counter of Contextual Search panel opens triggered by any gesture.*/
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT =
            "contextual_search_all_time_open_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT =
            "contextual_search_all_time_tap_quick_answer_count";
    /**
     * The number of times that a tap triggered the Contextual Search panel to peek since the last
     * time the panel was opened.  Note legacy string value without "open".
     */
    public static final String CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT =
            "contextual_search_tap_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown since
     * the last time the panel was opened.  Note legacy string value without "open".
     */
    public static final String CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT =
            "contextual_search_tap_quick_answer_count";
    /**
     * The number of times the Contextual Search panel was opened with the opt-in promo visible.
     */
    public static final String CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT =
            "contextual_search_promo_open_count";
    /**
     * The entity-data impressions count for Contextual Search, i.e. thumbnails shown in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT =
            "contextual_search_entity_impressions_count";
    /**
     * The entity-data opens count for Contextual Search, e.g. Panel opens following thumbnails
     * shown in the Bar. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT =
            "contextual_search_entity_opens_count";
    /**
     * The Quick Action impressions count for Contextual Search, i.e. actions presented in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT =
            "contextual_search_quick_action_impressions_count";
    /**
     * The Quick Actions taken count for Contextual Search, i.e. phone numbers dialed and similar
     * actions. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT =
            "contextual_search_quick_actions_taken_count";
    /**
     * The Quick Actions ignored count, i.e. phone numbers available but not dialed.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT =
            "contextual_search_quick_actions_ignored_count";
    /**
     * A user interaction event ID for interaction with Contextual Search, stored as a long.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID =
            "contextual_search_previous_interaction_event_id";
    /**
     * An encoded set of outcomes of user interaction with Contextual Search, stored as an int.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES =
            "contextual_search_previous_interaction_encoded_outcomes";
    /**
     * A timestamp indicating when we updated the user interaction with Contextual Search, stored
     * as a long, with resolution in days.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP =
            "contextual_search_previous_interaction_timestamp";
    public static final String CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT =
            "contextual_search_tap_triggered_promo_count";
    public static final String CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME =
            "contextual_search_last_animation_time";
    public static final String CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER =
            "contextual_search_current_week_number";

    /**
     * Whether the promotion for data reduction has been skipped on first invocation.
     * Default value is false.
     */
    public static final String PROMOS_SKIPPED_ON_FIRST_START = "promos_skipped_on_first_start";
    public static final String SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION =
            "signin_promo_last_shown_chrome_version";
    public static final String SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES =
            "signin_promo_last_shown_account_names";

    /**
     * Whether Chrome is set as the default browser.
     * Default value is false.
     */
    public static final String CHROME_DEFAULT_BROWSER = "applink.chrome_default_browser";

    /**
     * The current theme setting in the user settings.
     * Default value is -1. Use NightModeUtils#getThemeSetting() to retrieve current setting or
     * default theme.
     */
    public static final String UI_THEME_SETTING_KEY = "ui_theme_setting";

    /**
     * Whether or not darken websites is enabled.
     * Default value is false.
     */
    public static final String DARKEN_WEBSITES_ENABLED_KEY = "darken_websites_enabled";

    /**
     * Marks that the content suggestions surface has been shown.
     * Default value is false.
     */
    public static final String CONTENT_SUGGESTIONS_SHOWN_KEY = "content_suggestions_shown";

    /**
     * Whether the user dismissed the personalized sign in promo from the Settings.
     * Default value is false.
     */
    public static final String SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED =
            "settings_personalized_signin_promo_dismissed";
    /**
     * Whether the user dismissed the personalized sign in promo from the new tab page.
     * Default value is false.
     */
    public static final String NTP_SIGNIN_PROMO_DISMISSED =
            "ntp.personalized_signin_promo_dismissed";

    public static final String NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START =
            "ntp.signin_promo_suppression_period_start";

    public static final String SUCCESS_UPLOAD_SUFFIX = "_crash_success_upload";
    public static final String FAILURE_UPLOAD_SUFFIX = "_crash_failure_upload";

    public static final String VERIFIED_DIGITAL_ASSET_LINKS = "verified_digital_asset_links";
    public static final String TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES =
            "trusted_web_activity_disclosure_accepted_packages";

    /**
     * Whether VR assets component should be registered on startup.
     * Default value is false.
     */
    public static final String SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP =
            "should_register_vr_assets_component_on_startup";

    /*
     * Whether the simplified tab switcher is enabled when accessibility mode is enabled. Keep in
     * sync with accessibility_preferences.xml.
     * Default value is true.
     */
    public static final String ACCESSIBILITY_TAB_SWITCHER = "accessibility_tab_switcher";

    /**
     * When the user is shown a badge that the current Android OS version is unsupported, and they
     * tap it to display the menu (which has additional information), we store the current version
     * of Chrome to this preference to ensure we only show the badge once. The value is cleared
     * if the Chrome version later changes.
     */
    public static final String LATEST_UNSUPPORTED_VERSION = "android_os_unsupported_chrome_version";

    /**
     * Keys for deferred recording of the outcomes of showing the clear data dialog after
     * Trusted Web Activity client apps are uninstalled or have their data cleared.
     */
    public static final String TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL =
            "twa_dialog_number_of_dismissals_on_uninstall";
    public static final String TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA =
            "twa_dialog_number_of_dismissals_on_clear_data";

    /** Key for deferred recording of list of uninstalled WebAPK packages. */
    public static final String WEBAPK_UNINSTALLED_PACKAGES = "webapk_uninstalled_packages";

    /**
     * Contains a trial group that was used to determine whether the reached code profiler should be
     * enabled.
     */
    public static final String REACHED_CODE_PROFILER_GROUP_KEY = "reached_code_profiler_group";

    /**
     * Key to cache whether offline indicator v2 (persistent offline indicator) is enabled.
     */
    public static final String OFFLINE_INDICATOR_V2_ENABLED_KEY = "offline_indicator_v2_enabled";

    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> createUsedKeys() {
        // These values are currently used as SharedPreferences keys.
        // To deprecate a key that is not used anymore:
        // 1. Add its constant value to |sDeprecatedKeys|
        // 2. Remove the key from |sUsedKeys|
        // 3. Delete the constant.

        // clang-format off
        return Arrays.asList(
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP,
                CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT,
                CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME,
                CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER,
                PROMOS_SKIPPED_ON_FIRST_START,
                SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION,
                SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES,
                CHROME_DEFAULT_BROWSER,
                UI_THEME_SETTING_KEY,
                DARKEN_WEBSITES_ENABLED_KEY,
                CONTENT_SUGGESTIONS_SHOWN_KEY,
                SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START,
                SUCCESS_UPLOAD_SUFFIX,
                FAILURE_UPLOAD_SUFFIX,
                VERIFIED_DIGITAL_ASSET_LINKS,
                TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES,
                SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP,
                ACCESSIBILITY_TAB_SWITCHER,
                LATEST_UNSUPPORTED_VERSION,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA,
                WEBAPK_UNINSTALLED_PACKAGES,
                REACHED_CODE_PROFILER_GROUP_KEY,
                OFFLINE_INDICATOR_V2_ENABLED_KEY
        );
        // clang-format on
    }

    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> createDeprecatedKeysForTesting() {
        // These values have been used as SharedPreferences keys in the past and should not be
        // reused. Do not remove values from this list.

        // clang-format off
        return Arrays.asList(
                "allow_low_end_device_ui",
                "website_settings_filter",
                "chrome_modern_design_enabled",
                "home_page_button_force_enabled",
                "homepage_tile_enabled",
                "ntp_button_enabled",
                "ntp_button_variant",
                "tab_persistent_store_task_runner_enabled",
                "inflate_toolbar_on_background_thread",
                "sole_integration_enabled",
                "webapk_number_of_uninstalls",
                "allow_starting_service_manager_only",
                "chrome_home_user_enabled",
                "chrome_home_opt_out_snackbar_shown",
                "chrome_home_info_promo_shown",
                "chrome_home_enabled_date",
                "PrefMigrationVersion"
        );
        // clang-format on
    }

    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> createGrandfatheredFormatKeysForTesting() {
        // Do not add new constants to this list. Instead, declare new keys in the format
        // "Chrome.[Feature].[Key]", for example "Chrome.FooBar.FooEnabled".

        // clang-format off
        return Arrays.asList(
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT,
                CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT,
                CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT,
                CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES,
                CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP,
                CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT,
                CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME,
                CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER,
                PROMOS_SKIPPED_ON_FIRST_START,
                SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION,
                SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES,
                CHROME_DEFAULT_BROWSER,
                UI_THEME_SETTING_KEY,
                DARKEN_WEBSITES_ENABLED_KEY,
                CONTENT_SUGGESTIONS_SHOWN_KEY,
                SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_DISMISSED,
                NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START,
                SUCCESS_UPLOAD_SUFFIX,
                FAILURE_UPLOAD_SUFFIX,
                VERIFIED_DIGITAL_ASSET_LINKS,
                TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES,
                SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP,
                ACCESSIBILITY_TAB_SWITCHER,
                LATEST_UNSUPPORTED_VERSION,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL,
                TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA,
                WEBAPK_UNINSTALLED_PACKAGES,
                REACHED_CODE_PROFILER_GROUP_KEY,
                OFFLINE_INDICATOR_V2_ENABLED_KEY
        );
        // clang-format on
    }

    private ChromePreferenceKeys() {}
}
