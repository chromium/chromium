// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.build.annotations.CheckDiscard;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * These values have been used as SharedPreferences keys in the past and should not be reused.
 * Do not remove values from this list.
 */
@CheckDiscard("Validation is performed in tests and in debug builds.")
public class DeprecatedChromePreferenceKeys {
    static List<String> getKeysForTesting() {
        // clang-format off
        return Arrays.asList(
                "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN",
                "BANDWIDTH_REDUCTION_PROXY_ENABLED",
                "BANDWIDTH_REDUCTION_FIRST_ENABLED_TIME",
                "Chrome.AccountPickerBottomSheet.ShownCount",
                "Chrome.AutofillAssistant.NumberOfLiteScriptsCanceled",
                "Chrome.Flags.PaintPreviewTestEnabled",
                "Chrome.Flags.SafeBool.*",
                "Chrome.Flags.SafeDouble.*",
                "Chrome.Flags.SafeInt.*",
                "Chrome.Flags.SafeString.*",
                "Chrome.Flags.SafeValuesVersion",
                "Chrome.OfflineMeasurements.HttpProbeResultsList",
                "Chrome.OfflineMeasurements.IsAirplaneModeEnabledList",
                "Chrome.OfflineMeasurements.IsRoaming",
                "Chrome.OfflineMeasurements.TimeBetweenChecksMillisList",
                "Chrome.OfflineMeasurements.UserStateList",
                "Chrome.PriceTracking.PriceDropAlerts",
                "Chrome.RequestDesktopSiteGlobalSetting.DefaultEnabledShowMessage",
                "Chrome.RequestDesktopSiteGlobalSetting.OptInMessageShown",
                "Chrome.SigninPromo.NTPImpressions",
                "PersistedNotificationId",
                "PhysicalWeb.ActivityReferral",
                "PhysicalWeb.HasDeferredMetrics",
                "PhysicalWeb.OptIn.DeclineButtonPressed",
                "PhysicalWeb.OptIn.EnableButtonPressed",
                "PhysicalWeb.Prefs.FeatureDisabled",
                "PhysicalWeb.Prefs.FeatureEnabled",
                "PhysicalWeb.Prefs.LocationDenied",
                "PhysicalWeb.Prefs.LocationGranted",
                "PhysicalWeb.ResolveTime.Background",
                "PhysicalWeb.ResolveTime.Foreground",
                "PhysicalWeb.ResolveTime.Refresh",
                "PhysicalWeb.State",
                "PhysicalWeb.TotalUrls.OnInitialDisplay",
                "PhysicalWeb.TotalUrls.OnRefresh",
                "PhysicalWeb.UrlSelected",
                "PrefMigrationVersion",
                "ServiceManagerFeatures",
                "allow_low_end_device_ui",
                "allow_prefetch",
                "allow_starting_service_manager_only",
                "bookmark_search_history",
                "bottom_toolbar_enabled",
                "bottom_toolbar_variation",
                "cellular_experiment",
                "chrome_home_enabled_date",
                "chrome_home_info_promo_shown",
                "chrome_home_opt_out_snackbar_shown",
                "chrome_home_survey_info_bar_displayed",
                "chrome_home_user_enabled",
                "chrome_modern_design_enabled",
                "chromium.invalidations.uuid",
                "click_to_call_open_dialer_directly",
                "contextual_search_all_time_open_count",
                "contextual_search_all_time_tap_count",
                "contextual_search_all_time_tap_quick_answer_count",
                "contextual_search_clicks_week_*",
                "contextual_search_current_week_number",
                "contextual_search_entity_impressions_count",
                "contextual_search_entity_opens_count",
                "contextual_search_impressions_week_*",
                "contextual_search_last_animation_time",
                "contextual_search_newest_week",
                "contextual_search_oldest_week",
                "contextual_search_previous_interaction_encoded_outcomes",
                "contextual_search_previous_interaction_event_id",
                "contextual_search_previous_interaction_timestamp",
                "contextual_search_promo_open_count",
                "contextual_search_quick_action_impressions_count",
                "contextual_search_quick_actions_ignored_count",
                "contextual_search_quick_actions_taken_count",
                "contextual_search_tap_count",
                "crash_dump_upload",
                "crash_dump_upload_no_cellular",
                "data_reduction_site_breakdown_allowed_date",
                "displayed_data_reduction_infobar_promo",
                "displayed_data_reduction_infobar_promo_version",
                "displayed_data_reduction_promo",
                "displayed_data_reduction_promo_time_ms",
                "displayed_data_reduction_promo_version",
                "displayed_data_reduction_snackbar_promo_saved_bytes",
                "darken_websites_enabled",
                "fre_promo_opt_out",
                "home_page_button_force_enabled",
                "homepage_tile_enabled",
                "inflate_toolbar_on_background_thread",
                "interest_feed_content_suggestions",
                "labeled_bottom_toolbar_enabled",
                "last_shared_class_name",
                "last_shared_package_name",
                "night_mode_available",
                "night_mode_cct_available",
                "night_mode_default_to_light",
                "ntp_button_enabled",
                "ntp_button_variant",
                "physical_web",
                "physical_web_sharing",
                "prefetch_bandwidth",
                "prefetch_bandwidth_no_cellular",
                "prefetch_notification_enabled",
                "prefetch_notification_has_new_pages",
                "prefetch_notification_ignored_counter",
                "prefetch_notification_offline_counter",
                "prefetch_notification_shown_time",
                "prioritize_bootstrap_tasks",
                "service_manager_for_background_prefetch",
                "service_manager_for_download_resumption",
                "signin_promo_impressions_count_bookmarks",
                "signin_promo_impressions_count_settings",
                "sole_integration_enabled",
                "start_surface_single_pane_enabled",
                "tab_persistent_store_task_runner_enabled",
                "webapk_number_of_uninstalls",
                "website_settings_filter"
        );
        // clang-format on
    }

    static List<KeyPrefix> getPrefixesForTesting() {
        return Collections.EMPTY_LIST;
    }
}
