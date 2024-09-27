// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_PREF_NAMES_H_
#define ASH_CONSTANTS_ASH_PREF_NAMES_H_

namespace ash::prefs {

// Map of strings to values used for assistive input settings.
inline constexpr char kAssistiveInputFeatureSettings[] =
    "assistive_input_feature_settings";

// A boolean pref of whether assist personal info is enabled.
inline constexpr char kAssistPersonalInfoEnabled[] =
    "assistive_input.personal_info_enabled";

// A boolean pref of whether assist predictive writing is enabled.
inline constexpr char kAssistPredictiveWritingEnabled[] =
    "assistive_input.predictive_writing_enabled";

// A boolean pref of whether Google Calendar Integration is enabled.
// Disabling this pref disables calendar list and event list fetches from
// the user's Google Calendar, preventing them from viewing their Google
// Calendar events in the system tray - Calendar widget.
inline constexpr char kCalendarIntegrationEnabled[] =
    "ash.calendar_integration_enabled";

// Pref which stores a list of enabled Google integrations such as Calendar,
// Classroom, Tasks, etc. Individual integration names should not be changed
// because they are used in the `ContextualGoogleIntegrationsConfiguration`
// policy and metrics.
inline constexpr char kContextualGoogleIntegrationsConfiguration[] =
    "ash.contextual_google_integrations_configuration";
inline constexpr char kGoogleCalendarIntegrationName[] = "GoogleCalendar";
inline constexpr char kGoogleClassroomIntegrationName[] = "GoogleClassroom";
inline constexpr char kGoogleTasksIntegrationName[] = "GoogleTasks";
inline constexpr char kChromeSyncIntegrationName[] = "ChromeSync";
inline constexpr char kGoogleDriveIntegrationName[] = "GoogleDrive";
inline constexpr char kWeatherIntegrationName[] = "Weather";

// A boolean pref of whether emoji suggestion is enabled.
inline constexpr char kEmojiSuggestionEnabled[] =
    "assistive_input.emoji_suggestion_enabled";

// A boolean pref of whether emoji suggestion is enabled for managed device.
inline constexpr char kEmojiSuggestionEnterpriseAllowed[] =
    "assistive_input.emoji_suggestion.enterprise_allowed";

// A boolean pref of whether MagicBoost is enabled.
inline constexpr char kMagicBoostEnabled[] = "settings.magic_boost_enabled";

// A boolean pref of whether orca is enabled.
inline constexpr char kOrcaEnabled[] = "assistive_input.orca_enabled";

// A boolean pref indicating the status of the orca feedback.
inline constexpr char kOrcaFeedbackEnabled[] =
    "assistive_input.orca_feedback_enabled";

// A boolean pref used by an admin policy to enable/disable Orca. See the
// policy at OrcaEnabled.yaml.
inline constexpr char kManagedOrcaEnabled[] =
    "assistive_input.managed.orca_enabled";

// A boolean pref of whether Help Me Read (HMR) is enabled.
inline constexpr char kHmrEnabled[] = "settings.mahi_enabled";

// A boolean pref of whether feedback for Help Me Read (HMR) is allowed.
inline constexpr char kHmrFeedbackAllowed[] = "settings.mahi_feedback_allowed";

// An integer pref used by an admin policy to control the settings of Help Me
// Read (HMR). See the policy at HelpMeReadSettings.yaml.
inline constexpr char kHmrManagedSettings[] = "settings.managed.help_me_read";

// An integer pref that stores the times the Mahi nudge has been shown.
inline constexpr char kMahiNudgeShownCount[] = "mahi.nudge_shown_count";

// A time pref that stores the time the Mahi nudge was last shown.
inline constexpr char kMahiNudgeLastShownTime[] = "mahi.nudge_last_shown";

// An integer pref which indicates the HMR (Quick answers and Mahi) consent
// status from the user.
inline constexpr char kHMRConsentStatus[] = "settings.hmr.consent_status";

// An integer pref which indicates the number of times the HMR (Quick answers
// and Mahi) consent window has been dismissed by the user.
inline constexpr char kHMRConsentWindowDismissCount[] =
    "settings.hmr.consent_window_dismiss_count";

// A boolean pref used by an admin policy to enable/disable particular
// features on the physical keyboard. See the policy at
// PhysicalKeyboardAutocorrect.yml.
inline constexpr char kManagedPhysicalKeyboardAutocorrectAllowed[] =
    "settings.ime.managed.physical_keyboard.autocorrect_enabled";

// A boolean pref used by an admin policy to enable/disable particular
// features on the physical keyboard. See the policy at
// PhysicalKeyboardPredictiveWriting.yml.
inline constexpr char kManagedPhysicalKeyboardPredictiveWritingAllowed[] =
    "settings.ime.managed.physical_keyboard.predictive_writing_enabled";

// An integer pref which indicates the orca consent status from the user.
inline constexpr char kOrcaConsentStatus[] = "ash.orca.consent_status";

// An integer pref which indicates the number of times the orca consent window
// has been dismissed by the user.
inline constexpr char kOrcaConsentWindowDismissCount[] =
    "ash.orca.consent_window_dismiss_count";

// A boolean pref of whether GIF support is enabled in emoji picker.
inline constexpr char kEmojiPickerGifSupportEnabled[] =
    "emoji_picker.gif_support_enabled";

// A dictionary storing the history of emoji picker inputs. The history is keyed
// by the emoji picker categories, i.e. "emoji", "symbol" and "emoticon". The
// history value for each category is a list of objects, each should at least
// contain a "text" field indicating the inputted emoji/symbol/emoticon.
inline constexpr char kEmojiPickerHistory[] = "emoji_picker.history";

// A dictionary storing user preferences for emoji picker.
inline constexpr char kEmojiPickerPreferences[] = "emoji_picker.preferences";

// An integer pref which indicates the number of times the user selects the caps
// lock toggle in the picker. The number will be halved together with
// `caps_lock_displayed_count` whenever `caps_lock_displayed_count` reaches a
// threshold so that recent usages have more weights when calculating the ratio
// between them.
inline constexpr char kPickerCapsLockSelectedCountPrefName[] =
    "ash.picker.caps_lock_selected_count";

// An integer pref which indicates the number of times the caps lock toggle is
// displayed in the picker zero state view. Only used to calculate the ratio
// between `caps_lock_selected_count` and itself. It will not grow infinitely
// but will be halved whenever it reaches a threshold.
inline constexpr char kPickerCapsLockDislayedCountPrefName[] =
    "ash.picker.caps_lock_displayed_count";

// Pref which stores a list of Embedded Universal Integrated Circuit Card
// (EUICC) D-Bus paths which have had their installed profiles refreshed from
// Hermes. Each path is stored as a string.
inline constexpr char kESimRefreshedEuiccs[] = "cros_esim.refreshed_euiccs";

// Pref which stores a list of eSIM profiles. Each entry in the list is created
// by serializing a CellularESimProfile.
inline constexpr char kESimProfiles[] = "cros_esim.esim_profiles";

// Pref which stores metadata about enterprise managed cellular networks. The
// metadata includes the name of the network that was provided by policy, the
// activation code that was used to install an eSIM profile for this network,
// and the ICCID of the installed profile.
inline constexpr char kManagedCellularESimMetadata[] =
    "cros_esim.managed_esim_metadata";

// Pref which stores a dictionary of Integrated Circuit Card IDentifier (ICCID)
// and Subscription Management - Data Preparation (SMDP+) address pair for each
// managed cellular network.
inline constexpr char kManagedCellularIccidSmdpPair[] =
    "cros_esim.managed_iccid_smdp_pair";

// A boolean pref for whether playing charging sounds is enabled.
inline constexpr char kChargingSoundsEnabled[] = "ash.charging_sounds.enabled";

// A boolean pref indicating whether a user has enabled window snap suggestions.
inline constexpr char kSnapWindowSuggestions[] =
    "ash.snap_window_suggestions.enabled";

// A boolean pref for whether playing a low battery sound is enabled.
inline constexpr char kLowBatterySoundEnabled[] =
    "ash.low_battery_sound.enabled";

// A dictionary pref to hold the mute setting for all the currently known
// audio devices.
inline constexpr char kAudioDevicesMute[] = "settings.audio.devices.mute";

// A dictionary pref storing the gain settings for all the currently known
// audio input devices.
inline constexpr char kAudioDevicesGainPercent[] =
    "settings.audio.devices.gain_percent";

// A dictionary pref storing the volume settings for all the currently known
// audio output devices.
inline constexpr char kAudioDevicesVolumePercent[] =
    "settings.audio.devices.volume_percent";

// An integer pref to initially mute volume if 1. This pref is ignored if
// |kAudioOutputAllowed| is set to false, but its value is preserved, therefore
// when the policy is lifted the original mute state is restored.  This setting
// is here only for migration purposes now. It is being replaced by the
// |kAudioDevicesMute| setting.
inline constexpr char kAudioMute[] = "settings.audio.mute";

// A pref holding the value of the policy used to disable playing audio on
// ChromeOS devices. This pref overrides |kAudioMute| but does not overwrite
// it, therefore when the policy is lifted the original mute state is restored.
inline constexpr char kAudioOutputAllowed[] = "hardware.audio_output_enabled";

// A dictionary pref that maps stable device id string to |AudioDeviceState|.
// Different state values indicate whether or not a device has been selected
// as the active one for audio I/O, or it's a new plugged device.
inline constexpr char kAudioDevicesState[] = "settings.audio.device_state";

// A dictionary maps each input device to a unique natural number
// representing the user preference priority among all.
// E.g {(0x9a, 1), (0xab, 2), (0xbc, 3), (0xcd, 4)}
inline constexpr char kAudioInputDevicesUserPriority[] =
    "settings.audio.input_user_priority";

// A dictionary maps each input device to a unique natural number
// representing the user preference priority among all.
// E.g {(0x9a, 1), (0xab, 2), (0xbc, 3), (0xcd, 4)}
inline constexpr char kAudioOutputDevicesUserPriority[] =
    "settings.audio.output_user_priority";

// A dictionary pref that maps a set of input devices to the user-preferred
// device among this set. E.g {(0xab,0xbc -> 0xbc), (0xab,0xbc,0xcd -> 0xbc),
// (0xab,0xbc,0xcd,0xde -> 0xbc)}.
inline constexpr char kAudioInputDevicePreferenceSet[] =
    "settings.audio.input_preference_set";

// A dictionary pref that maps a set of output devices to the user-preferred
// device among this set. E.g {(0xab,0xbc -> 0xbc), (0xab,0xbc,0xcd -> 0xbc),
// (0xab,0xbc,0xcd,0xde -> 0xbc)}.
inline constexpr char kAudioOutputDevicePreferenceSet[] =
    "settings.audio.output_preference_set";

// A list pref containing most recently activated input device id list, most
// recent in the back of the list. E.g {0xab,0xbc,0xcd}.
inline constexpr char kAudioMostRecentActivatedInputDeviceIds[] =
    "settings.audio.most_recent_activated_input_device_ids";

// A list pref containing most recently activated output devices id list, most
// recent in the back of the list. E.g {0xab,0xbc,0xcd}.
inline constexpr char kAudioMostRecentActivatedOutputDeviceIds[] =
    "settings.audio.most_recent_activated_output_device_ids";

// A dictionary pref that maps device id string to the timestamp of the last
// time the audio device was connected, in
// `base::Time::InSecondsFSinceUnixEpoch()`'s format.
inline constexpr char kAudioDevicesLastSeen[] = "settings.audio.last_seen";

// A string pref storing an identifier that is getting sent with parental
// consent in EDU account addition flow.
inline constexpr char kEduCoexistenceId[] =
    "account_manager.edu_coexistence_id";

// A string pref containing valid version of Edu Coexistence Terms of Service.
// Controlled by EduCoexistenceToSVersion policy.
inline constexpr char kEduCoexistenceToSVersion[] =
    "family_link_user.edu_coexistence_tos_version";

// A dictionary pref that associates the secondary edu accounts gaia id string
// with the corresponding accepted Edu Coexistence Terms of Service version
// number.
inline constexpr char kEduCoexistenceToSAcceptedVersion[] =
    "family_link_user.edu_coexistence_tos_accepted_version";

// A boolean pref indicating if a notification about On Device App Controls
// has been shown to the user already.
inline constexpr char kOnDeviceAppControlsNotificationShown[] =
    "on_device_app_controls.notification_shown";

// A string pref that stores the PIN used to unlock parental app controls.
inline constexpr char kOnDeviceAppControlsPin[] = "on_device_app_controls.pin";

// A boolean pref indicating if a PIN has been set up for on-device apps
// parental controls.
inline constexpr char kOnDeviceAppControlsSetupCompleted[] =
    "on_device_app_controls.setup_completed";

// A list pref containing details of the apps blocked by on device controls.
inline constexpr char kOnDeviceAppControlsBlockedApps[] =
    "on_device_app_controls.blocked_apps";

// A boolean pref indicating whether welcome page should be skipped in
// in-session 'Add account' flow.
inline constexpr char kShouldSkipInlineLoginWelcomePage[] =
    "should_skip_inline_login_welcome_page";

// A dictionary of info for Quirks Client/Server interaction, mostly last server
// request times, keyed to display product_id's.
inline constexpr char kQuirksClientLastServerCheck[] =
    "quirks_client.last_server_check";

// Whether 802.11r Fast BSS Transition is currently enabled.
inline constexpr char kDeviceWiFiFastTransitionEnabled[] =
    "net.device_wifi_fast_transition_enabled";

// A boolean pref indicating whether hotspot has been used before.
inline constexpr char kHasHotspotUsedBefore[] = "ash.hotspot.has_used_before";

// A boolean pref that controls whether input noise cancellation is enabled.
inline constexpr char kInputNoiseCancellationEnabled[] =
    "ash.input_noise_cancellation_enabled";

// A boolean pref that controls whether input style transfer is enabled.
inline constexpr char kInputStyleTransferEnabled[] =
    "ash.input_style_transfer_enabled";

// The name of an integer pref that counts the number of times we have shown
// the multitask menu education nudge.
inline constexpr char kMultitaskMenuNudgeClamshellShownCount[] =
    "ash.wm_nudge.multitask_nudge_count";
inline constexpr char kMultitaskMenuNudgeTabletShownCount[] =
    "cros.wm_nudge.tablet_multitask_nudge_count";

// The name of a time pref that stores the time we last showed the multitask
// menu education nudge.
inline constexpr char kMultitaskMenuNudgeClamshellLastShown[] =
    "ash.wm_nudge.multitask_menu_nudge_last_shown";
inline constexpr char kMultitaskMenuNudgeTabletLastShown[] =
    "cros.wm_nudge.tablet_multitask_nudge_last_shown";

// The following SAML-related prefs are not settings that the domain admin can
// set, but information that the SAML Identity Provider can send us:

// A time pref - when the SAML password was last set, according to the SAML IdP.
inline constexpr char kSamlPasswordModifiedTime[] =
    "saml.password_modified_time";
// A time pref - when the SAML password did expire, or will expire, according to
// the SAML IDP.
inline constexpr char kSamlPasswordExpirationTime[] =
    "saml.password_expiration_time";
// A string pref - the URL where the user can update their password, according
// to the SAML IdP.
inline constexpr char kSamlPasswordChangeUrl[] = "saml.password_change_url";

// A dictionary pref that stores custom accelerators that overrides the default
// system-provided accelerators.
inline constexpr char kShortcutCustomizationOverrides[] =
    "accelerator.overrides";

// Boolean pref indicating whether the user has completed (or skipped) the
// out-of-box experience (OOBE) sync consent screen. Before this pref is set
// both OS and browser sync will be disabled. After this pref is set it's
// possible to check sync state to see if the user enabled it.
inline constexpr char kSyncOobeCompleted[] = "sync.oobe_completed";

// A string representing the last version of Chrome that System Web Apps were
// updated for.
inline constexpr char kSystemWebAppLastUpdateVersion[] =
    "web_apps.system_web_app_last_update";

// A string representing the last locale that System Web Apps were installed in.
// This is used to refresh System Web Apps i18n when the locale is changed.
inline constexpr char kSystemWebAppLastInstalledLocale[] =
    "web_apps.system_web_app_last_installed_language";

// An int representing the number of failures to install SWAs for a given
// version & locale pair. After 3 failures, we'll abandon this version to avoid
// bootlooping, and wait for a new version to come along.
inline constexpr char kSystemWebAppInstallFailureCount[] =
    "web_apps.system_web_app_failure_count";

// A string representing the latest Chrome version where an attempt was made
// to install. In the case of success, this and LastUpdateVersion will be the
// same. If there is an installation failure, they will diverge until a
// successful installation is made.
inline constexpr char kSystemWebAppLastAttemptedVersion[] =
    "web_apps.system_web_app_last_attempted_update";

// A string representing the most recent locale that was attempted to be
// installed. In the case of success, this and LastUpdateVersion will be the
// same. If there is an installation failure, they will diverge until a
// successful installation is made.
inline constexpr char kSystemWebAppLastAttemptedLocale[] =
    "web_apps.system_web_app_last_attempted_language";

// Boolean pref indicating whether a user has enabled the display password
// button on the login/lock screen.
inline constexpr char kLoginDisplayPasswordButtonEnabled[] =
    "login_display_password_button_enabled";

// A boolean user profile pref which indicates that the current Managed Guest
// Session is lockable. Set by the chrome.login extension API and read by
// `UserManager`.
inline constexpr char kLoginExtensionApiCanLockManagedGuestSession[] =
    "extensions_api.login.can_lock_managed_guest_session";

// Boolean pref indicating whether the user has enabled Suggested Content in
// OS settings > Privacy > "Suggest new content to explore".
inline constexpr char kSuggestedContentEnabled[] =
    "settings.suggested_content_enabled";

// Boolean value indicating the user has hidden the launcher continue section
// (usually because they want more visual space available for apps).
inline constexpr char kLauncherContinueSectionHidden[] =
    "launcher.continue_section_hidden";

// Boolean value that indicates that the user has given feedback for removing
// items from the continue section.
inline constexpr char kLauncherFeedbackOnContinueSectionSent[] =
    "ash.launcher.continue_section_removal_feedback_sent";

// A time pref indicating the last time a request was made to update the
// Continue section.
inline constexpr char kLauncherLastContinueRequestTime[] =
    "launcher.last_continue_request_time";

// Boolean pref recording whether a search result has ever been launched from
// the Chrome OS launcher.
inline constexpr char kLauncherResultEverLaunched[] =
    "launcher.result_ever_launched";

// A dictionary pref that determines if each user-facing category result should
// show in launcher.
inline constexpr char kLauncherSearchCategoryControlStatus[] =
    "launcher.search_category_control_status";

// A time pref indicating the last time a local file scan is logged in launcher
// search.
inline constexpr char kLauncherSearchLastFileScanLogTime[] =
    "launcher.search_last_file_scan_log_time";

// Dictionary pref to store data on the distribution of provider relevance
// scores for the launcher normalizer.
inline constexpr char kLauncherSearchNormalizerParameters[] =
    "launcher.search_normalizer_parameters";

// Whether or not to use a long delay for Continue section requests.
inline constexpr char kLauncherUseLongContinueDelay[] =
    "launcher.use_long_continue_delay";

// Boolean pref indicating whether system-wide trace collection using the
// Perfetto system tracing service is allowed.
inline constexpr char kDeviceSystemWideTracingEnabled[] =
    "device_system_wide_tracing_enabled";

// A boolean pref which determines whether the large cursor feature is enabled.
inline constexpr char kAccessibilityLargeCursorEnabled[] =
    "settings.a11y.large_cursor_enabled";
// An integer pref that specifies the size of large cursor for accessibility.
inline constexpr char kAccessibilityLargeCursorDipSize[] =
    "settings.a11y.large_cursor_dip_size";
// A boolean pref which determines whether the sticky keys feature is enabled.
inline constexpr char kAccessibilityStickyKeysEnabled[] =
    "settings.a11y.sticky_keys_enabled";
// A boolean pref which determines whether spoken feedback is enabled.
inline constexpr char kAccessibilitySpokenFeedbackEnabled[] =
    "settings.accessibility";
// A boolean pref which determines whether automatic reading for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxAutoRead[] =
    "settings.a11y.chromevox.auto_read";
// A boolean pref which determines whether announce download notifications for
// ChromeVox is enabled.
inline constexpr char kAccessibilityChromeVoxAnnounceDownloadNotifications[] =
    "settings.a11y.chromevox.announce_download_notifications";
// A boolean pref which determines whether announce rich text attributes for
// ChromeVox is enabled.
inline constexpr char kAccessibilityChromeVoxAnnounceRichTextAttributes[] =
    "settings.a11y.chromevox.announce_rich_text_attributes";
// A string pref which holds the current audio strategy for ChromeVox.
inline constexpr char kAccessibilityChromeVoxAudioStrategy[] =
    "settings.a11y.chromevox.audio_strategy";
// A boolean pref which determines whether braille side by side for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxBrailleSideBySide[] =
    "settings.a11y.chromevox.braille_side_by_side";
// A string pref which holds the current braille table for ChromeVox.
inline constexpr char kAccessibilityChromeVoxBrailleTable[] =
    "settings.a11y.chromevox.braille_table";
// A string pref which holds the current 6-dot braille for ChromeVox.
inline constexpr char kAccessibilityChromeVoxBrailleTable6[] =
    "settings.a11y.chromevox.braille_table_6";
// A string pref which holds the current 8-dot braille for ChromeVox.
inline constexpr char kAccessibilityChromeVoxBrailleTable8[] =
    "settings.a11y.chromevox.braille_table_8";
// A string pref which holds the current braille table type for ChromeVox.
inline constexpr char kAccessibilityChromeVoxBrailleTableType[] =
    "settings.a11y.chromevox.braille_table_type";
// A boolean pref which determines whether braille word wrap for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxBrailleWordWrap[] =
    "settings.a11y.chromevox.braille_word_wrap";
// A string pref which holds the current capital strategy for ChromeVox.
inline constexpr char kAccessibilityChromeVoxCapitalStrategy[] =
    "settings.a11y.chromevox.capital_strategy";
// A string pref which holds the current capital strategy backup for ChromeVox.
inline constexpr char kAccessibilityChromeVoxCapitalStrategyBackup[] =
    "settings.a11y.chromevox.capital_strategy_backup";
// A boolean pref which determines whether braille logging for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxEnableBrailleLogging[] =
    "settings.a11y.chromevox.enable_braille_logging";
// A boolean pref which determines whether earcon logging for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxEnableEarconLogging[] =
    "settings.a11y.chromevox.enable_earcon_logging";
// A boolean pref which determines whether event stream logging for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxEnableEventStreamLogging[] =
    "settings.a11y.chromevox.enable_event_stream_logging";
// A boolean pref which determines whether speech logging for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxEnableSpeechLogging[] =
    "settings.a11y.chromevox.enable_speech_logging";
// A dictionary pref that defines which event stream filters in ChromeVox are
// enabled. (e.g. {clicked: true, focus: false, hover: true})
inline constexpr char kAccessibilityChromeVoxEventStreamFilters[] =
    "settings.a11y.chromevox.event_stream_filters";
// A boolean pref which determines whether language switching for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxLanguageSwitching[] =
    "settings.a11y.chromevox.language_switching";
// A boolean pref which determines whether menu braille commands for ChromeVox
// are enabled.
inline constexpr char kAccessibilityChromeVoxMenuBrailleCommands[] =
    "settings.a11y.chromevox.menu_braille_commands";
// A string pref which holds the current number reading style for ChromeVox.
inline constexpr char kAccessibilityChromeVoxNumberReadingStyle[] =
    "settings.a11y.chromevox.number_reading_style";
// A string pref which holds the current preferred braille display address for
// ChromeVox.
inline constexpr char kAccessibilityChromeVoxPreferredBrailleDisplayAddress[] =
    "settings.a11y.chromevox.preferred_braille_display_address";
// An integer pref which holds the value for the current state of punctuation
// echo for ChromeVox. (0 = None, 1 = Some, 2 = All)
inline constexpr char kAccessibilityChromeVoxPunctuationEcho[] =
    "settings.a11y.chromevox.punctuation_echo";
// A boolean pref which determines whether smart sticky mode for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxSmartStickyMode[] =
    "settings.a11y.chromevox.smart_sticky_mode";
// A boolean pref which determines whether speak text under mouse for ChromeVox
// is enabled.
inline constexpr char kAccessibilityChromeVoxSpeakTextUnderMouse[] =
    "settings.a11y.chromevox.speak_text_under_mouse";
// A boolean pref which determines whether use pitch changes for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxUsePitchChanges[] =
    "settings.a11y.chromevox.use_pitch_changes";
// A boolean pref which determines whether use verbose mode for ChromeVox is
// enabled.
inline constexpr char kAccessibilityChromeVoxUseVerboseMode[] =
    "settings.a11y.chromevox.use_verbose_mode";
// An integer pref which holds the value for the number of virtual braille
// columns for ChromeVox.
inline constexpr char kAccessibilityChromeVoxVirtualBrailleColumns[] =
    "settings.a11y.chromevox.virtual_braille_columns";
// An integer pref which holds the value for the number of virtual braille rows
// for ChromeVox.
inline constexpr char kAccessibilityChromeVoxVirtualBrailleRows[] =
    "settings.a11y.chromevox.virtual_braille_rows";
// A string pref which holds the current voice name for ChromeVox.
inline constexpr char kAccessibilityChromeVoxVoiceName[] =
    "settings.a11y.chromevox.voice_name";
// A boolean pref which determines whether the disable trackpad feature is
// enabled.
inline constexpr char kAccessibilityDisableTrackpadEnabled[] =
    "settings.a11y.disable_trackpad_enabled";
// An integer pref which determines the mode of the disabled internal
// trackpad. Values are from the ash::kDisableTrackpadMode enum.
inline constexpr char kAccessibilityDisableTrackpadMode[] =
    "settings.a11y.disable_trackpad_mode";
// A boolean pref which determines whether high contrast is enabled.
inline constexpr char kAccessibilityHighContrastEnabled[] =
    "settings.a11y.high_contrast_enabled";
// A boolean pref which determines whether screen magnifier is enabled.
// NOTE: We previously had prefs named settings.a11y.screen_magnifier_type and
// settings.a11y.screen_magnifier_type2, but we only shipped one type (full).
// See http://crbug.com/170850 for history.
inline constexpr char kAccessibilityScreenMagnifierEnabled[] =
    "settings.a11y.screen_magnifier";
// A boolean pref which determines whether focus following for screen magnifier
// is enabled.
inline constexpr char kAccessibilityScreenMagnifierFocusFollowingEnabled[] =
    "settings.a11y.screen_magnifier_focus_following";
// A boolean pref which determines whether the magnifiers should follow
// ChromeVox focus changes.
inline constexpr char kAccessibilityMagnifierFollowsChromeVox[] =
    "settings.a11y.screen_magnifier_chromevox_focus_following";
// A boolean pref which determines whether the magnifiers should follow
// select to speak focus changes.
inline constexpr char kAccessibilityMagnifierFollowsSts[] =
    "settings.a11y.screen_magnifier_select_to_speak_focus_following";
// An integer pref which indicates the mouse following mode for screen
// magnifier. This maps to AccessibilityController::MagnifierMouseFollowingMode.
inline constexpr char kAccessibilityScreenMagnifierMouseFollowingMode[] =
    "settings.a11y.screen_magnifier_mouse_following_mode";
// A double pref which determines a zooming scale of the screen magnifier.
inline constexpr char kAccessibilityScreenMagnifierScale[] =
    "settings.a11y.screen_magnifier_scale";
// A boolean pref which determines whether the virtual keyboard is enabled for
// accessibility.  This feature is separate from displaying an onscreen keyboard
// due to lack of a physical keyboard.
inline constexpr char kAccessibilityVirtualKeyboardEnabled[] =
    "settings.a11y.virtual_keyboard";
// A pref that identifies which kind of features are enabled for the Web Kiosk
// session.
inline constexpr char kAccessibilityVirtualKeyboardFeatures[] =
    "settings.a11y.virtual_keyboard_features";
// A boolean pref which determines whether the mono audio output is enabled for
// accessibility.
inline constexpr char kAccessibilityMonoAudioEnabled[] =
    "settings.a11y.mono_audio";
// A boolean pref which determines if mouse keys are enabled.
inline constexpr char kAccessibilityMouseKeysEnabled[] =
    "settings.a11y.mouse_keys.enabled";
// A double pref which determines the acceleration of mouse keys as a scale
// factor ranging from 0-1 (0%-100%) inclusive.
inline constexpr char kAccessibilityMouseKeysAcceleration[] =
    "settings.a11y.mouse_keys.acceleration";
// A double pref which determines the max speed of mouse keys as a factor of the
// minimum speed.
inline constexpr char kAccessibilityMouseKeysMaxSpeed[] =
    "settings.a11y.mouse_keys.max_speed";
// A boolean pref which determines if you use mouse keys with the primary keys
// or the num pad.
inline constexpr char kAccessibilityMouseKeysUsePrimaryKeys[] =
    "settings.a11y.mouse_keys.use_primary_keys";
// An integer pref which determines if mouse keys uses left or right hand keys.
// Values are from the ash::MouseKeysDominantHand enum.
inline constexpr char kAccessibilityMouseKeysDominantHand[] =
    "settings.a11y.mouse_keys.dominant_hand";
// A boolean pref which determines whether autoclick is enabled.
inline constexpr char kAccessibilityAutoclickEnabled[] =
    "settings.a11y.autoclick";
// A boolean pref which determines whether the accessibility shortcuts are
// enabled or not.
inline constexpr char kAccessibilityShortcutsEnabled[] =
    "settings.a11y.shortcuts_enabled";
// An integer pref which determines time in ms between when the mouse cursor
// stops and when an autoclick event is triggered.
inline constexpr char kAccessibilityAutoclickDelayMs[] =
    "settings.a11y.autoclick_delay_ms";
// An integer pref which determines the event type for an autoclick event. This
// maps to AccessibilityController::AutoclickEventType.
inline constexpr char kAccessibilityAutoclickEventType[] =
    "settings.a11y.autoclick_event_type";
// Whether Autoclick should immediately return to left click after performing
// another event type action, or whether it should stay as the other event type.
inline constexpr char kAccessibilityAutoclickRevertToLeftClick[] =
    "settings.a11y.autoclick_revert_to_left_click";
// Whether Autoclick should stabilize the cursor movement before a click occurs
// or not.
inline constexpr char kAccessibilityAutoclickStabilizePosition[] =
    "settings.a11y.autoclick_stabilize_position";
// The default threshold of mouse movement, measured in DIP, that will initiate
// a new autoclick.
inline constexpr char kAccessibilityAutoclickMovementThreshold[] =
    "settings.a11y.autoclick_movement_threshold";
// The Autoclick menu position on the screen, an AutoclickMenuPosition.
inline constexpr char kAccessibilityAutoclickMenuPosition[] =
    "settings.a11y.autoclick_menu_position";
// Whether to enable color filtering settings.
inline constexpr char kAccessibilityColorCorrectionEnabled[] =
    "settings.a11y.color_filtering.enabled";
// Whether color filtering has been set up yet. It should be set up on first
// use.
inline constexpr char kAccessibilityColorCorrectionHasBeenSetup[] =
    "settings.a11y.color_filtering.setup";
// The amount of a color vision correction filter to apply.
inline constexpr char kAccessibilityColorVisionCorrectionAmount[] =
    "settings.a11y.color_filtering.color_vision_correction_amount";
// The type of color vision correction to apply.
inline constexpr char kAccessibilityColorVisionCorrectionType[] =
    "settings.a11y.color_filtering.color_vision_deficiency_type";
// A boolean pref which determines whether caret highlighting is enabled.
inline constexpr char kAccessibilityCaretHighlightEnabled[] =
    "settings.a11y.caret_highlight";
// A TimeDelta pref which stores the user's preferred caret blink interval. If
// 0, the caret will not blink.
inline constexpr char kAccessibilityCaretBlinkInterval[] =
    "settings.a11y.caret.blink_interval";
// A boolean pref which determines whether cursor highlighting is enabled.
inline constexpr char kAccessibilityCursorHighlightEnabled[] =
    "settings.a11y.cursor_highlight";
// A boolean pref which determines whether custom cursor color is enabled.
inline constexpr char kAccessibilityCursorColorEnabled[] =
    "settings.a11y.cursor_color_enabled";
// An integer pref which determines the custom cursor color.
inline constexpr char kAccessibilityCursorColor[] =
    "settings.a11y.cursor_color";
// A boolean pref which determines whether flash screen for notifications is
// enabled.
inline constexpr char kAccessibilityFlashNotificationsEnabled[] =
    "settings.a11y.flash_notifications_enabled";
// An integer pref which determines the flash screen color.
inline constexpr char kAccessibilityFlashNotificationsColor[] =
    "settings.a11y.flash_notifications_color";
// A boolean pref which determines whether floating accessibility menu is
// enabled.
inline constexpr char kAccessibilityFloatingMenuEnabled[] =
    "settings.a11y.floating_menu";
// Floating a11y menu position, a FloatingMenuPosition;
inline constexpr char kAccessibilityFloatingMenuPosition[] =
    "settings.a11y.floating_menu_position";
// A boolean pref which determines whether focus highlighting is enabled.
inline constexpr char kAccessibilityFocusHighlightEnabled[] =
    "settings.a11y.focus_highlight";
// A boolean pref which determines whether Select-to-speak is enabled.
inline constexpr char kAccessibilitySelectToSpeakEnabled[] =
    "settings.a11y.select_to_speak";

// A boolean pref which indicates whether the Select to Speak confirmation
// dialog has ever been accepted.
inline constexpr char kSelectToSpeakAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.select_to_speak_accelerator_dialog_has_been_accepted";

// A boolean pref which determines whether Switch Access is enabled.
inline constexpr char kAccessibilitySwitchAccessEnabled[] =
    "settings.a11y.switch_access.enabled";
// A dictionary pref keyed on a key code mapped to a list value of device types
// for the "select" action.
inline constexpr char kAccessibilitySwitchAccessSelectDeviceKeyCodes[] =
    "settings.a11y.switch_access.select.device_key_codes";
// A dictionary pref keyed on a key code mapped to a list value of device types
// for the "next" action.
inline constexpr char kAccessibilitySwitchAccessNextDeviceKeyCodes[] =
    "settings.a11y.switch_access.next.device_key_codes";
// A dictionary pref keyed on a key code mapped to a list value of device types
// for the "previous" action.
inline constexpr char kAccessibilitySwitchAccessPreviousDeviceKeyCodes[] =
    "settings.a11y.switch_access.previous.device_key_codes";
// A boolean pref which determines whether auto-scanning is enabled within
// Switch Access.
inline constexpr char kAccessibilitySwitchAccessAutoScanEnabled[] =
    "settings.a11y.switch_access.auto_scan.enabled";
// An integer pref which determines time delay in ms before automatically
// scanning forward (when auto-scan is enabled).
inline constexpr char kAccessibilitySwitchAccessAutoScanSpeedMs[] =
    "settings.a11y.switch_access.auto_scan.speed_ms";
// An integer pref which determines time delay in ms before automatically
// scanning forward while navigating the keyboard (when auto-scan is
// enabled).
inline constexpr char kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs[] =
    "settings.a11y.switch_access.auto_scan.keyboard.speed_ms";
// An integer pref which determines speed in dips per second that the gliding
// point scan cursor in switch access moves across the screen.
inline constexpr char kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond[] =
    "settings.a11y.switch_access.point_scan.speed_dips_per_second";
// A boolean pref which, if set, indicates that shelf navigation buttons (home,
// back and overview button) should be shown in tablet mode. Note that shelf
// buttons might be shown even if the pref value is false - for example, if
// spoken feedback, autoclick or switch access are enabled.
inline constexpr char kAccessibilityTabletModeShelfNavigationButtonsEnabled[] =
    "settings.a11y.tablet_mode_shelf_nav_buttons_enabled";
// A boolean pref which determines whether dictation is enabled.
inline constexpr char kAccessibilityDictationEnabled[] =
    "settings.a11y.dictation";
// A string pref which determines the locale used for dictation speech
// recognition. Should be BCP-47 format, e.g. "en-US" or "es-ES".
inline constexpr char kAccessibilityDictationLocale[] =
    "settings.a11y.dictation_locale";
// A dictionary pref which keeps track of which locales the user has seen an
// offline dictation upgrade nudge. A nudge will be shown once whenever a
// new language becomes available offline in the background, without repeating
// showing nudges where the language was already available. A locale code will
// map to a value of true if the nudge has been shown, false if it needs to be
// shown upon download completion, and will be absent from the map otherwise.
// Locales match kAccessibilityDictationLocale and are in BCP-47 format.
inline constexpr char kAccessibilityDictationLocaleOfflineNudge[] =
    "settings.a11y.dictation_locale_offline_nudge";
// A boolean pref which determines whether the enhanced network voices feature
// in Select-to-speak is allowed. This pref can only be set by policy.
inline constexpr char
    kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed[] =
        "settings.a11y.enhanced_network_voices_in_select_to_speak_allowed";

// A boolean pref which determines whether Select-to-speak shades the background
// contents that aren't being read.
inline constexpr char kAccessibilitySelectToSpeakBackgroundShading[] =
    "settings.a11y.select_to_speak_background_shading";

// A boolean pref which determines whether enhanced network TTS voices are
// enabled for Select-to-speak.
inline constexpr char kAccessibilitySelectToSpeakEnhancedNetworkVoices[] =
    "settings.a11y.select_to_speak_enhanced_network_voices";

// A string pref which determines the user's preferred enhanced voice for
// Select-to-speak.
inline constexpr char kAccessibilitySelectToSpeakEnhancedVoiceName[] =
    "settings.a11y.select_to_speak_enhanced_voice_name";

// A boolean pref which determines whether the initial popup authorizing
// enhanced network voices for Select-to-speak has been shown to the user.
inline constexpr char kAccessibilitySelectToSpeakEnhancedVoicesDialogShown[] =
    "settings.a11y.select_to_speak_enhanced_voices_dialog_shown";

// A string pref which determines the user's word highlighting color preference
// for Select-to-speak, stored as a hex color string. (e.g. "#ae003f")
inline constexpr char kAccessibilitySelectToSpeakHighlightColor[] =
    "settings.a11y.select_to_speak_highlight_color";

// A boolean pref which determines whether Select-to-speak shows navigation
// controls that allow the user to navigate to next/previous sentences,
// paragraphs, and more.
inline constexpr char kAccessibilitySelectToSpeakNavigationControls[] =
    "settings.a11y.select_to_speak_navigation_controls";

// A string pref which determines the user's preferred voice for
// Select-to-speak.
inline constexpr char kAccessibilitySelectToSpeakVoiceName[] =
    "settings.a11y.select_to_speak_voice_name";

// A boolean pref which determines whether Select-to-speak enables automatic
// voice switching between different languages.
inline constexpr char kAccessibilitySelectToSpeakVoiceSwitching[] =
    "settings.a11y.select_to_speak_voice_switching";

// A boolean pref which determines whether Select-to-speak highlights each word
// as it is read.
inline constexpr char kAccessibilitySelectToSpeakWordHighlight[] =
    "settings.a11y.select_to_speak_word_highlight";

// A boolean pref which determines whether ReducedAnimations is enabled.
inline constexpr char kAccessibilityReducedAnimationsEnabled[] =
    "settings.a11y.reduced_animations.enabled";

// A boolean pref which determines whether FaceGaze is enabled.
inline constexpr char kAccessibilityFaceGazeEnabled[] =
    "settings.a11y.face_gaze.enabled";
// An integer pref which scales the cursor speed when moving up.
inline constexpr char kAccessibilityFaceGazeCursorSpeedUp[] =
    "settings.a11y.face_gaze.cursor_speed_up";
// An integer pref which scales the cursor speed when moving down.
inline constexpr char kAccessibilityFaceGazeCursorSpeedDown[] =
    "settings.a11y.face_gaze.cursor_speed_down";
// An integer pref which scales the cursor speed when moving left.
inline constexpr char kAccessibilityFaceGazeCursorSpeedLeft[] =
    "settings.a11y.face_gaze.cursor_speed_left";
// An integer pref which scales the cursor speed when moving right.
inline constexpr char kAccessibilityFaceGazeCursorSpeedRight[] =
    "settings.a11y.face_gaze.cursor_speed_right";
// An integer pref which determines how much FaceGaze should smooth cursor
// movements.
inline constexpr char kAccessibilityFaceGazeCursorSmoothing[] =
    "settings.a11y.face_gaze.cursor_smoothing";
// A boolean pref which determines whether FaceGaze should use cursor
// acceleration, which makes the cursor move faster when the tracked point moves
// quickly.
inline constexpr char kAccessibilityFaceGazeCursorUseAcceleration[] =
    "settings.a11y.face_gaze.cursor_use_acceleration";
// A dictionary pref which maps facial gestures to key combinations defined by
// the user. Facial gestures are strings like 'browDownLeft', and key
// combinations are stringified JSON objects that define the key combination.
inline constexpr char kAccessibilityFaceGazeGesturesToKeyCombos[] =
    "settings.a11y.face_gaze.gestures_to_key_combos";
// A dictionary pref which maps facial gestures to action macros. Facial
// gestures are strings like 'browDownLeft', and actions are enums from
// macro_names.
inline constexpr char kAccessibilityFaceGazeGesturesToMacros[] =
    "settings.a11y.face_gaze.gestures_to_macros";
// A dictionary pref which maps facial gestures to confidence thresholds. Facial
// gestures are strings like 'browDownLeft', and confidence thresholds are
// integers between 0 and 100 representing percentages.
inline constexpr char kAccessibilityFaceGazeGesturesToConfidence[] =
    "settings.a11y.face_gaze.gestures_to_confidence";
// A boolean pref which controls whether FaceGaze controls the cursor position.
inline constexpr char kAccessibilityFaceGazeCursorControlEnabled[] =
    "settings.a11y.face_gaze.cursor_control_enabled";
// A boolean pref which controls whether FaceGaze can execute actions.
inline constexpr char kAccessibilityFaceGazeActionsEnabled[] =
    "settings.a11y.face_gaze.actions_enabled";
// A boolean pref which controls whether users can adjust the cursor speed
// separately or whether speeds in all directions should be adjusted together.
inline constexpr char kAccessibilityFaceGazeAdjustSpeedSeparately[] =
    "settings.a11y.face_gaze.adjust_speed_separately";
// A boolean pref which indicates whether the FaceGaze confirmation dialog has
// ever been accepted.
inline constexpr char kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.face_gaze.accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the FaceGaze DLC success notification
// has ever been shown.
inline constexpr char kFaceGazeDlcSuccessNotificationHasBeenShown[] =
    "settings.a11y.face_gaze.dlc_success_notification_has_been_shown";
// A boolean pref which indicates whether the FaceGaze DLC failure notification
// has ever been shown.
inline constexpr char kFaceGazeDlcFailureNotificationHasBeenShown[] =
    "settings.a11y.face_gaze.dlc_failure_notification_has_been_shown";
// An integer pref which determines the FaceGaze cursor controller movement
// threshold.
inline constexpr char kAccessibilityFaceGazeVelocityThreshold[] =
    "settings.a11y.face_gaze.velocity_threshold";

// A boolean pref which determines whether the accessibility menu shows
// regardless of the state of a11y features.
inline constexpr char kShouldAlwaysShowAccessibilityMenu[] =
    "settings.a11y.enable_menu";

// A boolean pref which determines whether alt-tab should show only windows in
// the current desk or all windows.
inline constexpr char kAltTabPerDesk[] = "ash.alttab.per_desk";

// A dictionary storing the number of times and most recent time all contextual
// tooltips have been shown.
inline constexpr char kContextualTooltips[] =
    "settings.contextual_tooltip.shown_info";

// A list containing the stored virtual desks names in the same order of the
// desks in the overview desks bar. This list will be used to restore the desks,
// their order, and their names for the primary user on first signin. If a desk
// hasn't been renamed by the user (i.e. it uses one of the default
// automatically-assigned desk names such as "Desk 1", "Desk 2", ... etc.), its
// name will appear in this list as an empty string. The desk names are stored
// as UTF8 strings.
inline constexpr char kDesksNamesList[] = "ash.desks.desks_names_list";
// A list containing the stored virtual desks guids in the same order of the
// desks in the overview desks bar. This list will be used to restore desk guids
// for the primary user on first sign-in. The guids are stored as lowercase
// strings.
inline constexpr char kDesksGuidsList[] = "ash.desks.desks_guids_list";
// A list containing the lacros profile ID associations for desks in the same
// order of the desks in the overview desks bar. This is used so that desk <->
// profile associations can be restored. The profile IDs are logically unsigned
// integers, but stored as strings since they can (and will) be 64-bits large.
inline constexpr char kDesksLacrosProfileIdList[] =
    "ash.desks.desks_lacros_profile_id_list";
// This list stores the metrics of virtual desks. Like |kDesksNamesList|, this
// list stores entries in the same order of the desks in the overview desks bar.
// Values are stored as dictionaries.
inline constexpr char kDesksMetricsList[] = "ash.desks.desks_metrics_list";
// A dict pref storing the metrics related to the weekly active desks of a user.
inline constexpr char kDesksWeeklyActiveDesksMetrics[] =
    "ash.desks.weekly_active_desks";
// An integer index of a user's active desk.
inline constexpr char kDesksActiveDesk[] = "ash.desks.active_desk";

// A boolean pref storing the enabled status of the Docked Magnifier feature.
inline constexpr char kDockedMagnifierEnabled[] =
    "ash.docked_magnifier.enabled";
// A double pref storing the scale value of the Docked Magnifier feature by
// which the screen is magnified.
inline constexpr char kDockedMagnifierScale[] = "ash.docked_magnifier.scale";
// A double pref storing the screen height divisor value of the Docked Magnifier
// feature defining what proportion of the screen the docked magnifier viewport
// occupies.
inline constexpr char kDockedMagnifierScreenHeightDivisor[] =
    "ash.docked_magnifier.screen_height_divisor";

// A boolean pref which indicates whether the docked magnifier confirmation
// dialog has ever been shown.
inline constexpr char kDockedMagnifierAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.docked_magnifier_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the high contrast magnifier
// confirmation dialog has ever been shown.
inline constexpr char kHighContrastAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.high_contrast_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the screen magnifier confirmation
// dialog has ever been shown.
inline constexpr char kScreenMagnifierAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.screen_magnifier_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the dictation confirmation dialog has
// ever been shown.
inline constexpr char kDictationAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.dictation_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the Dictation DLC success notification
// has ever been shown.
inline constexpr char kDictationDlcSuccessNotificationHasBeenShown[] =
    "settings.a11y.dictation_dlc_success_notification_has_been_shown";
// A boolean pref which indicates whether the Dictation DLC only Pumpkin
// downloaded notification has ever been shown.
inline constexpr char
    kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown[] =
        "settings.a11y.dictation_dlc_only_pumpkin_downloaded_notification_has_"
        "been_"
        "shown";
// A boolean pref which indicates whether the Dictation DLC only SODA
// downloaded notification has ever been shown.
inline constexpr char
    kDictationDlcOnlySodaDownloadedNotificationHasBeenShown[] =
        "settings.a11y.dictation_dlc_only_soda_downloaded_notification_has_"
        "been_"
        "shown";
// A boolean pref which indicates whether the Dictation No DLCs downloaded
// notification has ever been shown.
inline constexpr char kDictationNoDlcsDownloadedNotificationHasBeenShown[] =
    "settings.a11y.dictation_dlc_no_dlcs_downloaded_notification_has_been_"
    "shown";

// A boolean pref which indicates whether the display rotation confirmation
// dialog has ever been shown.
// Renamed 10/2019 to force reset the pref to false.
inline constexpr char kDisplayRotationAcceleratorDialogHasBeenAccepted2[] =
    "settings.a11y.display_rotation_accelerator_dialog_has_been_accepted2";

// A dictionary pref that stores the mixed mirror mode parameters.
inline constexpr char kDisplayMixedMirrorModeParams[] =
    "settings.display.mixed_mirror_mode_param";
// Power state of the current displays from the last run.
inline constexpr char kDisplayPowerState[] = "settings.display.power_state";
// A dictionary pref that stores per display preferences.
inline constexpr char kDisplayProperties[] = "settings.display.properties";
// Boolean controlling whether privacy screen is enabled.
inline constexpr char kDisplayPrivacyScreenEnabled[] =
    "settings.display.privacy_screen_enabled";
// A dictionary pref that specifies the state of the rotation lock, and the
// display orientation, for the internal display.
inline constexpr char kDisplayRotationLock[] = "settings.display.rotation_lock";
// A dictionary pref that stores the touch associations for the device.
inline constexpr char kDisplayTouchAssociations[] =
    "settings.display.touch_associations";
// A dictionary pref that stores the port mapping for touch devices.
inline constexpr char kDisplayTouchPortAssociations[] =
    "settings.display.port_associations";
// A list pref that stores the mirror info for each external display.
inline constexpr char kExternalDisplayMirrorInfo[] =
    "settings.display.external_display_mirror_info";
// A dictionary pref that specifies per-display layout/offset information.
// Its key is the ID of the display and its value is a dictionary for the
// layout/offset information.
inline constexpr char kSecondaryDisplays[] =
    "settings.display.secondary_displays";
// A boolean pref which determines whether the display configuration set by
// managed guest session should be stored in local state.
inline constexpr char kAllowMGSToStoreDisplayProperties[] =
    "settings.display.allow_mgs_to_store";

// A list of all displays used by the user and reported to popularity metrics.
const char kDisplayPopularityUserReportedDisplays[] =
    "display_popularity.user_reported_displays";
// A list of all displays used by the user and reported to popularity metrics.
const char kDisplayPopularityRevNumber[] = "display_popularity.revision_number";

// A double pref which stores the current screen brightness of the internal
// display.
const char kInternalDisplayScreenBrightnessPercent[] =
    "settings.display.internal_screen_brightness_percent";

// A boolean pref which stores whether the ambient light sensor is enabled for
// the internal display.
inline constexpr char kDisplayAmbientLightSensorEnabled[] =
    "settings.display.ambient_light_sensor_enabled";

// A boolean pref which stores whether the ambient light sensor is enabled for
// the internal display. This is a synced profile pref that stores the most
// recent value for a given user.
inline constexpr char kDisplayAmbientLightSensorLastEnabled[] =
    "settings.display.ambient_light_sensor_last_enabled";

// An integer pref which stores the reason that the ambient light sensor was
// disabled. This pref can be used if we need to systematically re-enable the
// ambient light sensor for a subset of users (e.g. those who didn't manually
// disable the sensor from the Settings app).
// Values are from `power_manager::AmbientLightSensorChange_Cause`.
inline constexpr char kAmbientLightSensorDisabledReason[] =
    "settings.display.ambient_light_sensor_disabled_reason";

// A double pref which stores the current keyboard brightness.
const char kKeyboardBrightnessPercent[] =
    "settings.keyboard.brightness_percent";

// A boolean pref which stores whether the keyboard ambient light sensor is
// enabled.
inline constexpr char kKeyboardAmbientLightSensorEnabled[] =
    "settings.keyboard.ambient_light_sensor_enabled";

// A boolean pref which stores whether the keyboard ambient light sensor is
// enabled. This is a synced profile pref that stores the most recent value for
// a given user.
inline constexpr char kKeyboardAmbientLightSensorLastEnabled[] =
    "settings.keyboard.ambient_light_sensor_last_enabled";

// An integer pref which stores the reason that the keyboard ambient light
// sensor was disabled. This pref can be used if we need to systematically
// re-enable the keyboard ambient light sensor for a subset of users (e.g. those
// who didn't manually disable the sensor from the Settings app). Values are
// from `power_manager::AmbientLightSensorChange_Cause`.
inline constexpr char kKeyboardAmbientLightSensorDisabledReason[] =
    "settings.keyboard.ambient_light_sensor_disabled_reason";

// A boolean pref that enable fullscreen alert bubble.
// TODO(zxdan): Change to an allowlist in M89.
inline constexpr char kFullscreenAlertEnabled[] =
    "ash.fullscreen_alert_enabled";

// A boolean pref which stores whether a stylus has been seen before.
inline constexpr char kHasSeenStylus[] = "ash.has_seen_stylus";
// A boolean pref which stores whether a the palette warm welcome bubble
// (displayed when a user first uses a stylus) has been shown before.
inline constexpr char kShownPaletteWelcomeBubble[] =
    "ash.shown_palette_welcome_bubble";
// A boolean pref that specifies if the stylus tools should be enabled/disabled.
inline constexpr char kEnableStylusTools[] = "settings.enable_stylus_tools";
// A boolean pref that specifies if the ash palette should be launched after an
// eject input event has been received.
inline constexpr char kLaunchPaletteOnEjectEvent[] =
    "settings.launch_palette_on_eject_event";

// Boolean pref indicating whether the PCI tunneling is allowed for external
// Thunderbolt/USB4 peripherals. This pref is only used if the policy
// "DevicePciPeripheralDataAccessEnabled" is set to "unset".
inline constexpr char kLocalStateDevicePeripheralDataAccessEnabled[] =
    "settings.local_state_device_pci_data_access_enabled";

// The timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of the user
// pressed the shutdown button from shelf.
// static
inline constexpr char kLoginShutdownTimestampPrefName[] =
    "ash.shelf.login_shutdown_timestamp";

// The number of times we have shown the informed restore education nudge.
inline constexpr char kInformedRestoreNudgeShownCount[] =
    "ash.wm.informed_restore_nudge_shown_count";

// The last time we showed the informed restore education nudge.
inline constexpr char kInformedRestoreNudgeLastShown[] =
    "ash.wm.informed_restore_nudge_last_shown";

// A `TimeDelta` pref indicates the length of time for taking the informed
// restore screenshot on shutdown. Pine is the old name for informed restore.
inline constexpr char kInformedRestoreScreenshotTakenDuration[] =
    "ash.pine.screenshot_taken_duration";

// A `TimeDelta` pref indicates the length of time for encoding and writing the
// informed restore screenshot to the disk. Pine is the old name for informed
// restore.
inline constexpr char kInformedRestoreScreenshotEncodeAndSaveDuration[] =
    "ash.pine.sreenshot_encode_and_save_duration";

// A boolean pref indicating whether the informed restore onboarding dialog
// should be shown. Pine is the old name for informed restore.
inline constexpr char kShowInformedRestoreOnboarding[] =
    "ash.pine.should_show_informed_restore_onboarding";

// A string pref that stores the last software version. Used to notify users if
// there has been an update.
inline constexpr char kInformedRestoreLastVersion[] =
    "ash.wm.informed_restore_last_version";

// A boolean pref that specifies if the cellular setup notification can be
// shown or not. This notification should be shown post-OOBE if the user has a
// cellular-capable device but no available cellular networks. It should only be
// shown at most once per user.
inline constexpr char kCanCellularSetupNotificationBeShown[] =
    "ash.cellular_setup.can_setup_notification_be_shown";

// Boolean pref indicating whether the privacy warning of the managed-guest
// session on both; the login screen and inside the auto-launched session,
// should be displayed or not.
inline constexpr char kManagedGuestSessionPrivacyWarningsEnabled[] =
    "managed_session.privacy_warning_enabled";

// Boolean pref indicating whether the user has enabled detection of snooping
// over their shoulder, and hiding of notifications when a snooper is detected.
inline constexpr char kSnoopingProtectionEnabled[] =
    "ash.privacy.snooping_protection_enabled";
inline constexpr char kSnoopingProtectionNotificationSuppressionEnabled[] =
    "ash.privacy.snooping_protection_notification_suppression_enabled";

// A string pref storing the type of lock screen notification mode.
// "show" -> show notifications on the lock screen
// "hide" -> hide notifications at all on the lock screen (default)
// "hideSensitive" -> hide sensitive content on the lock screen
// (other values are treated as "hide")
inline constexpr char kMessageCenterLockScreenMode[] =
    "ash.message_center.lock_screen_mode";

// Value of each options of the lock screen notification settings. They are
// used the pref of ash::prefs::kMessageCenterLockScreenMode.
inline constexpr char kMessageCenterLockScreenModeShow[] = "show";
inline constexpr char kMessageCenterLockScreenModeHide[] = "hide";
inline constexpr char kMessageCenterLockScreenModeHideSensitive[] =
    "hideSensitive";

// A boolean pref storing the enabled status of the ambient color feature.
inline constexpr char kAmbientColorEnabled[] = "ash.ambient_color.enabled";

// A boolean pref that indicates whether dark mode is enabled.
inline constexpr char kDarkModeEnabled[] = "ash.dark_mode.enabled";

// An integer pref that indicates the color scheme used to calculate the dynamic
// color palette.
inline constexpr char kDynamicColorColorScheme[] =
    "ash.dynamic_color.color_scheme";

// A uint64 pref that indicates the seed color used to calculate the dynamic
// color palette. It is an ARGB 32-bit unsigned integer stored as a uint64.
inline constexpr char kDynamicColorSeedColor[] = "ash.dynamic_color.seed_color";

// A boolean pref that indicates whether to use the k means color calculation
// for the seed color. This pref cannot be set by users -- it will be used to
// slowly migrate existing users to new dynamic colors.
inline constexpr char kDynamicColorUseKMeans[] =
    "ash.dynamic_color.use_k_means";

// An integer pref storing the type of automatic scheduling of turning on and
// off the dark mode feature similar to `kNightLightScheduleType`, but
// custom scheduling (2) is the same as sunset to sunrise scheduling (1)
// because dark mode does not support custom scheduling.
inline constexpr char kDarkModeScheduleType[] = "ash.dark_mode.schedule_type";

// A boolean pref storing the enabled status of the NightLight feature.
inline constexpr char kNightLightEnabled[] = "ash.night_light.enabled";

// A double pref storing the screen color temperature set by the NightLight
// feature. The expected values are in the range of 0.0 (least warm) and 1.0
// (most warm).
inline constexpr char kNightLightTemperature[] =
    "ash.night_light.color_temperature";

// An integer pref storing the type of automatic scheduling of turning on and
// off the NightLight feature. Valid values are:
// 0 -> NightLight is never turned on or off automatically.
// 1 -> NightLight is turned on and off at the sunset and sunrise times
// respectively.
// 2 -> NightLight schedule times are explicitly set by the user.
//
// See ash::ScheduleType.
inline constexpr char kNightLightScheduleType[] =
    "ash.night_light.schedule_type";

// Integer prefs storing the start and end times of the automatic schedule at
// which NightLight turns on and off respectively when the schedule type is set
// to a custom schedule. The times are represented as the number of minutes from
// 00:00 (12:00 AM) regardless of the date or the timezone.
// See ash::TimeOfDayTime.
inline constexpr char kNightLightCustomStartTime[] =
    "ash.night_light.custom_start_time";
inline constexpr char kNightLightCustomEndTime[] =
    "ash.night_light.custom_end_time";

// A boolean pref storing whether the AutoNightLight notification has ever been
// dismissed by the user, which we use to stop showing it again.
inline constexpr char kAutoNightLightNotificationDismissed[] =
    "ash.auto_night_light.notification_dismissed";

// Whether the Chrome OS lock screen is allowed.
inline constexpr char kAllowScreenLock[] = "allow_screen_lock";

// A boolean pref that turns on automatic screen locking.
inline constexpr char kEnableAutoScreenLock[] = "settings.enable_screen_lock";

// Screen brightness percent values to be used when running on AC power.
// Specified by the policy.
inline constexpr char kPowerAcScreenBrightnessPercent[] =
    "power.ac_screen_brightness_percent";

// Inactivity time in milliseconds while the system is on AC power before
// the screen should be dimmed, turned off, or locked, before an
// IdleActionImminent D-Bus signal should be sent, or before
// kPowerAcIdleAction should be performed.  0 disables the delay (N/A for
// kPowerAcIdleDelayMs).
inline constexpr char kPowerAcScreenDimDelayMs[] =
    "power.ac_screen_dim_delay_ms";
inline constexpr char kPowerAcScreenOffDelayMs[] =
    "power.ac_screen_off_delay_ms";
inline constexpr char kPowerAcScreenLockDelayMs[] =
    "power.ac_screen_lock_delay_ms";
inline constexpr char kPowerAcIdleWarningDelayMs[] =
    "power.ac_idle_warning_delay_ms";

// Boolean pref of whether adaptive charging (i.e. holding battery at a sub-100%
// charge until necessary to extend battery life) is enabled.
inline constexpr char kPowerAdaptiveChargingEnabled[] =
    "power.adaptive_charging_enabled";
// Boolean pref of whether adaptive charging educational nudge is shown to the
// user.
inline constexpr char kPowerAdaptiveChargingNudgeShown[] =
    "power.adaptive_charging_nudge_shown";

// Boolean pref for if ChromeOS battery saver is active.
inline constexpr char kPowerBatterySaver[] = "power.cros_battery_saver_active";

// Pref recording the charge percent while battery saver is enabled, used to
// detect charging during sleep or when shut down.
inline constexpr char kPowerBatterySaverPercent[] =
    "power.cros_battery_saver_percent";

// Screen brightness percent values to be used when running on battery power.
// Specified by the policy.
inline constexpr char kPowerBatteryScreenBrightnessPercent[] =
    "power.battery_screen_brightness_percent";

// Similar delays while the system is on battery power.
inline constexpr char kPowerBatteryScreenDimDelayMs[] =
    "power.battery_screen_dim_delay_ms";
inline constexpr char kPowerBatteryScreenOffDelayMs[] =
    "power.battery_screen_off_delay_ms";
inline constexpr char kPowerBatteryScreenLockDelayMs[] =
    "power.battery_screen_lock_delay_ms";
inline constexpr char kPowerBatteryIdleWarningDelayMs[] =
    "power.battery_idle_warning_delay_ms";
inline constexpr char kPowerBatteryIdleDelayMs[] =
    "power.battery_idle_delay_ms";
inline constexpr char kPowerAcIdleDelayMs[] = "power.ac_idle_delay_ms";

// Inactivity delays used to dim the screen or turn it off while the screen is
// locked.
inline constexpr char kPowerLockScreenDimDelayMs[] =
    "power.lock_screen_dim_delay_ms";
inline constexpr char kPowerLockScreenOffDelayMs[] =
    "power.lock_screen_off_delay_ms";

// Action that should be performed when the idle delay is reached while the
// system is on AC power or battery power.
// Values are from the chromeos::PowerPolicyController::Action enum.
inline constexpr char kPowerAcIdleAction[] = "power.ac_idle_action";
inline constexpr char kPowerBatteryIdleAction[] = "power.battery_idle_action";

// Action that should be performed when the lid is closed.
// Values are from the chromeos::PowerPolicyController::Action enum.
inline constexpr char kPowerLidClosedAction[] = "power.lid_closed_action";

// Should audio and video activity be used to disable the above delays?
inline constexpr char kPowerUseAudioActivity[] = "power.use_audio_activity";
inline constexpr char kPowerUseVideoActivity[] = "power.use_video_activity";

// Should extensions, ARC apps, and other code within Chrome be able to override
// system power management (preventing automatic actions like sleeping, locking,
// or screen dimming)?
inline constexpr char kPowerAllowWakeLocks[] = "power.allow_wake_locks";

// Should extensions, ARC apps, and other code within Chrome be able to override
// display-related power management? (Disallowing wake locks in general takes
// precedence over this.)
inline constexpr char kPowerAllowScreenWakeLocks[] =
    "power.allow_screen_wake_locks";

// Amount by which the screen-dim delay should be scaled while the system
// is in presentation mode. Values are limited to a minimum of 1.0.
inline constexpr char kPowerPresentationScreenDimDelayFactor[] =
    "power.presentation_screen_dim_delay_factor";

// Amount by which the screen-dim delay should be scaled when user activity is
// observed while the screen is dimmed or soon after the screen has been turned
// off.  Values are limited to a minimum of 1.0.
inline constexpr char kPowerUserActivityScreenDimDelayFactor[] =
    "power.user_activity_screen_dim_delay_factor";

// Whether the power management delays should start running only after the first
// user activity has been observed in a session.
inline constexpr char kPowerWaitForInitialUserActivity[] =
    "power.wait_for_initial_user_activity";

// Boolean controlling whether the panel backlight should be forced to a
// nonzero level when user activity is observed.
inline constexpr char kPowerForceNonzeroBrightnessForUserActivity[] =
    "power.force_nonzero_brightness_for_user_activity";

// Boolean controlling whether a shorter suspend delay should be used after the
// user forces the display off by pressing the power button. Provided to allow
// policy to control this behavior.
inline constexpr char kPowerFastSuspendWhenBacklightsForcedOff[] =
    "power.fast_suspend_when_backlights_forced_off";

// Boolean controlling whether smart dim model is enabled.
inline constexpr char kPowerSmartDimEnabled[] = "power.smart_dim_enabled";

// Boolean controlling whether ALS logging is enabled.
inline constexpr char kPowerAlsLoggingEnabled[] = "power.als_logging_enabled";

// Boolean controlling whether quick dim is enabled.
inline constexpr char kPowerQuickDimEnabled[] = "power.quick_dim_enabled";

// Quick lock delay is used inside powerd to control the delay time for a screen
// lock to happen if the user is detected to be absent.
inline constexpr char kPowerQuickLockDelay[] = "power.quick_lock_delay.ms";

// A `TimeDelta` pref for the duration when the critical notification is
// displayed to when no outcome has occurred yet. Updates every 15 seconds.
inline constexpr char kCriticalStateDuration[] =
    "ash.power_notification.critical_state_duration";

// A boolean pref that reflects the value of the policy
// DeviceEphemeralNetworkPoliciesEnabled.
inline constexpr char kDeviceEphemeralNetworkPoliciesEnabled[] =
    "ash.network.device_ephemeral_network_policies_enabled";

// Copy of the `proxy_config::prefs::kProxy` definition; available at compile
// time.
inline constexpr char kProxy[] = "proxy";

// Boolean controlling whether the settings is enabled. This pref is intended to
// be set only by policy not by user.
inline constexpr char kOsSettingsEnabled[] = "os_settings_enabled";

// |kShelfAlignment| and |kShelfAutoHideBehavior| have a local variant. The
// local variant is not synced and is used if set. If the local variant is not
// set its value is set from the synced value (once prefs have been
// synced). This gives a per-machine setting that is initialized from the last
// set value.
// These values are default on the machine but can be overridden by per-display
// values in kShelfPreferences (unless overridden by managed policy).
// String value corresponding to ash::ShelfAlignment (e.g. "Bottom").
inline constexpr char kShelfAlignment[] = "shelf_alignment";
inline constexpr char kShelfAlignmentLocal[] = "shelf_alignment_local";
// String value corresponding to ash::ShelfAutoHideBehavior (e.g. "Never").
inline constexpr char kShelfAutoHideBehavior[] = "auto_hide_behavior";
inline constexpr char kShelfAutoHideBehaviorLocal[] =
    "auto_hide_behavior_local";
inline constexpr char kShelfAutoHideTabletModeBehavior[] =
    "auto_hide_tablet_mode_behavior";
inline constexpr char kShelfAutoHideTabletModeBehaviorLocal[] =
    "auto_hide_tablet_mode_behavior_local";

// Dictionary value that determines when the launcher navigation nudge should
// show to the users.
inline constexpr char kShelfLauncherNudge[] = "ash.shelf.launcher_nudge";

// Dictionary value that holds per-display preference of shelf alignment and
// auto-hide behavior. Key of the dictionary is the id of the display, and
// its value is a dictionary whose keys are kShelfAlignment and
// kShelfAutoHideBehavior.
inline constexpr char kShelfPreferences[] = "shelf_preferences";

// String pref indicating that the user has manually chosen to show or hide the
// desk button.
inline constexpr char kShowDeskButtonInShelf[] = "show_desk_button_in_shelf";

// Boolean pref indicating that a virtual desk (other than the default desk)
// has been used on this device at any point in time after the addition of this
// pref.
inline constexpr char kDeviceUsesDesks[] = "device_uses_desks";

// Boolean pref indicating whether to show a logout button in the system tray.
inline constexpr char kShowLogoutButtonInTray[] = "show_logout_button_in_tray";

// Integer pref indicating the length of time in milliseconds for which a
// confirmation dialog should be shown when the user presses the logout button.
// A value of 0 indicates that logout should happen immediately, without showing
// a confirmation dialog.
inline constexpr char kLogoutDialogDurationMs[] = "logout_dialog_duration_ms";

// A boolean pref that when set to true, displays the logout confirmation
// dialog. If set to false, it prevents showing the dialog and the subsequent
// logout after closing the last window.
inline constexpr char kSuggestLogoutAfterClosingLastWindow[] =
    "suggest_logout_after_closing_last_window";

// A dictionary pref that maps usernames to wallpaper info.
inline constexpr char kUserWallpaperInfo[] = "user_wallpaper_info";

// An ordered list of hashed representations of IDs of Google Photos recently
// used as wallpapers for Daily Refresh.
inline constexpr char kRecentDailyGooglePhotosWallpapers[] =
    "recent_daily_google_photos_wallpapers";

// A dictionary pref that maps usernames to versioned wallpaper info.
// This is for wallpapers that are syncable across devices.
inline constexpr char kSyncableVersionedWallpaperInfo[] =
    "syncable_versioned_wallpaper_info";

// A dictionary pref that maps usernames to wallpaper info.
// This is for wallpapers that are syncable across devices. It is being replaced
// by `kSyncableVersionedWallpaperInfo`. Data from this pref will be migrated to
// the new pref.
inline constexpr char kSyncableWallpaperInfo[] = "syncable_wallpaper_info";

// A dictionary pref that maps wallpaper file paths to their prominent colors.
inline constexpr char kWallpaperColors[] = "ash.wallpaper.prominent_colors";

// A dictionary pref that maps wallpaper file paths to their k mean colors.
inline constexpr char kWallpaperMeanColors[] = "ash.wallpaper.k_mean_colors";

// A dictionary pref that maps wallpaper file paths to their celebi colors.
inline constexpr char kWallpaperCelebiColors[] = "ash.wallpaper.celebi_colors";

// A boolean pref used to initiate the wallpaper daily refresh scheduled
// feature. Unlike other scheduled features, the value is unimportant and should
// NOT be used to determine whether daily refresh mode is enabled. The change in
// this pref's value is used as a signal to check whether the user's wallpaper
// should be refreshed. Even though there are 2 changes per day, only one change
// (0->1) is meant to result in the update of the wallpaper. The other
// change is meant to be a retry in case this change fails.
inline constexpr char kWallpaperDailyRefreshCheck[] =
    "ash.wallpaper_daily_refresh.check";

// An integer pref storing the type of automatic scheduling of wallpaper daily
// refresh mode. The value will always be 2, which indicates that this scheduled
// feature runs on a custom schedule.
inline constexpr char kWallpaperDailyRefreshScheduleType[] =
    "ash.wallpaper_daily_refresh.schedule_type";

// Integer prefs storing the primary and secondary check times of the wallpaper
// daily refresh mode. The times are represented as the number of minutes from
// 00:00 (12:00 AM) regardless of the date or the timezone.
inline constexpr char kWallpaperDailyRefreshFirstCheckTime[] =
    "ash.wallpaper_daily_refresh.first_check_time";
inline constexpr char kWallpaperDailyRefreshSecondCheckTime[] =
    "ash.wallpaper_daily_refresh.second_check_time";

// Prefs required by `ScheduledFeature` for the time of day wallpaper to follow
// a sunset-to-sunrise schedule. Nothing in the system ultimately uses them.
// TODO(b/309020921): Remove these once ScheduledFeature doesn't require prefs
// to operate.
inline constexpr char kWallpaperTimeOfDayStatus[] =
    "ash.wallpaper_time_of_day.status";
inline constexpr char kWallpaperTimeOfDayScheduleType[] =
    "ash.wallpaper_time_of_day.schedule_type";

// Integer pref that tracks whether user SeaPen images have been moved to
// cryptohome. SeaPen initially stored images in the global wallpaper directory,
// but now only stores a copy of the active (currently set as wallpaper) image
// in global wallpaper directory. All other SeaPen images are in cryptohome so
// that they are encrypted upon logout.
// @see SeaPenWallpaperManager::MigrationStatus enum.
inline constexpr char kWallpaperSeaPenMigrationStatus[] =
    "ash.wallpaper.sea_pen.migration_status";

// Boolean pref indicating whether a user has enabled the bluetooth adapter.
inline constexpr char kUserBluetoothAdapterEnabled[] =
    "ash.user.bluetooth.adapter_enabled";

// Boolean pref indicating system-wide setting for bluetooth adapter power.
inline constexpr char kSystemBluetoothAdapterEnabled[] =
    "ash.system.bluetooth.adapter_enabled";

// An integer pref counting the number of times bluetooth connection toast has
// been displayed.
inline constexpr char kBluetoothConnectionToastShownCount[] =
    "ash.bluetooth.connection_toast_shown_count";

// A time pref storing the start time for counting the number of times the
// bluetooth connection toast has been displayed.
inline constexpr char kBluetoothToastCountStartTime[] =
    "ash.bluetooth.toast_count_start_time";

// A boolean pref indicating whether the camera is allowed to be used.
inline constexpr char kUserCameraAllowed[] = "ash.user.camera_allowed";

// A boolean pref remembering the previous value of `kUserCameraAllowed`.
// Set to ensure we can restore the previous value (even after a crash) when the
// preference is temporary changed through the `ForceDisableCameraAccess` API.
inline constexpr char kUserCameraAllowedPreviousValue[] =
    "ash.user.camera_allowed_previous_value";

// A boolean pref indicating whether the microphone is allowed to be used.
inline constexpr char kUserMicrophoneAllowed[] = "ash.user.microphone_allowed";

// A boolean pref indicating whether a user has enabled the speak-on-mute
// detection.
inline constexpr char kUserSpeakOnMuteDetectionEnabled[] =
    "ash.user.speak_on_mute_detection_enabled";
// A boolean pref indicating whether a speak-on-mute detection opt-in nudge
// should be displayed to the user.
inline constexpr char kShouldShowSpeakOnMuteOptInNudge[] =
    "ash.user.should_show_speak_on_mute_opt_in_nudge";
// An integer pref counting the number of times speak-on-mute detection opt-in
// nudge has been displayed to the user.
inline constexpr char kSpeakOnMuteOptInNudgeShownCount[] =
    "ash.user.speak_on_mute_opt_in_nudge_shown_count";

// An enum pref, indicating whether the geolocation is allowed inside user
// session. Values are from `ash::GeolocationAccessLevel`.
inline constexpr char kUserGeolocationAccessLevel[] =
    "ash.user.geolocation_access_level";
// An enum pref indicating whether the geolocation is allowed outside user
// session. Values are from `ash::GeolocationAccessLevel`.
inline constexpr char kDeviceGeolocationAllowed[] =
    "ash.device.geolocation_allowed";
// A pref that saves the previous geolocation access level.
// It has to be different value than the current level saved in
// `kUserGeolocationAccessLevel`.
inline constexpr char kUserPreviousGeolocationAccessLevel[] =
    "ash.user.previous_geolocation_access_level";

// A boolean pref indicating whether a user has enabled the improve location
// toggle.
inline constexpr char kUserGeolocationAccuracyEnabled[] =
    "ash.user.geolocation_accuracy_enabled";

// Double prefs storing the most recent valid geoposition, which is only used
// when the device lacks connectivity and we're unable to retrieve a valid
// geoposition to calculate the sunset / sunrise times.
//
// Note the night light feature will be migrated to use `GeolocationController`
// eventually, at which time `kNightLightCachedLatitude|Longitude` will be
// superseded by these prefs.
inline constexpr char kDeviceGeolocationCachedLatitude[] =
    "ash.device.cached_latitude";
inline constexpr char kDeviceGeolocationCachedLongitude[] =
    "ash.device.cached_longitude";

// A boolean pref which determines whether tap-dragging is enabled.
inline constexpr char kTapDraggingEnabled[] =
    "settings.touchpad.enable_tap_dragging";

// Boolean prefs for the status of the touchscreen and the touchpad.
inline constexpr char kTouchpadEnabled[] = "events.touch_pad.enabled";
inline constexpr char kTouchscreenEnabled[] = "events.touch_screen.enabled";

// Boolean value indicating that the ai intro screen should be shown to the
// user during oobe common or add person flow.
inline constexpr char kShowAiIntroScreenEnabled[] =
    "ash.ai_intro_screen_oobe_enabled";

// Boolean value indicating that the gemini intro screen should be shown to the
// user during oobe common or add person flow.
inline constexpr char kShowGeminiIntroScreenEnabled[] =
    "ash.gemini_intro_screen_oobe_enabled";

// Boolean value indicating that the touchpad scroll direction screen should be
// shown to the user during oobe.
inline constexpr char kShowTouchpadScrollScreenEnabled[] =
    "ash.touchpad_scroll_screen_oobe_enabled";

// Boolean value indicating that the human presence sesnsor screen should be
// shown to the user during oobe.
inline constexpr char kShowHumanPresenceSensorScreenEnabled[] =
    "ash.human_presence_sensor_scren_oobe_enabled";

// Boolean value indicating that the Display size screen should be
// shown to the user during the first sign-in.
inline constexpr char kShowDisplaySizeScreenEnabled[] =
    "ash.display_size_screen_oobe_enabled";

// Integer prefs indicating the minimum and maximum lengths of the lock screen
// pin.
inline constexpr char kPinUnlockMaximumLength[] = "pin_unlock_maximum_length";
inline constexpr char kPinUnlockMinimumLength[] = "pin_unlock_minimum_length";

// Boolean pref indicating whether users are allowed to set easy pins.
inline constexpr char kPinUnlockWeakPinsAllowed[] =
    "pin_unlock_weak_pins_allowed";

// An integer pref. Indicates the number of fingerprint records registered.
inline constexpr char kQuickUnlockFingerprintRecord[] =
    "quick_unlock.fingerprint.record";

// A list of allowed quick unlock modes. A quick unlock mode can only be used if
// its type is on this list, or if type all (all quick unlock modes enabled) is
// on this list.
inline constexpr char kQuickUnlockModeAllowlist[] =
    "quick_unlock_mode_allowlist";

// A list of allowed WebAuthn factors. A WebAuthn factor can only be
// used if its type is on this list, or if type all (all WebAuthn factors
// enabled) is on this list.
inline constexpr char kWebAuthnFactors[] = "authfactors.restrictions.webauthn";

// String pref storing the salt for the pin quick unlock mechanism.
inline constexpr char kQuickUnlockPinSalt[] = "quick_unlock.pin.salt";

// The hash for the pin quick unlock mechanism.
inline constexpr char kQuickUnlockPinSecret[] = "quick_unlock.pin.secret";

// Counter of failed pin quick unlock attempts.
inline constexpr char kQuickUnlockPinFailedAttempts[] =
    "quick_unlock.pin.failed_attempts";

// Enum that specifies how often a user has to enter their password to continue
// using quick unlock. These values are the same as the ones in
// `quick_unlock::PasswordConfirmationFrequency`.
// 0 - six hours. Users will have to enter their password every six hours.
// 1 - twelve hours. Users will have to enter their password every twelve hours.
// 2 - two days. Users will have to enter their password every two days.
// 3 - week. Users will have to enter their password every week.
inline constexpr char kQuickUnlockTimeout[] = "quick_unlock_timeout";

// Dictionary prefs in local state that keeps information about detachable
// bases - for example the last used base per user.
inline constexpr char kDetachableBaseDevices[] = "ash.detachable_base.devices";

// Pref storing the number of sessions in which Assistant onboarding was shown.
inline constexpr char kAssistantNumSessionsWhereOnboardingShown[] =
    "ash.assistant.num_sessions_where_onboarding_shown";

// Pref storing the time of the last Assistant interaction.
inline constexpr char kAssistantTimeOfLastInteraction[] =
    "ash.assistant.time_of_last_interaction";

// Whether the user is allowed to disconnect and configure VPN connections.
inline constexpr char kVpnConfigAllowed[] = "vpn_config_allowed";

// A boolean pref that indicates whether power peak shift is enabled.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kPowerPeakShiftEnabled[] = "ash.power.peak_shift_enabled";

// An integer pref that specifies the power peak shift battery threshold in
// percent.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kPowerPeakShiftBatteryThreshold[] =
    "ash.power.peak_shift_battery_threshold";

// A dictionary pref that specifies the power peak shift day configs.
// For details see "DevicePowerPeakShiftDayConfig" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kPowerPeakShiftDayConfig[] =
    "ash.power.peak_shift_day_config";

// A boolean pref that indicates whether boot on AC is enabled.
inline constexpr char kBootOnAcEnabled[] = "ash.power.boot_on_ac_enabled";

// A boolean pref that indicates whether advanced battery charge mode is
// enabled.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kAdvancedBatteryChargeModeEnabled[] =
    "ash.power.advanced_battery_charge_mode_enabled";

// A dictionary pref that specifies the advanced battery charge mode day config.
// For details see "DeviceAdvancedBatteryChargeModeDayConfig" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kAdvancedBatteryChargeModeDayConfig[] =
    "ash.power.advanced_battery_charge_mode_day_config";

// An integer pref that specifies the battery charge mode.
// For details see "DeviceBatteryChargeMode" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kBatteryChargeMode[] = "ash.power.battery_charge_mode";

// An integer pref that specifies the battery charge custom start charging in
// percent.
// For details see "DeviceBatteryChargeCustomStartCharging" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kBatteryChargeCustomStartCharging[] =
    "ash.power.battery_charge_custom_start_charging";

// An integer pref that specifies the battery charge custom stop charging in
// percent.
// For details see "DeviceBatteryChargeCustomStopCharging" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kBatteryChargeCustomStopCharging[] =
    "ash.power.battery_charge_custom_stop_charging";

// A boolean pref that indicates whether USB power share is enabled.
// For details see "DeviceUsbPowerShareEnabled" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
inline constexpr char kUsbPowerShareEnabled[] =
    "ash.power.usb_power_share_enabled";

// A bool pref to block the USB-C cable limiting device speed notification if it
// has already been clicked by the user.
inline constexpr char kUsbPeripheralCableSpeedNotificationShown[] =
    "ash.usb_peripheral_cable_speed_notification_shown";

// A dictionary value that determines whether the reorder nudge in app list
// should show to the users.
inline constexpr char kAppListReorderNudge[] =
    "ash.launcher.app_list_reorder_nudge";

// An integer pref that stores the experimental arm that the user belongs to in
// the AppsCollections experiment. Used for metrics.
inline constexpr char kLauncherAppsCollectionsExperimentArm[] =
    "ash.launcher.apps_collections_experimental_arm";

// A dictionary pref that stores information related to the privacy notice in
// the continue files section for the launcher.
inline constexpr char kLauncherFilesPrivacyNotice[] =
    "ash.launcher.continue_section_privacy_notice";

// A boolean pref that indicates whether lock screen media controls are enabled.
// Controlled by user policy.
inline constexpr char kLockScreenMediaControlsEnabled[] =
    "ash.lock_screen_media_controls_enabled";

// Boolean pref which indicates if long press diacritics is in use
inline constexpr char kLongPressDiacriticsEnabled[] =
    "settings.language.physical_keyboard_enable_diacritics_on_longpress";

// Boolean pref which determines whether key repeat is enabled.
inline constexpr char kXkbAutoRepeatEnabled[] =
    "settings.language.xkb_auto_repeat_enabled_r2";

// Integer pref which determines key repeat delay (in ms).
inline constexpr char kXkbAutoRepeatDelay[] =
    "settings.language.xkb_auto_repeat_delay_r2";

// Integer pref which determines key repeat interval (in ms).
inline constexpr char kXkbAutoRepeatInterval[] =
    "settings.language.xkb_auto_repeat_interval_r2";
// "_r2" suffixes were added to the three prefs above when we changed the
// preferences to not be user-configurable or sync with the cloud. The prefs are
// now user-configurable and syncable again, but we don't want to overwrite the
// current values with the old synced values, so we continue to use this suffix.

// A boolean pref that causes top-row keys to be interpreted as function keys
// instead of as media keys.
inline constexpr char kSendFunctionKeys[] =
    "settings.language.send_function_keys";

// A boolean pref that controls the value of the setting "Use the
// launcher/search key to change the behavior of function keys".
inline constexpr char kDeviceSwitchFunctionKeysBehaviorEnabled[] =
    "ash.settings.switch_function_keys_behavior_enabled";

// An enum pref that controls the value of the setting "How can applications
// capture and override the ChromeOS system shortcuts".
inline constexpr char kSystemShortcutBehavior[] =
    "ash.settings.system_shortcut_behavior";

// A string-enum-list pref that controls if the WiFi firmware dump is allowed to
// be included in user feedback report.
inline constexpr char kUserFeedbackWithLowLevelDebugDataAllowed[] =
    "ash.user_feedback_with_low_level_debug_data_allowed";

// A boolean pref which is true if touchpad reverse scroll is enabled.
inline constexpr char kNaturalScroll[] = "settings.touchpad.natural_scroll";
// A boolean pref which is true if mouse reverse scroll is enabled.
inline constexpr char kMouseReverseScroll[] = "settings.mouse.reverse_scroll";

// A time pref storing the last time the multipaste menu was shown.
inline constexpr char kMultipasteMenuLastTimeShown[] =
    "ash.clipboard.multipaste_menu.last_time_shown";

// A dictionary storing the number of times and most recent time the multipaste
// contextual nudge was shown.
inline constexpr char kMultipasteNudges[] = "ash.clipboard.multipaste_nudges";

// A boolean pref that indicates whether app badging is shown in launcher and
// shelf.
inline constexpr char kAppNotificationBadgingEnabled[] =
    "ash.app_notification_badging_enabled";

// A boolean pref for whether Isolated Web Apps are enabled by the OS.
inline constexpr char kIsolatedWebAppsEnabled[] =
    "ash.isolated_web_apps_enabled";

// An integer pref that indicates whether global media controls is pinned to
// shelf or it's unset and need to be determined by screen size during runtime.
inline constexpr char kGlobalMediaControlsPinned[] =
    "ash.system.global_media_controls_pinned";

// An integer pref that tracks how many times the user is able to click on
// PciePeripheral-related notifications before hiding new notifications.
inline constexpr char kPciePeripheralDisplayNotificationRemaining[] =
    "ash.pcie_peripheral_display_notification_remaining";

// Boolean prefs storing whether various IME-related keyboard shortcut reminders
// have previously been dismissed or not.
inline constexpr char kLastUsedImeShortcutReminderDismissed[] =
    "ash.shortcut_reminders.last_used_ime_dismissed";
inline constexpr char kNextImeShortcutReminderDismissed[] =
    "ash.shortcut_reminders.next_ime_dismissed";

// Boolean pref to indicate whether to use i18n shortcut mapping and deprecate
// legacy shortcuts.
inline constexpr char kDeviceI18nShortcutsEnabled[] =
    "ash.device_i18n_shortcuts_enabled";

// If a user installs an extension which controls the proxy settings in the
// primary profile of Chrome OS, this dictionary will contain information about
// the extension controlling the proxy (name, id and if it can be disabled by
// the user). Used to show the name and icon of the extension in the "Proxy"
// section of the OS Settings>Network dialog.
inline constexpr char kLacrosProxyControllingExtension[] =
    "ash.lacros_proxy_controlling_extension";

// A boolean pref which is true if Fast Pair is enabled.
inline constexpr char kFastPairEnabled[] = "ash.fast_pair.enabled";

// Boolean pref indicating a user entered Bluetooth pairing flow from a fast
// pair entry point.
inline constexpr char kUserPairedWithFastPair[] =
    "ash.user.paired_with_fast_pair";

// A list pref that contains predefined automation configured by policy
// administrators.
inline constexpr char kAppLaunchAutomation[] = "ash.app_launch_automation";

// A boolean pref that controls whether the user is allowed to use the Desk
// Templates feature - including creating Desks templates and using predefined
// Desks templates.
inline constexpr char kDeskTemplatesEnabled[] = "ash.desk_templates_enabled";

// A string pref which contains download URLs and hashes for files containing
// predefined Desks templates configured by policy administrators.
inline constexpr char kPreconfiguredDeskTemplates[] =
    "ash.preconfigured_desk_templates";

// An unsigned integer pref which contains the last used marker color for
// Projector.
inline constexpr char kProjectorAnnotatorLastUsedMarkerColor[] =
    "ash.projector.annotator_last_used_marker_color";

// A boolean pref that tracks whether the user has enabled Projector creation
// flow during onboarding.
inline constexpr char kProjectorCreationFlowEnabled[] =
    "ash.projector.creation_flow_enabled";

// A string pref that tracks the language installed for the Projector creation
// flow.
inline constexpr char kProjectorCreationFlowLanguage[] =
    "ash.projector.creation_flow_language";

// An integer pref counting the number of times the Onboarding flow has been
// shown to the user inside the Projector Gallery.
inline constexpr char kProjectorGalleryOnboardingShowCount[] =
    "ash.projector.gallery_onboarding_show_count";

// An integer pref counting the number of times the Onboarding flow has been
// shown to the user inside the Projector Viewer.
inline constexpr char kProjectorViewerOnboardingShowCount[] =
    "ash.projector.viewer_onboarding_show_count";

// A boolean pref that indicates the the exclude-transcript dialog has been
// shown.
inline constexpr char kProjectorExcludeTranscriptDialogShown[] =
    "ash.projector.exclude_transcript_dialog_shown";

// A boolean pref that indicates the Projector has been enabled by admin
// policy.
inline constexpr char kProjectorAllowByPolicy[] =
    "ash.projector.allow_by_policy";

// A boolean pref that controls Projector dogfood for Family Link users.
// Set with an enterprise user policy.
inline constexpr char kProjectorDogfoodForFamilyLinkEnabled[] =
    "ash.projector.dogfood_for_family_link_enabled";

// A boolean pref to keep track that the shelf-pin preferences have been
// migrated to the new app id based on chrome-untrusted://projector.
inline constexpr char kProjectorSWAUIPrefsMigrated[] =
    "ash.projector.swa_ui_prefs_migrated_to_chrome_untrusted";

// List of Drive Folder Shortcuts in the Files app. Used to sync the shortcuts
// across devices.
inline constexpr char kFilesAppFolderShortcuts[] =
    "ash.filesapp.folder_shortcuts";

// A boolean pref that indicates if the Files app UI prefs have migrated from
// the Chrome app to System Web App.
inline constexpr char kFilesAppUIPrefsMigrated[] =
    "ash.filesapp.ui_prefs_migrated";

// A boolean pref that indicates if files can be trashed (on a supported
// filesystem) or must be always permanently deleted.
inline constexpr char kFilesAppTrashEnabled[] = "ash.filesapp.trash_enabled";

// Boolean value for the DeviceLoginScreenWebUILazyLoading device policy.
inline constexpr char kLoginScreenWebUILazyLoading[] =
    "ash.login.LoginScreenWebUILazyLoading";

// Boolean value for the FloatingWorkspaceV2Enabled policy
inline constexpr char kFloatingWorkspaceV2Enabled[] =
    "ash.floating_workspace_v2_enabled";

// Boolean value indicating that post reboot notification should be shown to the
// user.
inline constexpr char kShowPostRebootNotification[] =
    "ash.show_post_reboot_notification";

// Boolean value indicating that the USB device detected notification should be
// shown to the user.
inline constexpr char kUsbDetectorNotificationEnabled[] =
    "ash.usb_detector_notification_enabled";

// This integer pref indicates which color for the backlight keyboard is
// currently selected. A pref with this name is registered in two different
// contexts:
// - User profile:
//   Indicates the color selected by the user for their profile.
//   Can be "recommended" through device policy DeviceKeyboardBacklightColor.
// - Local state:
//   Indicates the color used on the sign-in screen.
//   Can be "recommended" through device policy DeviceKeyboardBacklightColor
//   (but as there is no UI to change the color on the sign-in screen,
//   it's effectively policy-mandated then).
inline constexpr char kPersonalizationKeyboardBacklightColor[] =
    "ash.personalization.keyboard_backlight_color";

// A dictionary pref storing the color of each zone of the RGB keyboard. The key
// specifies the zone .e.g. `zone-1`, `zone-2`, whereas the value is a
// `personalization_app::mojom::BacklightColor`.
inline constexpr char kPersonalizationKeyboardBacklightZoneColors[] =
    "ash.personalization.keyboard_backlight_zone_colors";

// This integer pref indicates the display type of the keyboard backlight color.
// The value is one of `KeyboardBacklightColorController::DisplayType`.
inline constexpr char kPersonalizationKeyboardBacklightColorDisplayType[] =
    "ash.personalization.keyboard_backlight_color_display_type";

// An integer pref used by an admin policy to enable/disable GenAI wallpaper
// feature. See the policy at GenAIWallpaperEnabled.yaml.
inline constexpr char kGenAIWallpaperSettings[] =
    "ash.personalization_app.gen_ai_wallpaper_enterprise_policy_settings";

// An integer pref used by an admin policy to enable/disable GenAI VC Background
// feature. See the policy at GenAIVcBackgroundSettings.yaml.
inline constexpr char kGenAIVcBackgroundSettings[] =
    "ash.vc_background_ui.gen_ai_vc_background_enterprise_policy_settings";

// Integer pref corresponding to the autozoom state, the value should be one of
// cros::mojom::CameraAutoFramingState.
inline constexpr char kAutozoomState[] = "ash.camera.autozoom_state";

// A dictionary storing the number of times and most recent time the autozoom
// nudge was shown.
inline constexpr char kAutozoomNudges[] = "ash.camera.autozoom_nudges";

// Boolean pref to record if the DevTools should be opened with the camera app
// by default.
inline constexpr char kCameraAppDevToolsOpen[] =
    "ash.camera.cca_dev_tools_open";

// A boolean pref that specifies the recovery service activation for user.
// When the pref is set to `true`, the user data recovery is activated. When the
// pref is set to `false`, the user data recovery is not activated. The default
// value of the pref is `true`. Controlled by RecoveryFactorBehavior policy.
inline constexpr char kRecoveryFactorBehavior[] =
    "ash.recovery.recovery_factor_behavior";

// A boolean pref that specifies if the the improved management disclosure
// should be shown on the login/lock screen. When pref is set to true the
// improved management disclosure is shown. Otherwise it is the existing
// disclosure.
inline constexpr char kImprovedManagementDisclosure[] =
    "ash.login.improved_management_disclosure";

// Pref which stores ICCIDs of cellular networks that have been migrated to the
// APN Revamp feature.
inline constexpr char kApnMigratedIccids[] = "ash.cellular.apn_migrated_iccids";

// An integer pref that indicates the background blur level that is applied.
// -1 means disabled.
inline constexpr char kBackgroundBlur[] = "ash.camera.background_blur";

// A boolean pref that indicates whether background replacement is applied.
inline constexpr char kBackgroundReplace[] = "ash.camera.background_replace";

// An string pref that indicates the image path of the camera background.
inline constexpr char kBackgroundImagePath[] =
    "ash.camera.background_image_path";

// A boolean pref that indicates whether portrait relighting is applied.
inline constexpr char kPortraitRelighting[] = "ash.camera.portrait_relighting";

// A boolean pref that indicates whether face retouch is applied.
inline constexpr char kFaceRetouch[] = "ash.camera.face_retouch";

// A boolean pref that indicates whether Studio Look is applied.
inline constexpr char kStudioLook[] = "ash.camera.studio_look";

// Specifies if ARC app sync metrics should be recorded, i.e. this is the
// initial session after sync consent screen.
inline constexpr char kRecordArcAppSyncMetrics[] =
    "ash.should_record_arc_app_sync_metrics";

// A boolean pref set to true if primary mouse button is the left button.
inline constexpr char kPrimaryMouseButtonRight[] =
    "settings.mouse.primary_right";

// A integer pref for the touchpad sensitivity.
inline constexpr char kMouseSensitivity[] = "settings.mouse.sensitivity2";

// A boolean pref set to true if mouse acceleration is enabled. When disabled
// only simple linear scaling is applied based on sensitivity.
inline constexpr char kMouseAcceleration[] = "settings.mouse.acceleration";

// A integer pref for the touchpad scroll sensitivity, in the range
// [PointerSensitivity::kLowest, PointerSensitivity::kHighest].
inline constexpr char kMouseScrollSensitivity[] =
    "settings.mouse.scroll_sensitivity";

// A boolean pref set to true if mouse scroll acceleration is enabled. When
// disabled, only simple linear scaling is applied based on sensitivity.
inline constexpr char kMouseScrollAcceleration[] =
    "settings.mouse.scroll_acceleration";

// A integer pref for the touchpad sensitivity.
inline constexpr char kTouchpadSensitivity[] = "settings.touchpad.sensitivity2";

// A boolean pref set to true if touchpad acceleration is enabled. When
// disabled only simple linear scaling is applied based on sensitivity.
inline constexpr char kTouchpadAcceleration[] =
    "settings.touchpad.acceleration";

// A boolean pref set to true if touchpad three-finger-click is enabled.
inline constexpr char kEnableTouchpadThreeFingerClick[] =
    "settings.touchpad.enable_three_finger_click";

// A boolean pref set to true if touchpad tap-to-click is enabled.
inline constexpr char kTapToClickEnabled[] =
    "settings.touchpad.enable_tap_to_click";

// A integer pref for the touchpad scroll sensitivity, in the range
// [PointerSensitivity::kLowest, PointerSensitivity::kHighest].
inline constexpr char kTouchpadScrollSensitivity[] =
    "settings.touchpad.scroll_sensitivity";

// A boolean pref set to true if touchpad scroll acceleration is enabled. When
// disabled only simple linear scaling is applied based on sensitivity.
inline constexpr char kTouchpadScrollAcceleration[] =
    "settings.touchpad.scroll_acceleration";

// A boolean pref set to true if touchpad haptic feedback is enabled.
inline constexpr char kTouchpadHapticFeedback[] =
    "settings.touchpad.haptic_feedback";

// A integer pref for the touchpad haptic click sensitivity ranging from Soft
// feedback to Firm feedback [1, 3, 5].
inline constexpr char kTouchpadHapticClickSensitivity[] =
    "settings.touchpad.haptic_click_sensitivity";

// A integer pref for pointing stick sensitivity.
inline constexpr char kPointingStickSensitivity[] =
    "settings.pointing_stick.sensitivity";

// A boolean pref set to true if primary pointing stick button is the left
// button.
inline constexpr char kPrimaryPointingStickButtonRight[] =
    "settings.pointing_stick.primary_right";

// A boolean pref set to true if pointing stick acceleration is enabled. When
// disabled only simple linear scaling is applied based on sensitivity.
inline constexpr char kPointingStickAcceleration[] =
    "settings.pointing_stick.acceleration";

// A syncable time pref that stores the time of last session activation.
// Starting in M119, rounded down to the nearest day since Windows epoch to
// reduce syncs.
inline constexpr char kTimeOfLastSessionActivation[] =
    "ash.session.time_of_last_activation";

// Copy of owner swap mouse buttons option to use on login screen.
inline constexpr char kOwnerPrimaryMouseButtonRight[] =
    "owner.mouse.primary_right";

// Copy of the primary pointing stick buttons option to use on login screen.
inline constexpr char kOwnerPrimaryPointingStickButtonRight[] =
    "owner.pointing_stick.primary_right";

// Copy of owner tap-to-click option to use on login screen.
inline constexpr char kOwnerTapToClickEnabled[] =
    "owner.touchpad.enable_tap_to_click";

// An integer pref that is incremented anytime a user simulates a right click
// using their keyboard and touchpad with Alt+Click.
inline constexpr char kAltEventRemappedToRightClick[] =
    "ash.settings.alt_event_remapped_to_right_click";

// An integer pref that is incremented anytime a user simulates a right click
// using their keyboard and touchpad with Search+Click.
inline constexpr char kSearchEventRemappedToRightClick[] =
    "ash.settings.search_event_remapped_to_right_click";

// An integer pref for tracking Alt and Search based key event rewrites for
// the Delete "six pack" key. The value of this pref will be used to set the
// default behavior for remapping a key event to Delete.
// Default setting:
//  Pref contains a positive value: Alt+BackSpace
//  Pref contains a negative value: Search+BackSpace
inline constexpr char kKeyEventRemappedToSixPackDelete[] =
    "ash.settings.key_event_remapped_to_six_pack_delete";

// An integer pref for tracking Alt and Search based key event rewrites for
// the Home "six pack" key. The value of this pref will be used to set the
// default behavior for remapping a key event to Home.
// Default setting:
//  Pref contains a positive value: Control+Alt+Up
//  Pref contains a negative value: Search+Left
inline constexpr char kKeyEventRemappedToSixPackHome[] =
    "ash.settings.key_event_remapped_to_six_pack_home";

// An integer pref for tracking Alt and Search based key event rewrites for
// the End "six pack" key. The value of this pref will be used to set the
// default behavior for remapping a key event to End.
// Default setting:
//  Pref contains a positive value: Control+Alt+Down
//  Pref contains a negative value: Search+Right
inline constexpr char kKeyEventRemappedToSixPackEnd[] =
    "ash.settings.key_event_remapped_to_six_pack_end";

// An integer pref for tracking Alt and Search based key event rewrites for
// the PageUp "six pack" key. The value of this pref will be used to set the
// default behavior for remapping a key event to PageUp.
// Default setting:
//  Pref contains a positive value: Alt+Up
//  Pref contains a negative value: Search+Up
inline constexpr char kKeyEventRemappedToSixPackPageUp[] =
    "ash.settings.key_event_remapped_to_six_pack_page_up";

// An integer pref for tracking Alt and Search based key event rewrites for
// the PageDown "six pack" key. The value of this pref will be used to set the
// default behavior for remapping a key event to PageDown.
// Default setting:
//  Pref contains a positive value: Alt+Down
//  Pref contains a negative value: Search+Down
inline constexpr char kKeyEventRemappedToSixPackPageDown[] =
    "ash.settings.key_event_remapped_to_six_pack_page_down";

// This pref saves the absolute session start time for UMA.
inline constexpr char kAshLoginSessionStartedTime[] =
    "ash.Login.SessionStarted.Time";

// This pref saves the "first user session after user was added to the device"
// flag for UMA.
inline constexpr char kAshLoginSessionStartedIsFirstSession[] =
    "ash.Login.SessionStarted.IsFirstSession";

// A boolean pref that controls whether input force respect ui gains is enabled.
inline constexpr char kInputForceRespectUiGainsEnabled[] =
    "ash.input_force_respect_ui_gains_enabled";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming event would have been remapped to a right
// click but either the user's setting is inconsistent with the matched
// modifier key or remapping to right click is disabled before hiding new
// notifications.
inline constexpr char kRemapToRightClickNotificationsRemaining[] =
    "ash.settings.remap_to_right_click_notifications_remaining";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming key event would have been remapped to the
// Delete "six pack" key but either the user's setting is inconsistent with the
// matched modifier key or using a key combination to simulate the Delete key
// action is disabled.
inline constexpr char kSixPackKeyDeleteNotificationsRemaining[] =
    "ash.settings.delete_six_pack_key_notifications_remaining";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming key event would have been remapped to the
// Home "six pack" key but either the user's setting is inconsistent with the
// matched modifier key or using a key combination to simulate the Home key
// action is disabled.
inline constexpr char kSixPackKeyHomeNotificationsRemaining[] =
    "ash.settings.home_six_pack_key_notifications_remaining";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming key event would have been remapped to the
// End "six pack" key but either the user's setting is inconsistent with the
// matched modifier key or using a key combination to simulate the End key
// action is disabled.
inline constexpr char kSixPackKeyEndNotificationsRemaining[] =
    "ash.settings.end_six_pack_key_notifications_remaining";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming key event would have been remapped to the
// Page Up "six pack" key but either the user's setting is inconsistent with the
// matched modifier key or using a key combination to simulate the Page Up key
// action is disabled.
inline constexpr char kSixPackKeyPageUpNotificationsRemaining[] =
    "ash.settings.page_up_six_pack_key_notifications_remaining";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming key event would have been remapped to the
// Page Down "six pack" key but either the user's setting is inconsistent with
// the the matched modifier key or using a key combination to simulate the Page
// Down key action is disabled.
inline constexpr char kSixPackKeyPageDownNotificationsRemaining[] =
    "ash.settings.page_down_six_pack_key_notifications_remaining";

// An integer pref that tracks how many times (3) we'll show the user a
// notification when an incoming key event would have been remapped to the
// Insert "six pack" key but either the user's setting is inconsistent with the
// matched modifier key or using a key combination to simulate the Insert key
// action is disabled.
inline constexpr char kSixPackKeyInsertNotificationsRemaining[] =
    "ash.settings.insert_six_pack_key_notifications_remaining";

// A boolean pref that controls whether hands-free profile input super
// resolution is enabled.
inline constexpr char kHandsFreeProfileInputSuperResolution[] =
    "ash.hands_free_profile_input_super_resolution";

// A boolean pref used by an admin policy to allow/disallow user to customize
// system shortcut. See the policy at ShortcutCustomizationAllowed.yaml.
inline constexpr char kShortcutCustomizationAllowed[] =
    "ash.shortcut_customization_allowed";

// A dictionary pref used to record the number of times each deprecated
// accelerator notification has shown.
inline constexpr char kDeprecatedAcceleratorNotificationsShownCounts[] =
    "ash.deprecated_accelerator_notifications_shown_counts";
// A dictionary pref used to record the timestamp each deprecated accelerator
// notification last shown.
inline constexpr char kDeprecatedAcceleratorNotificationsLastShown[] =
    "ash.deprecated_accelerator_notifications_last_shown";

// A `TimeDelta` pref for the session duration Focus Mode should default to.
// Based off of the last session, if any.
inline constexpr char kFocusModeSessionDuration[] =
    "ash.focus_mode.session_duration";
// A boolean pref of whether Focus Mode should default to turning on DND. Based
// off of the last session, if any.
inline constexpr char kFocusModeDoNotDisturb[] =
    "ash.focus_mode.do_not_disturb";
// A dictionary pref containing the data pertaining to the task selected by the
// user for a focus session.
inline constexpr char kFocusModeSelectedTask[] = "ash.focus_mode.selected_task";
// A dictionary pref containing the set of sound section settings for the user.
// This is synced for all user devices.
inline constexpr char kFocusModeSoundSection[] = "ash.focus_mode.sound_section";
// An identifier for focus mode clients that is stable across reboot for use
// with the YMC API.
inline constexpr char kFocusModeDeviceId[] = "ash.focus_mode.device_id";
// An string enum pref containing the enabled sound types.
inline constexpr char kFocusModeSoundsEnabled[] =
    "ash.focus_mode.sounds_enabled";
// A boolean pref of whether the focus panel shows the OAuth consent screen for
// YouTube Music.
inline constexpr char kFocusModeYTMDisplayOAuthConsent[] =
    "ash.focus_mode.youtube_music.oauth";
// A boolean pref of whether the focus panel shows the view to let the user know
// a 3 months free trial for YouTube Music.
inline constexpr char kFocusModeYTMDisplayFreeTrial[] =
    "ash.focus_mode.youtube_music.free_trial";

// An integer pref that holds enum value of current demo mode configuration.
// Values are defined by DemoSession::DemoModeConfig enum.
inline constexpr char kDemoModeConfig[] = "demo_mode.config";

// A string pref holding the value of the current country for demo sessions.
inline constexpr char kDemoModeCountry[] = "demo_mode.country";

// A string pref holding the value of the retailer name input for demo sessions.
// This is now mostly called "retailer_name" in code other than in this pref and
// in Omaha request attributes
inline constexpr char kDemoModeRetailerId[] = "demo_mode.retailer_id";

// A string pref holding the value of the store number input for demo sessions.
// This is now mostly called "store_number" in code other than in this pref and
// in Omaha request attributes
inline constexpr char kDemoModeStoreId[] = "demo_mode.store_id";

// A string pref holding the value of the default locale for demo sessions.
inline constexpr char kDemoModeDefaultLocale[] = "demo_mode.default_locale";

// A string pref holding the version of the installed demo mode app.
inline constexpr char kDemoModeAppVersion[] = "demo_mode.app_version";

// A string pref holding the version of the installed demo mode resources.
inline constexpr char kDemoModeResourcesVersion[] =
    "demo_mode.resources_version";

// A string list pref holding the collection of user intested perk IDs. The
// values are used to filter campaign in growth framework.
inline constexpr char kGrowthPerksInterested[] = "growth.perks";

// A dictionary pref containing the set of touchpad settings for the user. This
// is synced for all user devices.
inline constexpr char kTouchpadInternalSettings[] =
    "ash.settings.touchpad.internal";

// A dictionary pref containing the set of pointing stick settings for the user.
// This is synced for all user devices.
inline constexpr char kPointingStickInternalSettings[] =
    "ash.settings.pointing_stick.internal";

// A dictionary pref containing the set of default mouse settings for the user.
// This is always configured to the settings for the mouse the user last used.
// These are applied to new mice that are connected to the system. This is
// synced for all user devices.
inline constexpr char kMouseDefaultSettings[] = "ash.settings.mouse.defaults";

// A dictionary pref containing the set of default ChromeOS keyboard settings
// for the user. This is always configured to the settings for the ChromeOS
// keyboard the user last used. These are applied to new ChromeOS keyboards that
// are connected to the system. This is synced for all user devices.
inline constexpr char kKeyboardDefaultChromeOSSettings[] =
    "ash.settings.keyboard.chromeos_defaults";

// A dictionary pref containing the set of default non-ChromeOS keyboard
// settings for the user. This is always configured to the settings for the
// non-ChromeOS keyboard the user last used. These are applied to new
// non-ChromeOS keyboards that are connected to the system. This is synced for
// all user devices.
inline constexpr char kKeyboardDefaultNonChromeOSSettings[] =
    "ash.settings.keyboard.non_chromeos_defaults";

// A dictionary pref containing the set of default split modifier keyboard
// settings for the user. This is always configured to the settings for the
// split modifier keyboard the user last used. These are applied to new
// split modifier keyboards that are connected to the system. This is synced for
// all user devices.
inline constexpr char kKeyboardDefaultSplitModifierSettings[] =
    "ash.settings.keyboard.split_modifier_defaults";

// A boolean pref of whether the user has used a split modifier keyboard before.
inline constexpr char kKeyboardHasSplitModifierKeyboard[] =
    "ash.settings.keyboard.has_split_modifier_keyboard";

// A dictionary pref containing the set of default touchpad settings for the
// user. These are applied to new touchpads that are connected to the system.
// This is synced for all user devices.
inline constexpr char kTouchpadDefaultSettings[] =
    "ash.settings.touchpad.defaults";

// An integer pref that counts the number of times we have shown a form of
// screen capture education (a nudge or tutorial).
inline constexpr char kCaptureModeEducationShownCount[] =
    "ash.capture_mode.capture_mode_education_shown_count";

// A time pref that tracks the most recent instance when we have shown a form of
// screen capture education (a nudge or tutorial).
inline constexpr char kCaptureModeEducationLastShown[] =
    "ash.capture_mode.capture_mode_education_last_shown";

// A string pref that is set by enterprise policy when admin forces or
// recommends a custom path to save screen captures.
inline constexpr char kCaptureModePolicySavePath[] =
    "ash.capture_mode.policy_save_path";

// A dictionary that stores app icons' light vibrant colors.
inline constexpr char kAshAppIconLightVibrantColorCache[] =
    "ash.app_icon_light_vibrant_color_cache";

// A dictionary that stores the color group component of app icons' sortable
// colors.
inline constexpr char kAshAppIconSortableColorGroupCache[] =
    "ash.app_icon_sortable_color_group_cache";

// A dictionary that stores the hue component of app icons' sortable colors.
inline constexpr char kAshAppIconSortableColorHueCache[] =
    "ash.app_icon_sortable_color_hue_cache";

// An integer pref that controls the state (Disabled, Ctrl, etc) of the
// F11 setting found in the customize keyboard keys subpage in device
// settings. Can be controlled through the F11KeyModifier policy.
inline constexpr char kF11KeyModifier[] = "ash.settings.f11_key_modifier";

// An integer pref that controls the state (Disabled, Ctrl, etc) of the
// F12 setting found in the customize keyboard keys subpage in device
// settings. Can be controlled through the F12KeyModifier policy.
inline constexpr char kF12KeyModifier[] = "ash.settings.f12_key_modifier";

// An integer pref that controls the default shortcut (Alt/Search/Disabled) for
// remapping a key event to the Home/End "six pack" keys. These settings can be
// controlled through policy HomeAndEndKeysModifier.
inline constexpr char kHomeAndEndKeysModifier[] =
    "ash.settings.home_and_end_keys_modifier";

// An integer pref that controls the default shortcut (Alt/Search/Disabled) for
// remapping a key event to the PageUp/PageUp "six pack" keys. These settings
// can be controlled through policy PageUpAndPageDownKeysModifier.
inline constexpr char kPageUpAndPageDownKeysModifier[] =
    "ash.settings.page_up_and_page_down_keys_modifier";

// An integer pref that controls the default shortcut (Alt/Search/Disabled) for
// remapping a key event to the Delete "six pack" key. This setting can be
// controlled through policy DeleteKeyModifier.
inline constexpr char kDeleteKeyModifier[] = "ash.settings.delete_key_modifier";

// An integer pref that controls the default shortcut (Search/Disabled) for
// remapping a key event to the Insert "six pack" key. This setting can be
// controlled through policy InsertKeyModifier.
inline constexpr char kInsertKeyModifier[] = "ash.settings.insert_key_modifier";

// An integer pref to define whether restore apps and web pages on startup.
// Refer to `ash::full_restore::RestoreOption`.
inline constexpr char kRestoreAppsAndPagesPrefName[] =
    "settings.restore_apps_and_pages";

// A boolean pref of whether the Game Dashboard should show the welcome dialog
// on app startup.
inline constexpr char kGameDashboardShowWelcomeDialog[] =
    "ash.game_dashboard.show_welcome_dialog";

// A boolean pref of whether the Game Dashboard should show the toolbar.
inline constexpr char kGameDashboardShowToolbar[] =
    "ash.game_dashboard.show_toolbar";

// An integer pref indicating the system's software scanning mode. Options:
// (0) Never, (1) Always, (2) Only when charging.
static constexpr char kSoftwareScanningEnabled[] =
    "ash.nearby.software_scanning_enabled";

// A boolean pref that tracks whether the Birch context menu has been shown.
inline constexpr char kBirchContextMenuShown[] = "ash.birch.context_menu_shown";

// An integer pref that tracks the number of times the Birch privacy nudge has
// been shown.
inline constexpr char kBirchPrivacyNudgeShownCount[] =
    "ash.birch.privacy_nudge_shown_count";

// A time pref that stores the time the Birch privacy nudge was last shown.
inline constexpr char kBirchPrivacyNudgeLastShownTime[] =
    "ash.birch.privacy_nudge_last_shown";

// A boolean pref indicating whether to show Birch suggestions in Overview mode.
inline constexpr char kBirchShowSuggestions[] = "ash.birch.show_suggestions";

// A boolean pref indicating whether to use Celsius temperatures in the weather
// suggestion.
inline constexpr char kBirchUseCelsius[] = "ash.birch.use_celsius";

// LINT.IfChange

// A boolean pref indicating whether Birch should use Google Calendar data.
inline constexpr char kBirchUseCalendar[] = "ash.birch.use_calendar";

// A boolean pref indicating whether Birch should use file suggestions.
inline constexpr char kBirchUseFileSuggest[] = "ash.birch.use_file_suggest";

// A boolean pref indicating whether Birch should use Chrome tab data, from
// recent tabs on other devices, last active URL, or most visited URL.
inline constexpr char kBirchUseChromeTabs[] = "ash.birch.use_chrome_tabs";

// A boolean pref indicating whether Birch should use lost media data.
inline constexpr char kBirchUseLostMedia[] = "ash.birch.use_lost_media";

// A boolean pref indicating whether Birch should use weather data.
inline constexpr char kBirchUseWeather[] = "ash.birch.use_weather";

// A boolean pref indicating whether Birch should use release notes data.
inline constexpr char kBirchUseReleaseNotes[] = "ash.birch.use_release_notes";

// A boolean pref indicating whether Birch should use coral data.
inline constexpr char kBirchUseCoral[] = "ash.birch.use_coral";

// LINT.ThenChange(/chrome/browser/ui/ash/birch/birch_browsertest.cc)

// A boolean pref that holds whether the user dismissed the extended updates
// notification.
inline constexpr char kExtendedUpdatesNotificationDismissed[] =
    "ash.extended_updates.notification_dismissed";

// An integer value that indicates the last expanded (opened) glanceables time
// management UI, which could be Tasks or Classroom.
inline constexpr char kGlanceablesTimeManagementLastExpandedBubble[] =
    "ash.glanceables_time_management.last_expanded";

// Lists of strings containing excluded and included domains for DNS-over-HTTPs.
inline constexpr char kDnsOverHttpsExcludedDomains[] =
    "dns_over_https.excluded_domains";
inline constexpr char kDnsOverHttpsIncludedDomains[] =
    "dns_over_https.included_domains";

// Dictionary pref representing information related to whether the Graduation
// app should be enabled for a user. This corresponds to the policy defined in
// GraduationEnablementStatus.yaml.
inline constexpr char kGraduationEnablementStatus[] =
    "ash.graduation.enablement_status";

// Boolean pref representing if the nudge for the Graduation app has been shown.
inline constexpr char kGraduationNudgeShown[] = "ash.graduation.nudge_shown";

//-----------------------------------------------------------------------------
// Language related Prefs
//-----------------------------------------------------------------------------

// A string pref set to the current input method.
inline constexpr char kLanguageCurrentInputMethod[] =
    "settings.language.current_input_method";

// A string pref (comma-separated list) that corresponds to the set of enabled
// 1P input method engine IDs.
inline constexpr char kLanguagePreloadEngines[] =
    "settings.language.preload_engines";

// NOTE: New prefs should start with the "ash." prefix. Existing prefs moved
// into this file should not be renamed, since they may be synced.

}  // namespace ash::prefs

#endif  // ASH_CONSTANTS_ASH_PREF_NAMES_H_
