// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.build.annotations.NullMarked;

import java.util.Arrays;
import java.util.List;

/**
 * Test-only allowlist of grandfathered SharedPreferences keys and key prefixes that do not conform
 * to the "Chrome.[Feature].[Key]" format.
 *
 * <p>ChromePreferenceKeysTest verifies that every legacy-format key and prefix registered in any
 * {@link org.chromium.base.shared_preferences.PreferenceKeyRegistry} (e.g. {@link
 * ChromePreferenceKeys#getLegacyKeysInUse()} or {@link ProfilePreferenceKeys#getLegacyKeysInUse()})
 * is listed here, so existing legacy keys can be moved between registries or deprecated freely,
 * while adding a brand-new unformatted key or prefix requires an OWNERS-approved change to this
 * file.
 *
 * <p>Values are string literals (like {@link DeprecatedChromePreferenceKeys}) so that moving or
 * deleting constants does not require changing this file.
 *
 * <p>Do not add new entries to this list unless you are migrating old SharedPreferences keys.
 * Instead, declare new keys in the format "Chrome.[Feature].[Key]", for example
 * "Chrome.FooBar.FooEnabled", and add them to {@link ChromePreferenceKeys#getKeysInUse()} or {@link
 * ProfilePreferenceKeys#getKeysInUse()}.
 */
@NullMarked
final class LegacyChromePreferenceKeys {
    /**
     * Returns the grandfathered keys that do not conform to the "Chrome.[Feature].[Key]" format.
     */
    static List<String> getKeysForTesting() {
        return Arrays.asList(
                "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED",
                "ChromeTabbedActivity.BackgroundTimeMs",
                "DownloadUmaEntry",
                "ForegroundServiceObservers",
                "LocaleManager_PARTNER_PROMO_SELECTED_KEYWORD",
                "LocaleManager_PREF_AUTO_SWITCH",
                "LocaleManager_PREF_PROMO_SHOWN",
                "LocaleManager_PREF_PROMO_VER2_CHECKED",
                "LocaleManager_SEARCH_WIDGET_PRESENT_FIRST_START",
                "LocaleManager_SHOULD_REPING_RLZ_FOR_SEARCH_PROMO_KEYWORD",
                "LocaleManager_WAS_IN_SPECIAL_LOCALE",
                "MainIntent.LaunchCount",
                "MainIntent.LaunchTimestamp",
                "ManagedSpace.FailedBuildVersion",
                "NextDownloadNotificationId",
                "NotificationUmaTracker.LastShownNotificationType",
                "PendingDownloadNotifications",
                "PendingOMADownloads",
                "ResumptionAttemptLeft",
                "ToS acknowledged accounts",
                "WebRTCNotificationIds",
                "android_backup_failure_count",
                "android_os_unsupported_chrome_version",
                "android_restore_status",
                "android_restore_status_recorded",
                "applink.chrome_default_browser",
                "autofill_assistant_switch",
                "browser_crash_failure_upload",
                "browser_crash_success_upload",
                "channels_version_key",
                "check_save_card_to_device",
                "com.android.chrome.MISSING_TIMEZONES",
                "com.android.chrome.SEARCH_ENGINE_PROMO_SHOWN",
                "com.google.android.apps.chrome.ChromeMobileApplication.BOOT_TIMESTAMP",
                "content_suggestions_shown",
                "contextual_search_tap_quick_answer_count",
                "contextual_search_tap_triggered_promo_count",
                "developer",
                "enhanced_bookmark_last_used_parent_folder",
                "enhanced_bookmark_last_used_url",
                "first_backup_done",
                "first_run_flow",
                "first_run_signin_account_name",
                "first_run_signin_setup",
                "google.services.username",
                "gpu_crash_failure_upload",
                "gpu_crash_success_upload",
                "history_home_show_info",
                "homepage",
                "homepage_custom_uri",
                "homepage_partner_enabled",
                "in_metrics_sample",
                "incognito-shortcut-added",
                "last_bookmark_folder_id",
                "lightweight_first_run_flow",
                "locale",
                "notification_trigger_scheduler.next_trigger",
                "ntp.personalized_signin_promo_dismissed",
                "ntp.signin_promo_suppression_period_start",
                "ntp_snippets.is_scheduled",
                "offline_auto_fetch_showing_in_progress",
                "offline_auto_fetch_user_cancel_action_in_progress",
                "offline_indicator_v2_enabled",
                "org.chromium.chrome.browser.download.IS_DOWNLOAD_HOME_ENABLED",
                "org.chromium.chrome.browser.settings.privacy.PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN",
                "org.chromium.chrome.browser.tab.TabIdManager.NEXT_ID",
                "org.chromium.chrome.browser.tabmodel.TabPersistentStore.ACTIVE_TAB_ID",
                "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_COMPUTED_MAX_ID",
                "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_RUN_FILE_MIGRATION",
                "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION",
                "other_crash_failure_upload",
                "other_crash_success_upload",
                "payment_complete_once",
                "pref_last_custom_tab_url",
                "promos_skipped_on_first_start",
                "renderer_crash_failure_upload",
                "renderer_crash_success_upload",
                "rlz_first_search_notified",
                "search_engine_choice_chrome_version",
                "search_engine_choice_default_type_before",
                "search_engine_choice_presented_version",
                "search_engine_choice_propagated_package_name",
                "search_engine_choice_requested_timestamp",
                "send_tab_to_self.notification.active",
                "send_tab_to_self.notification.next_id",
                "signin_promo_bookmarks_declined",
                "signin_promo_last_shown_account_names",
                "signin_promo_last_shown_chrome_version",
                "skip_welcome_page",
                "snapshot_database_removed",
                "sync_error_infobar_shown_shown_at_time",
                "tracing_categories",
                "tracing_mode",
                "trusted_web_activity_disclosure_accepted_packages",
                "ui_theme_setting",
                "verified_digital_asset_links",
                "webapk_uninstalled_packages");
    }

    /** Returns the grandfathered key prefixes that do not conform to the format. */
    static List<KeyPrefix> getPrefixesForTesting() {
        return Arrays.asList(
                new KeyPrefix("payment_instrument_use_count_*"),
                new KeyPrefix("payment_instrument_use_date_*"),
                new KeyPrefix("pref_local_custom_tabs_module_dex_last_update_time_*"));
    }

    private LegacyChromePreferenceKeys() {}
}
