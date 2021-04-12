// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.annotations.CheckDiscard;

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
                "Chrome.Flags.PaintPreviewTestEnabled",
                "Chrome.PriceTracking.PriceDropAlerts",
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
                "crash_dump_upload",
                "crash_dump_upload_no_cellular",
                "home_page_button_force_enabled",
                "homepage_tile_enabled",
                "inflate_toolbar_on_background_thread",
                "interest_feed_content_suggestions",
                "labeled_bottom_toolbar_enabled",
                "night_mode_available",
                "night_mode_cct_available",
                "night_mode_default_to_light",
                "ntp_button_enabled",
                "ntp_button_variant",
                "physical_web",
                "physical_web_sharing",
                "prefetch_bandwidth",
                "prefetch_bandwidth_no_cellular",
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
