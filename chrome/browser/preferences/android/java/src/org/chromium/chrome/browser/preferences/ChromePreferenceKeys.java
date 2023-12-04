// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.chromium.components.browser_ui.share.ClipboardConstants.CLIPBOARD_SHARED_URI;
import static org.chromium.components.browser_ui.share.ClipboardConstants.CLIPBOARD_SHARED_URI_TIMESTAMP;
import static org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants.USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.components.browser_ui.accessibility.AccessibilityConstants;

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
 * Tests in ChromePreferenceKeysTest and checks in {@link StrictPreferenceKeyChecker} ensure the
 * validity of this file.
 */
public final class ChromePreferenceKeys {
    /** Whether the current adaptive toolbar customization is enabled. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED =
            "Chrome.AdaptiveToolbarCustomization.Enabled";

    /** The current adaptive toolbar customization setting in the preferences. */
    public static final String ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS =
            "Chrome.AdaptiveToolbarCustomization.Settings";

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

    /** Autofill assistant keys. */
    /** Whether Autofill Assistant is enabled */
    public static final String AUTOFILL_ASSISTANT_ENABLED = "autofill_assistant_switch";

    /** Whether the user has seen a lite-script before or is a first-time user. */
    public static final String AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER =
            "Chrome.AutofillAssistant.LiteScriptFirstTimeUser";

    /** Whether the Autofill Assistant onboarding has been accepted. */
    public static final String AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED =
            "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED";

    /** Whether proactive help is enabled. */
    public static final String AUTOFILL_ASSISTANT_PROACTIVE_HELP_ENABLED =
            "Chrome.AutofillAssistant.ProactiveHelp";

    public static final String BACKUP_FIRST_BACKUP_DONE = "first_backup_done";

    public static final String BOOKMARKS_LAST_MODIFIED_FOLDER_ID = "last_bookmark_folder_id";
    public static final String BOOKMARKS_LAST_USED_URL = "enhanced_bookmark_last_used_url";
    public static final String BOOKMARKS_LAST_USED_PARENT =
            "enhanced_bookmark_last_used_parent_folder";
    public static final String BOOKMARKS_SORT_ORDER = "Chrome.Bookmarks.BookmarkRowSortOrder";
    public static final String BOOKMARKS_VISUALS_PREF = "Chrome.Bookmarks.BookmarkRowDisplay";

    /**
     * Whether Chrome is set as the default browser.
     * Default value is false.
     */
    public static final String CHROME_DEFAULT_BROWSER = "applink.chrome_default_browser";

    /**
     * The ID generated to represent the current browser installation in the DM Server for Cloud
     * Management.
     */
    public static final String CLOUD_MANAGEMENT_CLIENT_ID = "Chrome.Policy.CloudManagementClientId";

    /**
     * The server-side token generated by the Device Management server on browser enrollment for
     * Cloud Management.
     */
    public static final String CLOUD_MANAGEMENT_DM_TOKEN = "Chrome.Policy.CloudManagementDMToken";

    /**
     * Save the timestamp of the last time that chrome-managed commerce subscriptions are
     * initialized.
     */
    public static final String COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP =
            "Chrome.CommerceSubscriptions.ChromeManagedTimestamp";

    /**
     * Marks that the content suggestions surface has been shown.
     * Default value is false.
     */
    public static final String CONTENT_SUGGESTIONS_SHOWN = "content_suggestions_shown";

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

    public static final String CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED =
            "Chrome.ContextMenu.SearchWithGoogleLensClicked";

    public static final String CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED =
            "Chrome.ContextMenu.ShopImageWithGoogleLensClicked";

    /** Key used to record the number of dismissals of the Continuous Search UI. */
    public static final String CONTINUOUS_SEARCH_DISMISSAL_COUNT =
            "Chrome.ContinuousSearch.DismissalCount";

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

    /** Package name of the client app that uses CCT service of the last launched CCT. */
    public static final String CUSTOM_TABS_LAST_CLIENT_PACKAGE =
            "Chrome.CustomTabs.LastClientPackage";

    public static final String CUSTOM_TABS_LAST_CLOSE_TIMESTAMP =
            "Chrome.CustomTabs.LastCloseTimestamp";
    public static final String CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION =
            "Chrome.CustomTabs.LastCloseTabInteraction";

    /** The referrer URI string of the last launched CCT. */
    public static final String CUSTOM_TABS_LAST_REFERRER = "Chrome.CustomTabs.LastReferrer";

    /** {@link Activity#getTaskId()} of the last launched CCT. */
    public static final String CUSTOM_TABS_LAST_TASK_ID = "Chrome.CustomTabs.LastTaskId";

    /** Uri of the last launched CCT. */
    public static final String CUSTOM_TABS_LAST_URL = "pref_last_custom_tab_url";

    /** Keys used to save whether it is ready to promo. */
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

    /** Indicates whether the desktop site global setting was enabled by default for a device. */
    public static final String DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING =
            "Chrome.RequestDesktopSiteGlobalSetting.DefaultEnabled";

    /**
     * Indicates whether the device qualifies for default-enabling the desktop site global setting.
     * Not set: the device is not eligible for the experiment;
     * True: the device is eligible for the experiment;
     * False: a downgrade is pending because a previously eligible device is not eligible for the
     * experiment any more.
     */
    public static final String DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT =
            "Chrome.RequestDesktopSiteGlobalSetting.DefaultEnabledCohort";

    /**
     * Indicates whether the device qualifies for showing a message to opt-in to the desktop site
     * global setting.
     */
    public static final String DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT =
            "Chrome.RequestDesktopSiteGlobalSetting.OptInMessageCohort";

    /**
     * Indicates display spec when the device is added to the default-on cohort for the desktop site
     * global setting experiment.
     */
    public static final String DESKTOP_SITE_GLOBAL_SETTING_DEFAULT_ON_COHORT_DISPLAY_SPEC =
            "Chrome.RequestDesktopSiteGlobalSetting.DefaultOnCohortDisplaySpec";

    /**
     * Indicates that Chrome should show an alert to the user about data privacy if the device
     * lock is removed.
     */
    public static final String DEVICE_LOCK_SHOW_ALERT_IF_REMOVED =
            "Chrome.DeviceLock.ShowAlertIfRemoved";

    public static final String DOWNLOAD_AUTO_RESUMPTION_ATTEMPT_LEFT = "ResumptionAttemptLeft";
    public static final String DOWNLOAD_FOREGROUND_SERVICE_OBSERVERS = "ForegroundServiceObservers";
    public static final String DOWNLOAD_IS_DOWNLOAD_HOME_ENABLED =
            "org.chromium.chrome.browser.download.IS_DOWNLOAD_HOME_ENABLED";
    public static final String DOWNLOAD_INTERSTITIAL_DOWNLOAD_PENDING_REMOVAL =
            "Chrome.DownloadInterstitial.PendingRemoval";
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

    public static final String FIRST_RUN_CACHED_TOS_ACCEPTED = "first_run_tos_accepted";
    public static final String FIRST_RUN_FLOW_COMPLETE = "first_run_flow";
    // BACKUP_FLOW_SIGNIN_ACCOUNT_NAME used to be employed for the FRE too, thus the "first_run_"
    // prefix. The string should NOT be changed without some sort of migration.
    public static final String BACKUP_FLOW_SIGNIN_ACCOUNT_NAME = "first_run_signin_account_name";
    public static final String FIRST_RUN_FLOW_SIGNIN_SETUP = "first_run_signin_setup";
    // Needed by ChromeBackupAgent
    public static final String FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE = "lightweight_first_run_flow";
    public static final String FIRST_RUN_SKIP_WELCOME_PAGE = "skip_welcome_page";
    public static final String FIRST_RUN_SKIPPED_BY_POLICY = "Chrome.FirstRun.SkippedByPolicy";

    /** Cached feature flags generated by CachedFeatureFlags use this prefix. */
    public static final KeyPrefix FLAGS_CACHED = new KeyPrefix("Chrome.Flags.CachedFlag.*");

    /**
     * Streak of crashes before caching flags from native. This controls Safe Mode for Cached Flags.
     */
    public static final String FLAGS_CRASH_STREAK_BEFORE_CACHE =
            "Chrome.Flags.CrashStreakBeforeCache";

    /** How many runs of Safe Mode for Cached Flags are left before trying a normal run. */
    public static final String FLAGS_SAFE_MODE_RUNS_LEFT = "Chrome.Flags.SafeModeRunsLeft";

    /** Cached field trial parameters generated by CachedFeatureFlags use this prefix. */
    public static final KeyPrefix FLAGS_FIELD_TRIAL_PARAM_CACHED =
            new KeyPrefix("Chrome.Flags.FieldTrialParamCached.*");

    /** See CachedFeatureFlags.getLastCachedMinimalBrowserFlagsTimeMillis(). */
    public static final String FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS =
            "Chrome.Flags.LastCachedMinimalBrowserFlagsTimeMillis";

    public static final String FONT_USER_FONT_SCALE_FACTOR =
            AccessibilityConstants.FONT_USER_FONT_SCALE_FACTOR;
    public static final String FONT_USER_SET_FORCE_ENABLE_ZOOM =
            AccessibilityConstants.FONT_USER_SET_FORCE_ENABLE_ZOOM;

    public static final String HISTORY_SHOW_HISTORY_INFO = "history_home_show_info";

    /** Keys used to save settings related to homepage. */
    public static final String DEPRECATED_HOMEPAGE_CUSTOM_URI = "homepage_custom_uri";

    public static final String DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI =
            "Chrome.Homepage.PartnerCustomizedDefaultUri";
    public static final String HOMEPAGE_CUSTOM_GURL = "Chrome.Homepage.CustomGurl";
    public static final String HOMEPAGE_ENABLED = "homepage";
    public static final String HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL =
            "Chrome.Homepage.PartnerCustomizedDefaultGurl";
    public static final String HOMEPAGE_USE_CHROME_NTP = "Chrome.Homepage.UseNTP";
    public static final String HOMEPAGE_USE_DEFAULT_URI = "homepage_partner_enabled";

    /** Key used to save homepage location set by enterprise policy */
    public static final String DEPRECATED_HOMEPAGE_LOCATION_POLICY =
            "Chrome.Policy.HomepageLocation";

    public static final String HOMEPAGE_LOCATION_POLICY_GURL = "Chrome.Policy.HomepageLocationGurl";

    /** Used for get image descriptions feature, track "Just once"/"Don't ask again" choice. */
    public static final String IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT =
            "Chrome.ImageDescriptions.JustOnceCount";

    public static final String IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN =
            "Chrome.ImageDescriptions.DontAskAgain";

    public static final String INCOGNITO_SHORTCUT_ADDED = "incognito-shortcut-added";

    /** Indicates how many times the Incognito re-auth promo card was shown in the tab switcher. */
    public static final String INCOGNITO_REAUTH_PROMO_SHOW_COUNT =
            "Chrome.IncognitoReauth.PromoShowCount";

    /**
     * Indicates whether the re-auth promo card is enabled. This gets disabled if either the re-auth
     * feature is disabled or the INCONGITO_REAUTH_PROMO_SHOW_COUNT exceeds the limit.
     */
    public static final String INCOGNITO_REAUTH_PROMO_CARD_ENABLED =
            "Chrome.IncognitoReauth.PromoCardEnabled";

    /**
     * The last version the dex compile workaround ran on. See SplitChromeApplication for more
     * details.
     */
    public static final String ISOLATED_SPLITS_DEX_COMPILE_VERSION =
            "Chrome.IsolatedSplits.VersionCode";

    /** Whether the device is from an EEA country. */
    public static final String IS_EEA_CHOICE_COUNTRY = "Chrome.SearchEngine.IsEeaChoiceCountry";

    /** Whether the default search engine is Google. */
    public static final String IS_DSE_GOOGLE = "Chrome.SearchEngine.IsDSEGoogle";

    /** The new_tab_url of the default search engine if it isn't Google. */
    public static final String DSE_NEW_TAB_URL = "Chrome.SearchEngine.DSENewTabUrl";

    /**
     * When the user is shown a badge that the current Android OS version is unsupported, and they
     * tap it to display the menu (which has additional information), we store the current version
     * of Chrome to this preference to ensure we only show the badge once. The value is cleared
     * if the Chrome version later changes.
     */
    public static final String LATEST_UNSUPPORTED_VERSION = "android_os_unsupported_chrome_version";

    /** The previous browser process PID, updated when crash reporting is initialized. */
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
    public static final String LOCALE_MANAGER_MISSING_TIMEZONES =
            "com.android.chrome.MISSING_TIMEZONES";
    public static final String LOCALE_MANAGER_PARTNER_PROMO_KEYWORD_SELECTED =
            "LocaleManager_PARTNER_PROMO_SELECTED_KEYWORD";
    public static final String LOCALE_MANAGER_PROMO_SHOWN = "LocaleManager_PREF_PROMO_SHOWN";
    public static final String LOCALE_MANAGER_PROMO_V2_CHECKED =
            "LocaleManager_PREF_PROMO_VER2_CHECKED";
    public static final String LOCALE_MANAGER_PROMO_V3_CHECKED =
            "Chrome.SearchEngineChoice.LocaleManagerPromoV3Checked";
    public static final String LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE =
            "com.android.chrome.SEARCH_ENGINE_PROMO_SHOWN";
    public static final String LOCALE_MANAGER_SEARCH_WIDGET_PRESENT_FIRST_START =
            "LocaleManager_SEARCH_WIDGET_PRESENT_FIRST_START";
    public static final String LOCALE_MANAGER_SHOULD_REPING_RLZ_FOR_SEARCH_PROMO =
            "LocaleManager_SHOULD_REPING_RLZ_FOR_SEARCH_PROMO_KEYWORD";
    public static final String LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE =
            "LocaleManager_WAS_IN_SPECIAL_LOCALE";

    public static final String MEDIA_WEBRTC_NOTIFICATION_IDS = "WebRTCNotificationIds";

    public static final String METRICS_MAIN_INTENT_LAUNCH_COUNT = "MainIntent.LaunchCount";
    public static final String METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP = "MainIntent.LaunchTimestamp";

    // {Instance:Task} ID mapping for multi-instance support.
    public static final KeyPrefix MULTI_INSTANCE_TASK_MAP =
            new KeyPrefix("Chrome.MultiInstance.TaskMap.*");
    public static final String MULTI_WINDOW_START_TIME = "Chrome.MultiWindow.StartTime";
    public static final String MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM =
            "Chrome.MultiWindow.CloseWindowSkipConfirm";

    public static final String MULTI_INSTANCE_START_TIME = "Chrome.MultiInstance.StartTime";

    // Start timestamp of 1-day period for measuring the max count of instances used simultaneously.
    public static final String MULTI_INSTANCE_MAX_COUNT_TIME = "Chrome.MultiInstance.MaxCountTime";
    // Max count of Chrome instances used in a day.
    public static final String MULTI_INSTANCE_MAX_INSTANCE_COUNT =
            "Chrome.MultiInstance.MaxInstanceCount";
    // Information on each instance.
    public static final KeyPrefix MULTI_INSTANCE_INCOGNITO_TAB_COUNT =
            new KeyPrefix("Chrome.MultiInstance.IncognitoTabCount.*");
    public static final KeyPrefix MULTI_INSTANCE_IS_INCOGNITO_SELECTED =
            new KeyPrefix("Chrome.MultiInstance.IsIncognitoSelected.*");
    public static final KeyPrefix MULTI_INSTANCE_TAB_COUNT =
            new KeyPrefix("Chrome.MultiInstance.TabCount.*"); // Normal tab count
    public static final KeyPrefix MULTI_INSTANCE_TITLE =
            new KeyPrefix("Chrome.MultiInstance.Title.*");
    public static final KeyPrefix MULTI_INSTANCE_LAST_ACCESSED_TIME =
            new KeyPrefix("Chrome.MultiInstance.LastAccessedTime.*");
    public static final KeyPrefix MULTI_INSTANCE_URL = new KeyPrefix("Chrome.MultiInstance.Url.*");

    public static final String NOTIFICATIONS_CHANNELS_VERSION = "channels_version_key";
    public static final String NOTIFICATIONS_LAST_SHOWN_NOTIFICATION_TYPE =
            "NotificationUmaTracker.LastShownNotificationType";
    public static final String NOTIFICATIONS_NEXT_TRIGGER =
            "notification_trigger_scheduler.next_trigger";

    public static final String NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY =
            "Chrome.NotificationPermission.RationaleTimestamp";

    // Number of times we've showed any prompt (either Android UI or Chrome rationale) related to
    // the notification permission.
    public static final String NOTIFICATION_PERMISSION_REQUEST_COUNT =
            "Chrome.NotificationPermission.RequestCount";

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

    /** Key to cache whether offline indicator v2 (persistent offline indicator) is enabled. */
    public static final String OFFLINE_INDICATOR_V2_ENABLED = "offline_indicator_v2_enabled";

    /** The wall time of when the offline indicator was shown in milliseconds. */
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
     * Keys used to store data for the OfflineMeasurementsBackgroundTask. The background task has
     * been removed, and these keys are just used to clear any persisted data.
     */
    public static final String OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES =
            "Chrome.OfflineMeasurements.CurrentTaskMeasurementIntervalInMinutes";

    public static final String OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS =
            "Chrome.OfflineMeasurements.LastCheckMillis";
    public static final String OFFLINE_MEASUREMENTS_USER_AGENT_STRING =
            "Chrome.OfflineMeasurements.UserAgentString";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_URL =
            "Chrome.OfflineMeasurements.HttpProbeUrl";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS =
            "Chrome.OfflineMeasurements.HttpProbeTimeoutMs";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD =
            "Chrome.OfflineMeasurements.HttpProbeMethod";

    /**
     * Serialized SystemStateList of aggregated SystemStates collected from the
     * OfflineMeasurementsBackgroundTask. When possible, these values will be recorded to UMA and
     * UKM then cleared.
     */
    public static final String OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST =
            "Chrome.OfflineMeasurements.SystemStateList";

    /**
     * Serialized GroupsInfo data used by Omnibox Suggestions to present suggestions before
     * natives are ready.
     */
    public static final String OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO =
            "Chrome.Omnibox.CachedZeroSuggestGroupsInfo";

    /**
     * Prefix of the preferences to persist pushed notifications when native is not initialized.
     * Each suffix pertains to a specific OptimizationType. All entries are cleared when native is
     * initialized.
     */
    public static final KeyPrefix OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CACHE =
            new KeyPrefix("Chrome.OptimizationGuide.PushNotificationCache.*");

    /** The accounts Password Protection should protect. */
    public static final String PASSWORD_PROTECTION_ACCOUNTS = "Chrome.PasswordProtection.Accounts";

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

    /**
     * Indicates whether Page Insights Hub's Privacy Notice has been closed by user; used to ensure
     * user does not see it again.
     */
    public static final String PIH_PRIVACY_NOTICE_CLOSED =
            "Chrome.PageInsightsHub.PrivacyNoticeClosedByUser";

    /** Indicates the latest timestamp when Privacy Notice was shown to the user */
    public static final String PIH_PRIVACY_NOTICE_LAST_SHOWN_TIMESTAMP =
            "Chrome.PageInsightsHub.PageInsightsHubLastOpenedTimestamp";

    /**
     * Indicates the number of times Privacy Notice of Page Insights Hub was opened by user till now
     * . The privacy notice is not shown after it has been shown 3 times.
     */
    public static final String PIH_PRIVACY_NOTICE_SHOWN_TOTAL_COUNT =
            "Chrome.PageInsightsHub.NumberOfTimesPageInsightsHubOpenedByUser";

    /**
     * Save the timestamp of the last time that we record metrics on whether user enables the price
     * tracking annotations.
     */
    public static final String PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP =
            "Chrome.PriceTracking.AnnotationsEnabledMetricsTimestamp";

    /**
     * Save the serialized timestamps of the previously shown chrome-managed price drop
     * notifications.
     */
    public static final String PRICE_TRACKING_CHROME_MANAGED_NOTIFICATIONS_TIMESTAMPS =
            "Chrome.PriceTracking.ChromeManagedNotificationsTimestamps";

    /** Whether the PriceAlertsMessageCard is enabled. */
    public static final String PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD =
            "Chrome.PriceTracking.PriceAlerts";

    /** Indicates how many times the PriceAlertsMessageCard has shown in the tab switcher. */
    public static final String PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT =
            "Chrome.PriceTracking.PriceAlertsShowCount";

    /** Whether the PriceWelcomeMessageCard is enabled. */
    public static final String PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD =
            "Chrome.PriceTracking.PriceWelcome";

    /** Indicates how many times the PriceWelcomeMessageCard has shown in the tab switcher. */
    public static final String PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT =
            "Chrome.PriceTracking.PriceWelcomeShowCount";

    /** Whether users turn on the feature track prices on tabs. */
    public static final String PRICE_TRACKING_TRACK_PRICES_ON_TABS =
            "Chrome.PriceTracking.TrackPricesOnTabs";

    /**
     * Save the serialized timestamps of the previously shown user-managed price drop notifications.
     */
    public static final String PRICE_TRACKING_USER_MANAGED_NOTIFICATIONS_TIMESTAMPS =
            "Chrome.PriceTracking.UserManagedNotificationsTimestamps";

    public static final String PRIVACY_METRICS_IN_SAMPLE = "in_metrics_sample";

    /**
     * This is deprecated and is going to be removed in the future (See https://crbug.com/1320040).
     * Use PrivacyPreferencesManagerImpl#isUsageAndCrashReportingPermitted to know if metrics
     * reporting is allowed.
     */
    @Deprecated public static final String PRIVACY_METRICS_REPORTING = "metrics_reporting";

    public static final String PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER =
            "Chrome.Privacy.UsageAndCrashReportingPermittedByUser";

    public static final String PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY =
            "Chrome.Privacy.UsageAndCrashReportingPermittedByPolicy";

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
     * Whether the promotions were skipped on first invocation.
     * Default value is false.
     */
    public static final String PROMOS_SKIPPED_ON_FIRST_START = "promos_skipped_on_first_start";

    /**
     * Key for the PWA Restore feature. Used to indicate to the promo code that the PWA Restore
     * backend has determined that apps are available for restoring (boolean flag).
     */
    public static final String PWA_RESTORE_APPS_AVAILABLE = "Chrome.PwaRestore.AppsAvailable";

    /**
     * Key for the PWA Restore feature. Used in {@link
     * org.chromium.chrome.browser.webapps.PwaRestorePromoUtils} to figure out when to show the
     * promo.
     */
    public static final String PWA_RESTORE_PROMO_STAGE = "Chrome.PwaRestore.PromoStage";

    /** The next timestamp to decide whether to show query tiles. */
    public static final String QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS =
            "Chrome.Querytiles.NextDecisionTime";

    /** Whether query tiles should be shown on NTP. Default value is false. */
    public static final String QUERY_TILES_SHOW_ON_NTP = "Chrome.Querytiles.ShowOnNTP";

    /** Keys used to store result from segmentation model of showing query tiles on NTP. */
    public static final String QUERY_TILES_SHOW_SEGMENTATION_RESULT =
            "Chrome.QueryTiles.ShowSegmentationResult";

    /** Whether query tiles is already shown on Start surface. Default value is false. */
    public static final String QUERY_TILES_SHOWN_ON_START_SURFACE =
            "Chrome.QueryTiles.ShownOnStartSurface";

    /**
     * Keys used to store user actions for behavioral targeting of showing Start surface on startup.
     */
    public static final String START_RETURN_TIME_SEGMENTATION_RESULT_MS =
            "Chrome.StartSurface.StartReturnTimeSegmentationResultMs";

    public static final String REGULAR_TAB_COUNT = "Chrome.StartSurface.RegularTabCount";
    public static final String INCOGNITO_TAB_COUNT = "Chrome.StartSurface.IncognitoTabCount";
    public static final String IS_LAST_VISITED_TAB_SRP = "Chrome.StartSurface.IsLastVisitedTabSRP";

    /** Key used to store user actions for collapsing search resumption module on NTP. */
    public static final String SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP =
            "Chrome.SearchResumptionModule.Collapse";

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

    /**
     * Key used to store the date of when an OS level choice had last been applied as default
     * search engine by Chrome. Linux epoch timestamp in millis.
     */
    public static final String SEARCH_ENGINE_CHOICE_OS_CHOICE_APPLIED_TIMESTAMP =
            "Chrome.SearchEngineChoice.OsChoiceAppliedTimestamp";

    public static final String SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE =
            "org.chromium.chrome.browser.searchwidget.IS_VOICE_SEARCH_AVAILABLE";
    public static final String SEARCH_WIDGET_NUM_CONSECUTIVE_CRASHES =
            "org.chromium.chrome.browser.searchwidget.NUM_CONSECUTIVE_CRASHES";
    public static final String SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME =
            "org.chromium.chrome.browser.searchwidget.SEARCH_ENGINE_SHORTNAME";
    public static final String SEARCH_WIDGET_SEARCH_ENGINE_URL =
            "org.chromium.chrome.browser.searchwidget.SEARCH_ENGINE_URL";
    public static final String SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE =
            "org.chromium.chrome.browser.searchwidget.IS_GOOGLE_LENS_AVAILABLE";
    public static final String SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE =
            "org.chromium.chrome.browser.searchwidget.IS_INCOGNITO_AVAILABLE";

    // Segmentation platform related prefs.
    public static final String SEGMENTATION_FEED_ACTIVE_USER = "Chrome.Segmentation.FeedActiveUser";
    public static final String SEGMENTATION_SHOW_QUERY_TILES = "Chrome.Segmentation.ShowQueryTiles";

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

    public static final String SHARING_LAST_SHARED_COMPONENT_NAME =
            "Chrome.Sharing.LastSharedComponentName";

    public static final String SIGNIN_ACCOUNTS_CHANGED = "prefs_sync_accounts_changed";

    /** Holds the new account's name if the currently signed in account has been renamed. */
    public static final String SIGNIN_ACCOUNT_RENAMED = "prefs_sync_account_renamed";

    /**
     * Holds the last read index of all the account changed events of the current signed in account.
     */
    public static final String SIGNIN_ACCOUNT_RENAME_EVENT_INDEX =
            "prefs_sync_account_rename_event_index";

    /** SyncPromo Show Count preference. */
    public static final KeyPrefix SYNC_PROMO_SHOW_COUNT =
            new KeyPrefix("Chrome.SyncPromo.ShowCount.*");

    /** SyncPromo total shown count preference across all access points. */
    public static final String SYNC_PROMO_TOTAL_SHOW_COUNT = "Chrome.SyncPromo.TotalShowCount";

    /** Generic signin and sync promo preferences. */
    public static final String SIGNIN_AND_SYNC_PROMO_SHOW_COUNT =
            "enhanced_bookmark_signin_promo_show_count";

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
    public static final String SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME =
            "Chrome.SigninPromoNTP.FirstShownTime";
    public static final String SIGNIN_PROMO_NTP_LAST_SHOWN_TIME =
            "Chrome.SigninPromoNTP.LastShownTime";

    /** Personalized signin promo preference. */
    public static final String SIGNIN_PROMO_BOOKMARKS_DECLINED = "signin_promo_bookmarks_declined";

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

    // sWAA (Supplemental Web and App Activity) user setting.
    public static final String SWAA_TIMESTAMP = "Chrome.Swaa.Timestamp";
    public static final String SWAA_STATUS = "Chrome.Swaa.Status";

    // The UI used to be an infobar in the past.
    public static final String SYNC_ERROR_MESSAGE_SHOWN_AT_TIME =
            "sync_error_infobar_shown_shown_at_time";

    public static final String TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF =
            "ChromeTabbedActivity.BackgroundTimeMs";

    public static final String TABBED_ACTIVITY_LAST_VISIBLE_TIME_MS =
            "Chrome.StartSurface.LastVisibleTimeMs";
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
     * The current theme setting in the user settings.
     * Default value is -1. Use NightModeUtils#getThemeSetting() to retrieve current setting or
     * default theme.
     */
    public static final String UI_THEME_SETTING = "ui_theme_setting";

    // Diagnostic counters for short sessions; see histogram
    // UMA.PreNative.ChromeActivityCounter2.
    public static final String UMA_ON_POSTCREATE_COUNTER = "Chrome.UMA.OnPostCreateCounter2";
    public static final String UMA_ON_RESUME_COUNTER = "Chrome.UMA.OnResumeCounter2";

    public static final String VERIFIED_DIGITAL_ASSET_LINKS = "verified_digital_asset_links";

    /** Key for deferred recording of list of uninstalled WebAPK packages. */
    public static final String WEBAPK_UNINSTALLED_PACKAGES = "webapk_uninstalled_packages";

    /**
     * Key used to save the time in milliseconds since epoch that the WebFeed intro was last shown.
     */
    public static final String WEB_FEED_INTRO_LAST_SHOWN_TIME_MS =
            "Chrome.WebFeed.IntroLastShownTimeMs";

    public static final String WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT =
            "Chrome.AccountPickerBottomSheet.ConsecutiveActiveDismissalCount";

    /**
     * Key used to save the time in milliseconds since epoch that the WebFeed intro for the WebFeed
     * ID was last shown.
     */
    public static final KeyPrefix WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX =
            new KeyPrefix("Chrome.WebFeed.IntroWebFeedIdShownTimeMs.*");

    /** Key used to save the number of times the WebFeed intro for the WebFeed ID was shown. */
    public static final KeyPrefix WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_COUNT_PREFIX =
            new KeyPrefix("Chrome.WebFeed.IntroWebFeedIdShownCount.*");

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

    public static final String BLUETOOTH_NOTIFICATION_IDS = "Chrome.Bluetooth.NotificationIds";
    public static final String USB_NOTIFICATION_IDS = "Chrome.USB.NotificationIds";

    /**
     * These values are currently used as SharedPreferences keys, along with the keys in
     * {@link LegacyChromePreferenceKeys#getKeysInUse()}. Add new SharedPreferences keys
     * here.
     *
     * @return The list of [keys in use] conforming to the format.
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> getKeysInUse() {
        return Arrays.asList(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED,
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER,
                AUTOFILL_ASSISTANT_PROACTIVE_HELP_ENABLED,
                APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE,
                APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO,
                APPLICATION_OVERRIDE_LANGUAGE,
                BLUETOOTH_NOTIFICATION_IDS,
                BOOKMARKS_SORT_ORDER,
                BOOKMARKS_VISUALS_PREF,
                CLIPBOARD_SHARED_URI,
                CLIPBOARD_SHARED_URI_TIMESTAMP,
                CLOUD_MANAGEMENT_CLIENT_ID,
                CLOUD_MANAGEMENT_DM_TOKEN,
                COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP,
                CONTEXT_MENU_OPEN_IMAGE_IN_EPHEMERAL_TAB_CLICKED,
                CONTEXT_MENU_OPEN_IN_EPHEMERAL_TAB_CLICKED,
                CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED,
                CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED,
                CONTINUOUS_SEARCH_DISMISSAL_COUNT,
                CUSTOM_TABS_LAST_CLIENT_PACKAGE,
                CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION,
                CUSTOM_TABS_LAST_CLOSE_TIMESTAMP,
                CUSTOM_TABS_LAST_REFERRER,
                CUSTOM_TABS_LAST_TASK_ID,
                CRYPTID_LAST_RENDER_TIMESTAMP,
                DEFAULT_BROWSER_PROMO_LAST_DEFAULT_STATE,
                DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME,
                DEFAULT_BROWSER_PROMO_PROMOED_BY_SYSTEM_SETTINGS,
                DEFAULT_BROWSER_PROMO_PROMOED_COUNT,
                DEFAULT_BROWSER_PROMO_SESSION_COUNT,
                DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING,
                DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT,
                DEPRECATED_HOMEPAGE_LOCATION_POLICY,
                DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                DESKTOP_SITE_GLOBAL_SETTING_DEFAULT_ON_COHORT_DISPLAY_SPEC,
                DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT,
                DEVICE_LOCK_SHOW_ALERT_IF_REMOVED,
                DOWNLOAD_INTERSTITIAL_DOWNLOAD_PENDING_REMOVAL,
                DSE_NEW_TAB_URL,
                EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                FEED_ARTICLES_LIST_VISIBLE,
                FIRST_RUN_SKIPPED_BY_POLICY,
                FLAGS_CACHED.pattern(),
                FLAGS_CRASH_STREAK_BEFORE_CACHE,
                FLAGS_FIELD_TRIAL_PARAM_CACHED.pattern(),
                FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                FLAGS_SAFE_MODE_RUNS_LEFT,
                HOMEPAGE_CUSTOM_GURL,
                HOMEPAGE_LOCATION_POLICY_GURL,
                HOMEPAGE_USE_CHROME_NTP,
                HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL,
                IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT,
                IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN,
                INCOGNITO_REAUTH_PROMO_CARD_ENABLED,
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                INCOGNITO_TAB_COUNT,
                IS_EEA_CHOICE_COUNTRY,
                IS_LAST_VISITED_TAB_SRP,
                IS_DSE_GOOGLE,
                ISOLATED_SPLITS_DEX_COMPILE_VERSION,
                LAST_SESSION_BROWSER_PID,
                LAST_SESSION_APPLICATION_STATE,
                LOCALE_MANAGER_PROMO_V3_CHECKED,
                MULTI_WINDOW_START_TIME,
                MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM,
                MULTI_INSTANCE_IS_INCOGNITO_SELECTED.pattern(),
                MULTI_INSTANCE_INCOGNITO_TAB_COUNT.pattern(),
                MULTI_INSTANCE_MAX_COUNT_TIME,
                MULTI_INSTANCE_MAX_INSTANCE_COUNT,
                MULTI_INSTANCE_LAST_ACCESSED_TIME.pattern(),
                MULTI_INSTANCE_START_TIME,
                MULTI_INSTANCE_TAB_COUNT.pattern(),
                MULTI_INSTANCE_TASK_MAP.pattern(),
                MULTI_INSTANCE_TITLE.pattern(),
                MULTI_INSTANCE_URL.pattern(),
                NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                NOTIFICATION_PERMISSION_REQUEST_COUNT,
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
                OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS,
                OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST,
                OFFLINE_MEASUREMENTS_USER_AGENT_STRING,
                OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO,
                OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CACHE.pattern(),
                PASSWORD_PROTECTION_ACCOUNTS,
                PERSISTENT_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                PIH_PRIVACY_NOTICE_CLOSED,
                PIH_PRIVACY_NOTICE_LAST_SHOWN_TIMESTAMP,
                PIH_PRIVACY_NOTICE_SHOWN_TOTAL_COUNT,
                PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                PRICE_TRACKING_CHROME_MANAGED_NOTIFICATIONS_TIMESTAMPS,
                PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD,
                PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT,
                PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD,
                PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT,
                PRICE_TRACKING_TRACK_PRICES_ON_TABS,
                PRICE_TRACKING_USER_MANAGED_NOTIFICATIONS_TIMESTAMPS,
                PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
                PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY,
                PROMO_IS_DISMISSED.pattern(),
                PROMO_TIMES_SEEN.pattern(),
                PWA_RESTORE_APPS_AVAILABLE,
                PWA_RESTORE_PROMO_STAGE,
                QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                QUERY_TILES_SHOW_ON_NTP,
                QUERY_TILES_SHOW_SEGMENTATION_RESULT,
                QUERY_TILES_SHOWN_ON_START_SURFACE,
                REGULAR_TAB_COUNT,
                SEARCH_ENGINE_CHOICE_OS_CHOICE_APPLIED_TIMESTAMP,
                SEGMENTATION_FEED_ACTIVE_USER,
                SEGMENTATION_SHOW_QUERY_TILES,
                SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                SETTINGS_SAFETY_CHECK_RUN_COUNTER,
                SHARING_LAST_SHARED_COMPONENT_NAME,
                START_RETURN_TIME_SEGMENTATION_RESULT_MS,
                SYNC_PROMO_SHOW_COUNT.pattern(),
                SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME,
                SIGNIN_PROMO_NTP_LAST_SHOWN_TIME,
                SYNC_PROMO_TOTAL_SHOW_COUNT,
                SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP,
                SWAA_TIMESTAMP,
                SWAA_STATUS,
                TABBED_ACTIVITY_LAST_VISIBLE_TIME_MS,
                TWA_DISCLOSURE_SEEN_PACKAGES,
                UMA_ON_POSTCREATE_COUNTER,
                UMA_ON_RESUME_COUNTER,
                USB_NOTIFICATION_IDS,
                USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                WEB_FEED_INTRO_LAST_SHOWN_TIME_MS,
                WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX.pattern(),
                WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_COUNT_PREFIX.pattern(),
                WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT);
    }

    private ChromePreferenceKeys() {}
}
