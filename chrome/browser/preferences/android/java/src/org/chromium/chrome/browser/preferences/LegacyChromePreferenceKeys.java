// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.build.annotations.CheckDiscard;

import java.util.Arrays;
import java.util.List;

/**
 * Do not add new constants to this list unless you are migrating old SharedPreferences keys.
 * Instead, declare new keys in the format "Chrome.[Feature].[Key]", for example
 * "Chrome.FooBar.FooEnabled", and add them to {@link ChromePreferenceKeys#getKeysInUse()}.
 */
@CheckDiscard("Validation is performed in tests and in debug builds.")
public class LegacyChromePreferenceKeys {
    /**
     * @return The list of [keys in use] that do not conform to the "Chrome.[Feature].[Key]" format.
     */
    static List<String> getKeysInUse() {
        return Arrays.asList(
                ChromePreferenceKeys.APP_LOCALE,
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED,
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED,
                ChromePreferenceKeys.BACKUP_FIRST_BACKUP_DONE,
                ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME,
                ChromePreferenceKeys.BOOKMARKS_LAST_MODIFIED_FOLDER_ID,
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL,
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT,
                ChromePreferenceKeys.CHROME_DEFAULT_BROWSER,
                ChromePreferenceKeys.CONTENT_SUGGESTIONS_SHOWN,
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT,
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT,
                ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_BROWSER,
                ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_GPU,
                ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_OTHER,
                ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_RENDERER,
                ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_BROWSER,
                ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_GPU,
                ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_OTHER,
                ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_RENDERER,
                ChromePreferenceKeys.CUSTOM_TABS_LAST_URL,
                ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI,
                ChromePreferenceKeys.DOWNLOAD_AUTO_RESUMPTION_ATTEMPT_LEFT,
                ChromePreferenceKeys.DOWNLOAD_FOREGROUND_SERVICE_OBSERVERS,
                ChromePreferenceKeys.DOWNLOAD_IS_DOWNLOAD_HOME_ENABLED,
                ChromePreferenceKeys.DOWNLOAD_NEXT_DOWNLOAD_NOTIFICATION_ID,
                ChromePreferenceKeys.DOWNLOAD_PENDING_DOWNLOAD_NOTIFICATIONS,
                ChromePreferenceKeys.DOWNLOAD_PENDING_OMA_DOWNLOADS,
                ChromePreferenceKeys.DOWNLOAD_UMA_ENTRY,
                ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED,
                ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE,
                ChromePreferenceKeys.FIRST_RUN_FLOW_SIGNIN_SETUP,
                ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
                ChromePreferenceKeys.FIRST_RUN_SKIP_WELCOME_PAGE,
                ChromePreferenceKeys.FONT_USER_FONT_SCALE_FACTOR,
                ChromePreferenceKeys.FONT_USER_SET_FORCE_ENABLE_ZOOM,
                ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO,
                ChromePreferenceKeys.HOMEPAGE_ENABLED,
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI,
                ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED,
                ChromePreferenceKeys.LATEST_UNSUPPORTED_VERSION,
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH,
                ChromePreferenceKeys.LOCALE_MANAGER_MISSING_TIMEZONES,
                ChromePreferenceKeys.LOCALE_MANAGER_PARTNER_PROMO_KEYWORD_SELECTED,
                ChromePreferenceKeys.LOCALE_MANAGER_PROMO_SHOWN,
                ChromePreferenceKeys.LOCALE_MANAGER_PROMO_V2_CHECKED,
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_WIDGET_PRESENT_FIRST_START,
                ChromePreferenceKeys.LOCALE_MANAGER_SHOULD_REPING_RLZ_FOR_SEARCH_PROMO,
                ChromePreferenceKeys.LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE,
                ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS,
                ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT,
                ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP,
                ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION,
                ChromePreferenceKeys.NOTIFICATIONS_LAST_SHOWN_NOTIFICATION_TYPE,
                ChromePreferenceKeys.NOTIFICATIONS_NEXT_TRIGGER,
                ChromePreferenceKeys.NTP_SNIPPETS_IS_SCHEDULED,
                ChromePreferenceKeys.OFFLINE_AUTO_FETCH_SHOWING_IN_PROGRESS,
                ChromePreferenceKeys.OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS,
                ChromePreferenceKeys.OFFLINE_INDICATOR_V2_ENABLED,
                ChromePreferenceKeys.PAYMENTS_CHECK_SAVE_CARD_TO_DEVICE,
                ChromePreferenceKeys.PAYMENTS_PAYMENT_COMPLETE_ONCE,
                ChromePreferenceKeys.PRIVACY_IN_SAMPLE_FOR_METRICS,
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING,
                ChromePreferenceKeys.PROFILES_BOOT_TIMESTAMP,
                ChromePreferenceKeys.PROMOS_SKIPPED_ON_FIRST_START,
                ChromePreferenceKeys.RLZ_NOTIFIED,
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE,
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION,
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP,
                ChromePreferenceKeys.SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE,
                ChromePreferenceKeys.SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE,
                ChromePreferenceKeys.SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE,
                ChromePreferenceKeys.SEARCH_WIDGET_NUM_CONSECUTIVE_CRASHES,
                ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME,
                ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_URL,
                ChromePreferenceKeys.SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS,
                ChromePreferenceKeys.SEND_TAB_TO_SELF_NEXT_NOTIFICATION_ID,
                ChromePreferenceKeys.SETTINGS_DEVELOPER_ENABLED,
                ChromePreferenceKeys.SETTINGS_DEVELOPER_TRACING_CATEGORIES,
                ChromePreferenceKeys.SETTINGS_DEVELOPER_TRACING_MODE,
                ChromePreferenceKeys.SETTINGS_PRIVACY_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN,
                ChromePreferenceKeys.SETTINGS_WEBSITE_FAILED_BUILD_VERSION,
                ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT,
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES,
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION,
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED,
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START,
                ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED,
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED,
                ChromePreferenceKeys.SIGNIN_LEGACY_PRIMARY_ACCOUNT_EMAIL,
                ChromePreferenceKeys.SNAPSHOT_DATABASE_REMOVED,
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME,
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF,
                ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID,
                ChromePreferenceKeys.TABMODEL_HAS_COMPUTED_MAX_ID,
                ChromePreferenceKeys.TABMODEL_HAS_RUN_FILE_MIGRATION,
                ChromePreferenceKeys.TABMODEL_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION,
                ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID,
                ChromePreferenceKeys.TOS_ACKED_ACCOUNTS,
                ChromePreferenceKeys.TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA,
                ChromePreferenceKeys.TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL,
                ChromePreferenceKeys.TWA_DISCLOSURE_ACCEPTED_PACKAGES,
                ChromePreferenceKeys.UI_THEME_SETTING,
                ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS,
                ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE);
    }

    static List<KeyPrefix> getPrefixesInUse() {
        return Arrays.asList(
                ChromePreferenceKeys.CUSTOM_TABS_DEX_LAST_UPDATE_TIME_PREF_PREFIX,
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT,
                ChromePreferenceKeys.PAYMENTS_PAYMENT_INSTRUMENT_USE_DATE,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_URL_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_GROUP_ID_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_STARRED_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX,
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX);
    }
}
