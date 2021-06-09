// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.annotations.CheckDiscard;

import java.util.Arrays;
import java.util.List;

/**
 * Contains String and {@link KeyPrefix} constants with the SharedPreferences keys used by Chrome.
 *
 * All Chrome layer SharedPreferences keys should be declared in this class.
 *
 * To add a new key:
 * 1. Declare it as a String constant in this class. Its value should follow the format
 *    "Chrome.[Feature].[Key]" and the constants names should be in alphabetical order.
 * 2. Add it to {@link #getKeysInUse()}.
 *
 * To deprecate a key that is not used anymore:
 * 1. Add its constant value to {@link DeprecatedChromePreferenceKeys#getKeysForTesting()}, in
 * alphabetical order by value.
 * 2. Remove the key from {@link #getKeysInUse()} or {@link
 * LegacyChromePreferenceKeys#getKeysInUse()}.
 * 3. Delete the constant.
 *
 * To add a new KeyPrefix:
 * 1. Declare it as a KeyPrefix constant in this class. Its value should follow the format
 *    "Chrome.[Feature].[KeyPrefix].*" and the constants names should be in alphabetical order.
 * 2. Add PREFIX_CONSTANT.pattern() to {@link #getKeysInUse()}}.
 *
 * To deprecate a KeyPrefix that is not used anymore:
 * 1. Add its String value to {@link DeprecatedChromePreferenceKeys#getPrefixesForTesting()},
 * including the ".*", in alphabetical order by value.
 * 2. Remove it from {@link #getKeysInUse()} or {@link
 * LegacyChromePreferenceKeys#getPrefixesInUse()}.
 * 3. Delete the KeyPrefix constant.
 *
 * Tests in ChromePreferenceKeysTest and checks in {@link ChromePreferenceKeyChecker} ensure the
 * sanity of this file.
 */
public final class ChromePreferenceKeys {
    /**
     * Whether the simplified tab switcher is enabled when accessibility mode is enabled. Keep in
     * sync with accessibility_preferences.xml.
     * Default value is true.
     */
    public static final String ACCESSIBILITY_TAB_SWITCHER = "accessibility_tab_switcher";

    public static final String ACCOUNT_PICKER_BOTTOM_SHEET_SHOWN_COUNT =
            "Chrome.AccountPickerBottomSheet.ShownCount";

    public static final String ACCOUNT_PICKER_BOTTOM_SHEET_ACTIVE_DISMISSAL_COUNT =
            "Chrome.AccountPickerBottomSheet.ConsecutiveActiveDismissalCount";

    /** The language code to override application language with. */
    public static final String APPLICATION_OVERRIDE_LANGUAGE =
            "Chrome.Language.ApplicationOverrideLanguage";

    /**
     * The last known state of the active tab that can take any value from
     * {@link TabPersistentStore#ActiveTabState}, recorded when TabModelSelector is serialized. This
     * pref is recorded because we delay the first draw only if we're going to show the NTP, and the
     * tab state isn't available when we need to make a decision
     * (ChromeTabbedActivity#performPostInflationStartup).
     */
    public static final String APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE =
            "Chrome.AppLaunch.LastKnownActiveTabState";

    /**
     * Whether the default search engine had a logo when #onStop was called. This is used with
     * |Chrome.AppLaunch.LastKnownActiveTabState| to predict if we are going to show NTP with a
     * logo on startup.
     */
    public static final String APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO =
            "Chrome.AppLaunch.SearchEngineHadLogo";

    public static final String APP_LOCALE = "locale";

    /** Assistant voice search keys. */
    public static final String ASSISTANT_VOICE_SEARCH_ENABLED = "Chrome.Assistant.Enabled";

    /** Whether Autofill Assistant is enabled */
    public static final String AUTOFILL_ASSISTANT_ENABLED = "autofill_assistant_switch";
    /** Whether the Autofill Assistant onboarding has been accepted. */
    public static final String AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED =
            "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED";
    /** Whether the user has seen a lite-script before or is a first-time user. */
    public static final String AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER =
            "Chrome.AutofillAssistant.LiteScriptFirstTimeUser";
    /** The number of times a user has explicitly canceled a lite script. */
    public static final String AUTOFILL_ASSISTANT_NUMBER_OF_LITE_SCRIPTS_CANCELED =
            "Chrome.AutofillAssistant.NumberOfLiteScriptsCanceled";
    /** Whether proactive help is enabled. */
    public static final String AUTOFILL_ASSISTANT_PROACTIVE_HELP =
            "Chrome.AutofillAssistant.ProactiveHelp";
    /**
     * LEGACY preference indicating whether "do not show again" was checked in the autofill
     * assistant onboarding
     */
    public static final String AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN =
            "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN";

    public static final String BACKUP_FIRST_BACKUP_DONE = "first_backup_done";

    public static final String BOOKMARKS_LAST_MODIFIED_FOLDER_ID = "last_bookmark_folder_id";
    public static final String BOOKMARKS_LAST_USED_URL = "enhanced_bookmark_last_used_url";
    public static final String BOOKMARKS_LAST_USED_PARENT =
            "enhanced_bookmark_last_used_parent_folder";

    /**
     * Whether Chrome is set as the default browser.
     * Default value is false.
     */
    public static final String CHROME_DEFAULT_BROWSER = "applink.chrome_default_browser";

    /** Number of attempts that have been made to download a survey. */
    public static final KeyPrefix CHROME_SURVEY_DOWNLOAD_ATTEMPTS =
            new KeyPrefix("Chrome.Survey.DownloadAttempts.*");
    /**
     * Key prefix used to indicate the timestamps when the survey info bar is displayed for a
     * certain survey.
     */
    public static final KeyPrefix CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP =
            new KeyPrefix("Chrome.Survey.PromptDisplayedTimestamp.*");

    /** The URI of Chrome shared URI to Android system clibpoard. */
    public static final String CLIPBOARD_SHARED_URI = "Chrome.Clipboard.SharedUri";

    /** The timestamp of Chrome shared URI to Android system clibpoard. */
    public static final String CLIPBOARD_SHARED_URI_TIMESTAMP =
            "Chrome.Clipboard.SharedUriTimestamp";

    /**
     * Save the timestamp of the last time that chrome-managed commerce subscriptions are
     * initialized.
     */
    public static final String COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP =
            "Chrome.CommerceSubscriptions.ChromeManagedTimestamp";

    /**
     * Saves a counter of how many continuous feature sessions in which a user has dismissed
     * conditional tab strip.
     */
    public static final String CONDITIONAL_TAB_STRIP_CONTINUOUS_DISMISS_COUNTER =
            "Chrome.ConditionalTabStrip.ContinuousDismissCounter";

    /**
     * Saves the feature status of conditional tab strip.
     */
    public static final String CONDITIONAL_TAB_STRIP_FEATURE_STATUS =
            "Chrome.ConditionalTabStrip.FeatureStatus";

    /**
     * Saves the timestamp of the last time that conditional tab strip shows.
     */
    public static final String CONDITIONAL_TAB_STRIP_LAST_SHOWN_TIMESTAMP =
            "Chrome.ConditionalTabStrip.LastShownTimeStamp";

    /**
     * Saves whether a user has chosen to opt-out the conditional tab strip feature.
     */
    public static final String CONDITIONAL_TAB_STRIP_OPT_OUT = "Chrome.ConditionalTabStrip.OptOut";

    /**
     * Marks that the content suggestions surface has been shown.
     * Default value is false.
     */
    public static final String CONTENT_SUGGESTIONS_SHOWN = "content_suggestions_shown";

    /** An all-time counter of Contextual Search panel opens triggered by any gesture.*/
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT =
            "contextual_search_all_time_open_count";
    /** An all-time counter of taps that triggered the Contextual Search peeking panel. */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT =
            "contextual_search_all_time_tap_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT =
            "contextual_search_all_time_tap_quick_answer_count";
    public static final KeyPrefix CONTEXTUAL_SEARCH_CLICKS_WEEK_PREFIX =
            new KeyPrefix("contextual_search_clicks_week_*");
    public static final String CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER =
            "contextual_search_current_week_number";
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
    public static final KeyPrefix CONTEXTUAL_SEARCH_IMPRESSIONS_WEEK_PREFIX =
            new KeyPrefix("contextual_search_impressions_week_*");
    public static final String CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME =
            "contextual_search_last_animation_time";
    public static final String CONTEXTUAL_SEARCH_NEWEST_WEEK = "contextual_search_newest_week";
    public static final String CONTEXTUAL_SEARCH_OLDEST_WEEK = "contextual_search_oldest_week";
    /**
     * An encoded set of outcomes of user interaction with Contextual Search, stored as an int.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES =
            "contextual_search_previous_interaction_encoded_outcomes";
    /**
     * A user interaction event ID for interaction with Contextual Search, stored as a long.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID =
            "contextual_search_previous_interaction_event_id";
    /**
     * A timestamp indicating when we updated the user interaction with Contextual Search, stored
     * as a long, with resolution in days.
     */
    public static final String CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP =
            "contextual_search_previous_interaction_timestamp";
    /**
     * The number of times the Contextual Search panel was opened with the opt-in promo visible.
     */
    public static final String CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT =
            "contextual_search_promo_open_count";
    /**
     * The Quick Actions ignored count, i.e. phone numbers available but not dialed.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT =
            "contextual_search_quick_actions_ignored_count";
    /**
     * The Quick Actions taken count for Contextual Search, i.e. phone numbers dialed and similar
     * actions. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT =
            "contextual_search_quick_actions_taken_count";
    /**
     * The Quick Action impressions count for Contextual Search, i.e. actions presented in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT =
            "contextual_search_quick_action_impressions_count";
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
    public static final String CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT =
            "contextual_search_tap_triggered_promo_count";

    /**
     * Keys that indicates if an item in the context menu has been clicked or not.
     * Used to hide the "new" tag for the items after they are clicked.
     */
    public static final String CONTEXT_MENU_OPEN_IMAGE_IN_EPHEMERAL_TAB_CLICKED =
            "Chrome.Contextmenu.OpenImageInEphemeralTabClicked";
    public static final String CONTEXT_MENU_OPEN_IN_EPHEMERAL_TAB_CLICKED =
            "Chrome.Contextmenu.OpenInEphemeralTabClicked";

    /**
     * Key used to save the context menu item order for the "Open in new tab" item and
     * the "Open in new tab in group" item.
     */
    public static final String CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST =
            "Chrome.ContextMenu.OpenNewTabInGroupFirst";

    public static final String CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED =
            "Chrome.ContextMenu.SearchWithGoogleLensClicked";

    public static final String CONTEXT_MENU_SHOP_SIMILAR_PRODUCTS_CLICKED =
            "Chrome.ContextMenu.ShopSimilarProductsClicked";

    public static final String CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED =
            "Chrome.ContextMenu.ShopImageWithGoogleLensClicked";

    public static final String CONTEXT_MENU_SEARCH_SIMILAR_PRODUCTS_CLICKED =
            "Chrome.ContextMenu.SearchSimilarProductsClicked";

    public static final String CRASH_UPLOAD_FAILURE_BROWSER = "browser_crash_failure_upload";
    public static final String CRASH_UPLOAD_FAILURE_GPU = "gpu_crash_failure_upload";
    public static final String CRASH_UPLOAD_FAILURE_OTHER = "other_crash_failure_upload";
    public static final String CRASH_UPLOAD_FAILURE_RENDERER = "renderer_crash_failure_upload";
    public static final String CRASH_UPLOAD_SUCCESS_BROWSER = "browser_crash_success_upload";
    public static final String CRASH_UPLOAD_SUCCESS_GPU = "gpu_crash_success_upload";
    public static final String CRASH_UPLOAD_SUCCESS_OTHER = "other_crash_success_upload";
    public static final String CRASH_UPLOAD_SUCCESS_RENDERER = "renderer_crash_success_upload";

    public static final String CRYPTID_LAST_RENDER_TIMESTAMP = "Chrome.Cryptid.LastRenderTimestamp";

    public static final KeyPrefix CUSTOM_TABS_DEX_LAST_UPDATE_TIME_PREF_PREFIX =
            new KeyPrefix("pref_local_custom_tabs_module_dex_last_update_time_*");
    public static final String CUSTOM_TABS_LAST_URL = "pref_last_custom_tab_url";

    /**
     * Key used to save the time in milliseconds since epoch that the first run experience or second
     * run promo was shown.
     */
    public static final String DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_PROMO_TIME_MS =
            "displayed_data_reduction_promo_time_ms";
    /**
     * Key used to save the Chrome version the first run experience or second run promo was shown
     * in.
     */
    public static final String DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION =
            "displayed_data_reduction_promo_version";
    /**
     * Key used to save whether the first run experience or second run promo screen has been shown.
     */
    public static final String DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_RUN_PROMO =
            "displayed_data_reduction_promo";
    /**
     * Key used to save whether the infobar promo has been shown.
     */
    public static final String DATA_REDUCTION_DISPLAYED_INFOBAR_PROMO =
            "displayed_data_reduction_infobar_promo";
    /**
     * Key used to save the Chrome version the infobar promo was shown in.
     */
    public static final String DATA_REDUCTION_DISPLAYED_INFOBAR_PROMO_VERSION =
            "displayed_data_reduction_infobar_promo_version";
    /**
     * Key used to save the saved bytes when the milestone promo was last shown. This value is
     * initialized to the bytes saved for data saver users that had data saver turned on when this
     * pref was added. This prevents us from showing promo for savings that have already happened
     * for existing users.
     * Note: For historical reasons, this pref key is misnamed. This promotion used to be conveyed
     * in a snackbar but was moved to an IPH in M74.
     */
    public static final String DATA_REDUCTION_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES =
            "displayed_data_reduction_snackbar_promo_saved_bytes";

    // Visible for backup and restore
    public static final String DATA_REDUCTION_ENABLED = "BANDWIDTH_REDUCTION_PROXY_ENABLED";
    public static final String DATA_REDUCTION_FIRST_ENABLED_TIME =
            "BANDWIDTH_REDUCTION_FIRST_ENABLED_TIME";
    /**
     * Key used to save whether the user opted out of the data reduction proxy in the FRE promo.
     */
    public static final String DATA_REDUCTION_FRE_PROMO_OPT_OUT = "fre_promo_opt_out";
    /**
     * Key used to save the date on which the site breakdown should be shown. If the user has
     * historical data saver stats, the site breakdown cannot be shown for MAXIMUM_DAYS_IN_CHART.
     */
    public static final String DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE =
            "data_reduction_site_breakdown_allowed_date";

    /**
     * Keys used to save whether it is ready to promo.
     */
    public static final String DEFAULT_BROWSER_PROMO_SESSION_COUNT =
            "Chrome.DefaultBrowserPromo.SessionCount";
    public static final String DEFAULT_BROWSER_PROMO_PROMOED_COUNT =
            "Chrome.DefaultBrowserPromo.PromoedCount";
    public static final String DEFAULT_BROWSER_PROMO_LAST_DEFAULT_STATE =
            "Chrome.DefaultBrowserPromo.LastDefaultState";
    public static final String DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME =
            "Chrome.DefaultBrowserPromo.LastPromoTime";
    public static final String DEFAULT_BROWSER_PROMO_PROMOED_BY_SYSTEM_SETTINGS =
            "Chrome.DefaultBrowserPromo.PromoedBySystemSettings";

    public static final String DOWNLOAD_AUTO_RESUMPTION_ATTEMPT_LEFT = "ResumptionAttemptLeft";
    public static final String DOWNLOAD_FOREGROUND_SERVICE_OBSERVERS = "ForegroundServiceObservers";
    public static final String DOWNLOAD_IS_DOWNLOAD_HOME_ENABLED =
            "org.chromium.chrome.browser.download.IS_DOWNLOAD_HOME_ENABLED";
    public static final String DOWNLOAD_NEXT_DOWNLOAD_NOTIFICATION_ID =
            "NextDownloadNotificationId";
    public static final String DOWNLOAD_PENDING_DOWNLOAD_NOTIFICATIONS =
            "PendingDownloadNotifications";
    public static final String DOWNLOAD_PENDING_OMA_DOWNLOADS = "PendingOMADownloads";
    public static final String DOWNLOAD_UMA_ENTRY = "DownloadUmaEntry";

    /**
     * Indicates whether or not there are prefetched content in chrome that can be viewed offline.
     */
    public static final String EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS =
            "Chrome.NTPExploreOfflineCard.HasExploreOfflineContent";

    /**
     * The Feed articles visibility. This value is used as a pre-native cache and should be kept
     * consistent with {@link Pref.ARTICLES_LIST_VISIBLE}.
     */
    public static final String FEED_ARTICLES_LIST_VISIBLE = "Chrome.Feed.ArticlesListVisible";
    public static final String FEED_PLACEHOLDER_DENSE = "Chrome.Feed.PlaceholderIsDense";

    public static final String FIRST_RUN_CACHED_TOS_ACCEPTED = "first_run_tos_accepted";
    public static final String FIRST_RUN_FLOW_COMPLETE = "first_run_flow";
    public static final String FIRST_RUN_FLOW_SIGNIN_ACCOUNT_NAME = "first_run_signin_account_name";
    public static final String FIRST_RUN_FLOW_SIGNIN_COMPLETE = "first_run_signin_complete";
    // Needed by ChromeBackupAgent
    public static final String FIRST_RUN_FLOW_SIGNIN_SETUP = "first_run_signin_setup";
    public static final String FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE = "lightweight_first_run_flow";
    public static final String FIRST_RUN_SKIP_WELCOME_PAGE = "skip_welcome_page";
    public static final String FIRST_RUN_SKIPPED_BY_POLICY = "Chrome.FirstRun.SkippedByPolicy";

    /**
     * Cached feature flags generated by CachedFeatureFlags use this prefix.
     */
    public static final KeyPrefix FLAGS_CACHED = new KeyPrefix("Chrome.Flags.CachedFlag.*");

    /**
     * Cached field trial parameters generated by CachedFeatureFlags use this prefix.
     */
    public static final KeyPrefix FLAGS_FIELD_TRIAL_PARAM_CACHED =
            new KeyPrefix("Chrome.Flags.FieldTrialParamCached.*");

    /**
     * Whether or not the adaptive toolbar is enabled.
     * Default value is true.
     */
    public static final String FLAGS_CACHED_ADAPTIVE_TOOLBAR_ENABLED = "adaptive_toolbar_enabled";

    /**
     * Whether or not command line on non-rooted devices is enabled.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_COMMAND_LINE_ON_NON_ROOTED_ENABLED =
            "command_line_on_non_rooted_enabled";
    /**
     * Whether or not the download auto-resumption is enabled in native.
     * Default value is true.
     */
    public static final String FLAGS_CACHED_DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE =
            "download_auto_resumption_in_native";
    /**
     * Whether or not the grid tab switcher is enabled.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_GRID_TAB_SWITCHER_ENABLED = "grid_tab_switcher_enabled";
    /**
     * Key to cache whether immersive ui mode is enabled.
     */
    public static final String FLAGS_CACHED_IMMERSIVE_UI_MODE_ENABLED = "immersive_ui_mode_enabled";
    /**
     * Whether warming up network service is enabled.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED =
            "network_service_warm_up_enabled";
    /**
     * Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task prioritization
     * experiment is enabled). Default value is true.
     */
    public static final String FLAGS_CACHED_PRIORITIZE_BOOTSTRAP_TASKS =
            "prioritize_bootstrap_tasks";
    /**
     * Key for whether PrefetchBackgroundTask should load native in service manager only mode.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH =
            "service_manager_for_background_prefetch";
    /**
     * Key for whether DownloadResumptionBackgroundTask should load native in service manager only
     * mode.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION =
            "service_manager_for_download_resumption";
    /**
     * Whether or not the start surface is enabled.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_START_SURFACE_ENABLED = "start_surface_enabled";

    /**
     * Key to cache whether SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT is enabled.
     */
    public static final String FLAGS_CACHED_SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT =
            "swap_pixel_format_to_fix_convert_from_translucent";
    /**
     * Whether or not the tab group is enabled.
     * Default value is false.
     */
    public static final String FLAGS_CACHED_TAB_GROUPS_ANDROID_ENABLED =
            "tab_group_android_enabled";

    /** See CachedFeatureFlags.getLastCachedMinimalBrowserFlagsTimeMillis(). */
    public static final String FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS =
            "Chrome.Flags.LastCachedMinimalBrowserFlagsTimeMillis";

    public static final String FONT_USER_FONT_SCALE_FACTOR = "user_font_scale_factor";
    public static final String FONT_USER_SET_FORCE_ENABLE_ZOOM = "user_set_force_enable_zoom";

    public static final String HISTORY_SHOW_HISTORY_INFO = "history_home_show_info";

    /** Keys used to save settings related to homepage. */
    public static final String HOMEPAGE_CUSTOM_URI = "homepage_custom_uri";
    public static final String HOMEPAGE_ENABLED = "homepage";
    public static final String HOMEPAGE_USE_CHROME_NTP = "Chrome.Homepage.UseNTP";
    public static final String HOMEPAGE_USE_DEFAULT_URI = "homepage_partner_enabled";

    /**
     * Key used to save homepage location set by enterprise policy
     */
    public static final String HOMEPAGE_LOCATION_POLICY = "Chrome.Policy.HomepageLocation";

    /**
     * Used for get image descriptions feature, track "Just once"/"Don't ask again" choice.
     */
    public static final String IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT =
            "Chrome.ImageDescriptions.JustOnceCount";
    public static final String IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN =
            "Chrome.ImageDescriptions.DontAskAgain";

    public static final String INCOGNITO_SHORTCUT_ADDED = "incognito-shortcut-added";

    /**
     * The last version the dex compile workaround ran on. See SplitChromeApplication for more
     * details.
     */
    public static final String ISOLATED_SPLITS_DEX_COMPILE_VERSION =
            "Chrome.IsolatedSplits.VersionCode";

    /**
     * When the user is shown a badge that the current Android OS version is unsupported, and they
     * tap it to display the menu (which has additional information), we store the current version
     * of Chrome to this preference to ensure we only show the badge once. The value is cleared
     * if the Chrome version later changes.
     */
    public static final String LATEST_UNSUPPORTED_VERSION = "android_os_unsupported_chrome_version";

    /**
     * The previous browser process PID, updated when crash reporting is initialized.
     */
    public static final String LAST_SESSION_BROWSER_PID =
            "Chrome.CrashReporting.LastSessionBrowserPid";

    /**
     * The application state last recorded by browser in previous session, updated when crash
     * reporting is initialized and when current application state changes henceforth. If read after
     * crash reporting is initialized, then the value would hold current session state.
     */
    public static final String LAST_SESSION_APPLICATION_STATE =
            "Chrome.CrashReporting.LastSessionApplicationState";

    public static final String LOCALE_MANAGER_AUTO_SWITCH = "LocaleManager_PREF_AUTO_SWITCH";
    public static final String LOCALE_MANAGER_PROMO_SHOWN = "LocaleManager_PREF_PROMO_SHOWN";
    public static final String LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE =
            "com.android.chrome.SEARCH_ENGINE_PROMO_SHOWN";
    public static final String LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE =
            "LocaleManager_WAS_IN_SPECIAL_LOCALE";

    public static final String MEDIA_WEBRTC_NOTIFICATION_IDS = "WebRTCNotificationIds";

    public static final String METRICS_MAIN_INTENT_LAUNCH_COUNT = "MainIntent.LaunchCount";
    public static final String METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP = "MainIntent.LaunchTimestamp";

    public static final String NOTIFICATIONS_CHANNELS_VERSION = "channels_version_key";
    public static final String NOTIFICATIONS_LAST_SHOWN_NOTIFICATION_TYPE =
            "NotificationUmaTracker.LastShownNotificationType";
    public static final String NOTIFICATIONS_NEXT_TRIGGER =
            "notification_trigger_scheduler.next_trigger";

    public static final String NTP_SNIPPETS_IS_SCHEDULED = "ntp_snippets.is_scheduled";

    // Name of an application preference variable used to track whether or not the in-progress
    // notification is being shown. This is an alternative to
    // NotificationManager.getActiveNotifications, which isn't available prior to API level 23.
    public static final String OFFLINE_AUTO_FETCH_SHOWING_IN_PROGRESS =
            "offline_auto_fetch_showing_in_progress";
    // The application preference variable which is set to the NotificationAction that triggered the
    // cancellation, when a cancellation is requested by the user.
    public static final String OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS =
            "offline_auto_fetch_user_cancel_action_in_progress";

    /**
     * Key to cache whether offline indicator v2 (persistent offline indicator) is enabled.
     */
    public static final String OFFLINE_INDICATOR_V2_ENABLED = "offline_indicator_v2_enabled";

    /**
     * The wall time of when the offline indicator was shown in milliseconds.
     */
    public static final String OFFLINE_INDICATOR_V2_WALL_TIME_SHOWN_MS =
            "Chrome.OfflineIndicatorV2.WallTimeShownMs";

    /**
     * Used to divide the duration that the offline indicator is shown between when Chrome is in the
     * foreground and the background.
     */
    public static final String OFFLINE_INDICATOR_V2_LAST_UPDATE_WALL_TIME_MS =
            "Chrome.OfflineIndicatorV2.LastUpdateWallTimeMs";
    public static final String OFFLINE_INDICATOR_V2_TIME_IN_FOREGROUND_MS =
            "Chrome.OfflineIndicatorV2.TimeInForegroundMs";
    public static final String OFFLINE_INDICATOR_V2_TIME_IN_BACKGROUND_MS =
            "Chrome.OfflineIndicatorV2.TimeInBackgroundMs";
    public static final String OFFLINE_INDICATOR_V2_FIRST_TIME_IN_FOREGROUND_MS =
            "Chrome.OfflineIndicatorV2.FirstTimeInForegroundMs";
    public static final String OFFLINE_INDICATOR_V2_NUM_TIMES_BACKGROUNDED =
            "Chrome.OfflineIndicatorV2.NumTimesBackgrounded";

    /**
     * The measurement interval (in minutes) used to schedule the currently running
     * OfflineMeasureBackgroundTask. This value is zero if the OfflineMeasureBackgroundTask is not
     * currently running.
     */
    public static final String OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES =
            "Chrome.OfflineMeasurements.CurrentTaskMeasurementIntervalInMinutes";

    /** Time of the last OfflineMeasurementsBackgroundTask check. */
    public static final String OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS =
            "Chrome.OfflineMeasurements.LastCheckMillis";

    /** Parameters that control the HTTP probe of the Offline Measurements Background task */
    public static final String OFFLINE_MEASUREMENTS_USER_AGENT_STRING =
            "Chrome.OfflineMeasurements.UserAgentString";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_URL =
            "Chrome.OfflineMeasurements.HttpProbeUrl";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS =
            "Chrome.OfflineMeasurements.HttpProbeTimeoutMs";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD =
            "Chrome.OfflineMeasurements.HttpProbeMethod";

    /**
     * Comma separated list of time between OfflineMeasurementsBackgroundTask checks. When possible
     * these values will be recorded to UMA.
     */
    public static final String OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST =
            "Chrome.OfflineMeasurements.TimeBetweenChecksMillisList";

    /**
     * Comma separated list of the results of HTTP probes from OfflineMeasurementsBackgroundTask.
     * When possible values will be recorded to UMA then cleared.
     */
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULTS_LIST =
            "Chrome.OfflineMeasurements.HttpProbeResultsList";

    /**
     * Comma separated list of the airplane mode and roaming state from the
     * OfflineMeasurementsBackgroundTask. When possible, values will be recorded to UMA then
     * cleared.
     */
    public static final String OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED_LIST =
            "Chrome.OfflineMeasurements.IsAirplaneModeEnabledList";
    public static final String OFFLINE_MEASUREMENTS_IS_ROAMING_LIST =
            "Chrome.OfflineMeasurements.IsRoaming";

    /** The shared preference for the 'save card to device' checkbox status. */
    public static final String PAYMENTS_CHECK_SAVE_CARD_TO_DEVICE = "check_save_card_to_device";

    /** Prefix of the preferences to persist use count of the payment instruments. */
    public static final KeyPrefix PAYMENTS_PAYMENT_INSTRUMENT_USE_COUNT =
            new KeyPrefix("payment_instrument_use_count_*");

    /** Prefix of the preferences to persist last use date of the payment instruments. */
    public static final KeyPrefix PAYMENTS_PAYMENT_INSTRUMENT_USE_DATE =
            new KeyPrefix("payment_instrument_use_date_*");

    /** Preference to indicate whether payment request has been completed successfully once.*/
    public static final String PAYMENTS_PAYMENT_COMPLETE_ONCE = "payment_complete_once";

    /**
     * Indicates whether or not there is any persistent (i.e. non-transient) content in chrome that
     * can be viewed offline.
     */
    public static final String PERSISTENT_OFFLINE_CONTENT_AVAILABILITY_STATUS =
            "Chrome.OfflineIndicatorV2.HasPersistentOfflineContent";

    public static final String PREFETCH_HAS_NEW_PAGES = "prefetch_notification_has_new_pages";
    public static final String PREFETCH_IGNORED_NOTIFICATION_COUNTER =
            "prefetch_notification_ignored_counter";
    public static final String PREFETCH_NOTIFICATION_ENABLED = "prefetch_notification_enabled";
    public static final String PREFETCH_NOTIFICATION_TIME = "prefetch_notification_shown_time";
    public static final String PREFETCH_OFFLINE_COUNTER = "prefetch_notification_offline_counter";

    /**
     * Whether the PriceAlertsMessageCard is enabled.
     */
    public static final String PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD =
            "Chrome.PriceTracking.PriceAlerts";
    /**
     * Indicates how many times the PriceAlertsMessageCard has shown in the tab switcher.
     */
    public static final String PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT =
            "Chrome.PriceTracking.PriceAlertsShowCount";
    /**
     * Whether the PriceWelcomeMessageCard is enabled.
     */
    public static final String PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD =
            "Chrome.PriceTracking.PriceWelcome";
    /**
     * Indicates how many times the PriceWelcomeMessageCard has shown in the tab switcher.
     */
    public static final String PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT =
            "Chrome.PriceTracking.PriceWelcomeShowCount";
    /**
     * Whether users turn on the feature track prices on tabs.
     */
    public static final String PRICE_TRACKING_TRACK_PRICES_ON_TABS =
            "Chrome.PriceTracking.TrackPricesOnTabs";

    public static final String PRIVACY_METRICS_REPORTING = "metrics_reporting";
    public static final String PRIVACY_METRICS_IN_SAMPLE = "in_metrics_sample";

    public static final String PROFILES_BOOT_TIMESTAMP =
            "com.google.android.apps.chrome.ChromeMobileApplication.BOOT_TIMESTAMP";

    /**
     * Key prefix for base promo component. Used in {@link
     * org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator} to store related state
     * or statistics.
     */
    public static final KeyPrefix PROMO_IS_DISMISSED =
            new KeyPrefix("Chrome.PromoCard.IsDismissed.*");
    public static final KeyPrefix PROMO_TIMES_SEEN = new KeyPrefix("Chrome.PromoCard.TimesSeen.*");

    /**
     * Whether the promotion for data reduction has been skipped on first invocation.
     * Default value is false.
     */
    public static final String PROMOS_SKIPPED_ON_FIRST_START = "promos_skipped_on_first_start";

    /**
     * The next timestamp to decide whether to show query tiles.
     */
    public static final String QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS =
            "Chrome.Querytiles.NextDecisionTime";

    /**
     * Recent number of MV tile clicks, before the decision time.
     */
    public static final String QUERY_TILES_NUM_RECENT_MV_TILE_CLICKS =
            "Chrome.Querytiles.RecentMvClicks";

    /**
     * Recent number of query tile clicks, before the decision time.
     */
    public static final String QUERY_TILES_NUM_RECENT_QUERY_TILE_CLICKS =
            "Chrome.Querytiles.RecentQueryTileClicks";

    /**
     * Whether query tiles should be shown on NTP. Default value is false.
     */
    public static final String QUERY_TILES_SHOW_ON_NTP = "Chrome.Querytiles.ShowOnNTP";

    /**
     * Contains a trial group that was used to determine whether the reached code profiler should be
     * enabled.
     */
    public static final String REACHED_CODE_PROFILER_GROUP = "reached_code_profiler_group";

    public static final String RLZ_NOTIFIED = "rlz_first_search_notified";

    /** Key used to store the default Search Engine Type before choice is presented. */
    public static final String SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE =
            "search_engine_choice_default_type_before";
    /** Key used to store the version of Chrome in which the choice was presented. */
    public static final String SEARCH_ENGINE_CHOICE_PRESENTED_VERSION =
            "search_engine_choice_presented_version";
    /** Key used to store the date of when search engine choice was requested. */
    public static final String SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP =
            "search_engine_choice_requested_timestamp";

    public static final String SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE =
            "org.chromium.chrome.browser.searchwidget.IS_VOICE_SEARCH_AVAILABLE";
    public static final String SEARCH_WIDGET_NUM_CONSECUTIVE_CRASHES =
            "org.chromium.chrome.browser.searchwidget.NUM_CONSECUTIVE_CRASHES";
    public static final String SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME =
            "org.chromium.chrome.browser.searchwidget.SEARCH_ENGINE_SHORTNAME";

    // Tracks which GUIDs there is an active notification for.
    public static final String SEND_TAB_TO_SELF_ACTIVE_NOTIFICATIONS =
            "send_tab_to_self.notification.active";
    public static final String SEND_TAB_TO_SELF_NEXT_NOTIFICATION_ID =
            "send_tab_to_self.notification.next_id";

    public static final String SETTINGS_DEVELOPER_ENABLED = "developer";
    public static final String SETTINGS_DEVELOPER_TRACING_CATEGORIES = "tracing_categories";
    public static final String SETTINGS_DEVELOPER_TRACING_MODE = "tracing_mode";

    public static final String SETTINGS_PRIVACY_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN =
            "org.chromium.chrome.browser.settings.privacy."
            + "PREF_OTHER_FORMS_OF_HISTORY_DIALOG_SHOWN";

    /** Stores the timestamp of the last performed Safety check. */
    public static final String SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP =
            "Chrome.SafetyCheck.LastRunTimestamp";

    /** Stores the number of times the user has performed Safety check. */
    public static final String SETTINGS_SAFETY_CHECK_RUN_COUNTER = "Chrome.SafetyCheck.RunCounter";

    public static final String SETTINGS_WEBSITE_FAILED_BUILD_VERSION =
            "ManagedSpace.FailedBuildVersion";

    public static final String SHARING_LAST_SHARED_CLASS_NAME = "last_shared_class_name";
    public static final String SHARING_LAST_SHARED_PACKAGE_NAME = "last_shared_package_name";

    public static final String SIGNIN_ACCOUNTS_CHANGED = "prefs_sync_accounts_changed";

    /**
     * Holds the new account's name if the currently signed in account has been renamed.
     */
    public static final String SIGNIN_ACCOUNT_RENAMED = "prefs_sync_account_renamed";

    /**
     * Holds the last read index of all the account changed events of the current signed in account.
     */
    public static final String SIGNIN_ACCOUNT_RENAME_EVENT_INDEX =
            "prefs_sync_account_rename_event_index";

    /**
     * Generic signin and sync promo preferences.
     */
    public static final String SIGNIN_AND_SYNC_PROMO_SHOW_COUNT =
            "enhanced_bookmark_signin_promo_show_count";

    public static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS =
            "signin_promo_impressions_count_bookmarks";
    public static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP =
            "Chrome.SigninPromo.NTPImpressions";
    public static final String SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS =
            "signin_promo_impressions_count_settings";
    public static final String SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES =
            "signin_promo_last_shown_account_names";
    public static final String SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION =
            "signin_promo_last_shown_chrome_version";
    /**
     * Whether the user dismissed the personalized sign in promo from the new tab page.
     * Default value is false.
     */
    public static final String SIGNIN_PROMO_NTP_PROMO_DISMISSED =
            "ntp.personalized_signin_promo_dismissed";
    public static final String SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START =
            "ntp.signin_promo_suppression_period_start";
    /**
     * Personalized signin promo preference.
     */
    public static final String SIGNIN_PROMO_PERSONALIZED_DECLINED =
            "signin_promo_bookmarks_declined";
    /**
     * Whether the user dismissed the personalized sign in promo from the Settings.
     * Default value is false.
     */
    public static final String SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED =
            "settings_personalized_signin_promo_dismissed";

    // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that uses
    //                                  the sync account before the native is loaded.
    public static final String SIGNIN_LEGACY_SYNC_ACCOUNT_EMAIL = "google.services.username";

    public static final String SNAPSHOT_DATABASE_REMOVED = "snapshot_database_removed";

    public static final String SURVEY_DATE_LAST_ROLLED = "last_rolled_for_chrome_survey_key";

    public static final String TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF =
            "ChromeTabbedActivity.BackgroundTimeMs";

    public static final String TABMODEL_ACTIVE_TAB_ID =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.ACTIVE_TAB_ID";
    public static final String TABMODEL_HAS_COMPUTED_MAX_ID =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_COMPUTED_MAX_ID";
    public static final String TABMODEL_HAS_RUN_FILE_MIGRATION =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_RUN_FILE_MIGRATION";
    public static final String TABMODEL_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore."
            + "HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION";

    public static final String TAB_ID_MANAGER_NEXT_ID =
            "org.chromium.chrome.browser.tab.TabIdManager.NEXT_ID";

    public static final String TOS_ACKED_ACCOUNTS = "ToS acknowledged accounts";

    /**
     * Keys for deferred recording of the outcomes of showing the clear data dialog after
     * Trusted Web Activity client apps are uninstalled or have their data cleared.
     */
    public static final String TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_CLEAR_DATA =
            "twa_dialog_number_of_dismissals_on_clear_data";
    public static final String TWA_DIALOG_NUMBER_OF_DISMISSALS_ON_UNINSTALL =
            "twa_dialog_number_of_dismissals_on_uninstall";
    public static final String TWA_DISCLOSURE_ACCEPTED_PACKAGES =
            "trusted_web_activity_disclosure_accepted_packages";
    public static final String TWA_DISCLOSURE_SEEN_PACKAGES =
            "Chrome.TrustedWebActivities.DisclosureAcceptedPackages";

    /**
     * Whether or not darken websites is enabled.
     * Default value is false.
     */
    public static final String UI_THEME_DARKEN_WEBSITES_ENABLED = "darken_websites_enabled";
    /**
     * The current theme setting in the user settings.
     * Default value is -1. Use NightModeUtils#getThemeSetting() to retrieve current setting or
     * default theme.
     */
    public static final String UI_THEME_SETTING = "ui_theme_setting";

    public static final String VERIFIED_DIGITAL_ASSET_LINKS = "verified_digital_asset_links";

    public static final String VIDEO_TUTORIALS_SHARE_URL_SET = "Chrome.VideoTutorials.ShareUrls";
    public static final String VR_EXIT_TO_2D_COUNT = "VR_EXIT_TO_2D_COUNT";
    public static final String VR_FEEDBACK_OPT_OUT = "VR_FEEDBACK_OPT_OUT";

    /**
     * Whether VR assets component should be registered on startup.
     * Default value is false.
     */
    public static final String VR_SHOULD_REGISTER_ASSETS_COMPONENT_ON_STARTUP =
            "should_register_vr_assets_component_on_startup";

    /**
     * Name of the shared preference for the version number of the dynamically loaded dex.
     */
    public static final String WEBAPK_EXTRACTED_DEX_VERSION =
            "org.chromium.chrome.browser.webapps.extracted_dex_version";

    /**
     * Name of the shared preference for the Android OS version at the time that the dex was last
     * extracted from Chrome's assets and optimized.
     */
    public static final String WEBAPK_LAST_SDK_VERSION =
            "org.chromium.chrome.browser.webapps.last_sdk_version";

    /** Key for deferred recording of list of uninstalled WebAPK packages. */
    public static final String WEBAPK_UNINSTALLED_PACKAGES = "webapk_uninstalled_packages";

    /** Cached Suggestions and Suggestion Headers. */
    public static final String KEY_ZERO_SUGGEST_LIST_SIZE = "zero_suggest_list_size";
    public static final KeyPrefix KEY_ZERO_SUGGEST_URL_PREFIX = new KeyPrefix("zero_suggest_url*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX =
            new KeyPrefix("zero_suggest_display_text*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX =
            new KeyPrefix("zero_suggest_description*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX =
            new KeyPrefix("zero_suggest_native_type*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX =
            new KeyPrefix("zero_suggest_native_subtypes*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX =
            new KeyPrefix("zero_suggest_is_search*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX =
            new KeyPrefix("zero_suggest_answer_text*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_GROUP_ID_PREFIX =
            new KeyPrefix("zero_suggest_group_id*");
    @Deprecated
    public static final KeyPrefix KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX =
            new KeyPrefix("zero_suggest_is_deletable*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_IS_STARRED_PREFIX =
            new KeyPrefix("zero_suggest_is_starred*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX =
            new KeyPrefix("zero_suggest_post_content_type*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX =
            new KeyPrefix("zero_suggest_post_content_data*");
    public static final String KEY_ZERO_SUGGEST_HEADER_LIST_SIZE = "zero_suggest_header_list_size";
    public static final KeyPrefix KEY_ZERO_SUGGEST_HEADER_GROUP_ID_PREFIX =
            new KeyPrefix("zero_suggest_header_group_id*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_HEADER_GROUP_TITLE_PREFIX =
            new KeyPrefix("zero_suggest_header_group_title*");
    public static final KeyPrefix KEY_ZERO_SUGGEST_HEADER_GROUP_COLLAPSED_BY_DEFAULT_PREFIX =
            new KeyPrefix("zero_suggest_header_group_collapsed_by_default*");

    /**
     * These values are currently used as SharedPreferences keys, along with the keys in
     * {@link LegacyChromePreferenceKeys#getKeysInUse()}. Add new SharedPreferences keys
     * here.
     *
     * @return The list of [keys in use] conforming to the format.
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> getKeysInUse() {
        // clang-format off
        return Arrays.asList(
                ACCOUNT_PICKER_BOTTOM_SHEET_SHOWN_COUNT,
                ACCOUNT_PICKER_BOTTOM_SHEET_ACTIVE_DISMISSAL_COUNT,
                ASSISTANT_VOICE_SEARCH_ENABLED,
                AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER,
                AUTOFILL_ASSISTANT_NUMBER_OF_LITE_SCRIPTS_CANCELED,
                AUTOFILL_ASSISTANT_PROACTIVE_HELP,
                APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE,
                APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO,
                APPLICATION_OVERRIDE_LANGUAGE,
                CHROME_SURVEY_DOWNLOAD_ATTEMPTS.pattern(),
                CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.pattern(),
                CLIPBOARD_SHARED_URI,
                CLIPBOARD_SHARED_URI_TIMESTAMP,
                COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP,
                CONDITIONAL_TAB_STRIP_CONTINUOUS_DISMISS_COUNTER,
                CONDITIONAL_TAB_STRIP_FEATURE_STATUS,
                CONDITIONAL_TAB_STRIP_LAST_SHOWN_TIMESTAMP,
                CONDITIONAL_TAB_STRIP_OPT_OUT,
                CONTEXT_MENU_OPEN_IMAGE_IN_EPHEMERAL_TAB_CLICKED,
                CONTEXT_MENU_OPEN_IN_EPHEMERAL_TAB_CLICKED,
                CONTEXT_MENU_OPEN_NEW_TAB_IN_GROUP_ITEM_FIRST,
                CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED,
                CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED,
                CONTEXT_MENU_SHOP_SIMILAR_PRODUCTS_CLICKED,
                CONTEXT_MENU_SEARCH_SIMILAR_PRODUCTS_CLICKED,
                CRYPTID_LAST_RENDER_TIMESTAMP,
                DEFAULT_BROWSER_PROMO_LAST_DEFAULT_STATE,
                DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME,
                DEFAULT_BROWSER_PROMO_PROMOED_BY_SYSTEM_SETTINGS,
                DEFAULT_BROWSER_PROMO_PROMOED_COUNT,
                DEFAULT_BROWSER_PROMO_SESSION_COUNT,
                EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                FEED_ARTICLES_LIST_VISIBLE,
                FEED_PLACEHOLDER_DENSE,
                FIRST_RUN_SKIPPED_BY_POLICY,
                FLAGS_CACHED.pattern(),
                FLAGS_FIELD_TRIAL_PARAM_CACHED.pattern(),
                FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                HOMEPAGE_LOCATION_POLICY,
                HOMEPAGE_USE_CHROME_NTP,
                IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT,
                IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN,
                ISOLATED_SPLITS_DEX_COMPILE_VERSION,
                LAST_SESSION_BROWSER_PID,
                LAST_SESSION_APPLICATION_STATE,
                OFFLINE_INDICATOR_V2_WALL_TIME_SHOWN_MS,
                OFFLINE_INDICATOR_V2_LAST_UPDATE_WALL_TIME_MS,
                OFFLINE_INDICATOR_V2_TIME_IN_FOREGROUND_MS,
                OFFLINE_INDICATOR_V2_TIME_IN_BACKGROUND_MS,
                OFFLINE_INDICATOR_V2_FIRST_TIME_IN_FOREGROUND_MS,
                OFFLINE_INDICATOR_V2_NUM_TIMES_BACKGROUNDED,
                OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES,
                OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD,
                OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS,
                OFFLINE_MEASUREMENTS_HTTP_PROBE_URL,
                OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULTS_LIST,
                OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED_LIST,
                OFFLINE_MEASUREMENTS_IS_ROAMING_LIST,
                OFFLINE_MEASUREMENTS_USER_AGENT_STRING,
                OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS,
                OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST,
                PERSISTENT_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD,
                PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT,
                PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD,
                PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT,
                PRICE_TRACKING_TRACK_PRICES_ON_TABS,
                PROMO_IS_DISMISSED.pattern(),
                PROMO_TIMES_SEEN.pattern(),
                QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                QUERY_TILES_NUM_RECENT_MV_TILE_CLICKS,
                QUERY_TILES_NUM_RECENT_QUERY_TILE_CLICKS,
                QUERY_TILES_SHOW_ON_NTP,
                SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                SETTINGS_SAFETY_CHECK_RUN_COUNTER,
                SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP,
                TWA_DISCLOSURE_SEEN_PACKAGES,
                VIDEO_TUTORIALS_SHARE_URL_SET
        );
        // clang-format on
    }

    private ChromePreferenceKeys() {}
}
