// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"

namespace ash {
namespace prefs {

// Map of strings to values used for assistive input settings.
const char kAssistiveInputFeatureSettings[] =
    "assistive_input_feature_settings";

// A boolean pref of whether assist personal info is enabled.
const char kAssistPersonalInfoEnabled[] =
    "assistive_input.personal_info_enabled";

// A boolean pref of whether assist predictive writing is enabled.
const char kAssistPredictiveWritingEnabled[] =
    "assistive_input.predictive_writing_enabled";

// A boolean pref of whether Google Calendar Integration is enabled.
// Disabling this pref would stop the user from getting their
// Google Calendar events from the system tray - Calendar widget.
const char kCalendarIntegrationEnabled[] = "ash.calendar_integration_enabled";

// A boolean pref of whether emoji suggestion is enabled.
const char kEmojiSuggestionEnabled[] =
    "assistive_input.emoji_suggestion_enabled";

// A boolean pref of whether emoji suggestion is enabled for managed device.
const char kEmojiSuggestionEnterpriseAllowed[] =
    "assistive_input.emoji_suggestion.enterprise_allowed";

// Pref which stores a list of Embedded Universal Integrated Circuit Card
// (EUICC) D-Bus paths which have had their installed profiles refreshed from
// Hermes. Each path is stored as a string.
const char kESimRefreshedEuiccs[] = "cros_esim.refreshed_euiccs";

// Pref which stores a list of eSIM profiles. Each entry in the list is created
// by serializing a CellularESimProfile.
const char kESimProfiles[] = "cros_esim.esim_profiles";

// Pref which stores a dictionary of Integrated Circuit Card IDentifier (ICCID)
// and Subscription Management - Data Preparation (SMDP+) address pair for each
// managed cellular network.
const char kManagedCellularIccidSmdpPair[] =
    "cros_esim.managed_iccid_smdp_pair";

// A dictionary pref to hold the mute setting for all the currently known
// audio devices.
const char kAudioDevicesMute[] = "settings.audio.devices.mute";

// A dictionary pref storing the gain settings for all the currently known
// audio input devices.
const char kAudioDevicesGainPercent[] = "settings.audio.devices.gain_percent";

// A dictionary pref storing the volume settings for all the currently known
// audio output devices.
const char kAudioDevicesVolumePercent[] =
    "settings.audio.devices.volume_percent";

// An integer pref to initially mute volume if 1. This pref is ignored if
// |kAudioOutputAllowed| is set to false, but its value is preserved, therefore
// when the policy is lifted the original mute state is restored.  This setting
// is here only for migration purposes now. It is being replaced by the
// |kAudioDevicesMute| setting.
const char kAudioMute[] = "settings.audio.mute";

// A pref holding the value of the policy used to disable playing audio on
// ChromeOS devices. This pref overrides |kAudioMute| but does not overwrite
// it, therefore when the policy is lifted the original mute state is restored.
const char kAudioOutputAllowed[] = "hardware.audio_output_enabled";

// A double pref storing the user-requested volume. This setting is here only
// for migration purposes now. It is being replaced by the
// |kAudioDevicesVolumePercent| setting.
const char kAudioVolumePercent[] = "settings.audio.volume_percent";

// A dictionary pref that maps stable device id string to |AudioDeviceState|.
// Different state values indicate whether or not a device has been selected
// as the active one for audio I/O, or it's a new plugged device.
const char kAudioDevicesState[] = "settings.audio.device_state";

// A dictionary maps each input device to a unique natural number
// representing the user preference priority among all.
// E.g {(0x9a, 1), (0xab, 2), (0xbc, 3), (0xcd, 4)}
const char kAudioInputDevicesUserPriority[] =
    "settings.audio.input_user_priority";

// A dictionary maps each input device to a unique natural number
// representing the user preference priority among all.
// E.g {(0x9a, 1), (0xab, 2), (0xbc, 3), (0xcd, 4)}
const char kAudioOutputDevicesUserPriority[] =
    "settings.audio.output_user_priority";

// A dictionary pref that maps device id string to the timestamp of the last
// time the audio device was connected, in `base::Time::ToDoubleT()`'s format.
const char kAudioDevicesLastSeen[] = "settings.audio.last_seen";

// A string pref storing an identifier that is getting sent with parental
// consent in EDU account addition flow.
const char kEduCoexistenceId[] = "account_manager.edu_coexistence_id";

// A string pref containing valid version of Edu Coexistence Terms of Service.
// Controlled by EduCoexistenceToSVersion policy.
const char kEduCoexistenceToSVersion[] =
    "family_link_user.edu_coexistence_tos_version";

// A dictionary pref that associates the secondary edu accounts gaia id string
// with the corresponding accepted Edu Coexistence Terms of Service version
// number.
const char kEduCoexistenceToSAcceptedVersion[] =
    "family_link_user.edu_coexistence_tos_accepted_version";

// A boolean pref indicating whether welcome page should be skipped in
// in-session 'Add account' flow.
const char kShouldSkipInlineLoginWelcomePage[] =
    "should_skip_inline_login_welcome_page";

// A dictionary of info for Quirks Client/Server interaction, mostly last server
// request times, keyed to display product_id's.
const char kQuirksClientLastServerCheck[] = "quirks_client.last_server_check";

// Whether 802.11r Fast BSS Transition is currently enabled.
const char kDeviceWiFiFastTransitionEnabled[] =
    "net.device_wifi_fast_transition_enabled";

// A boolean pref that controls whether input noise cancellation is enabled.
const char kInputNoiseCancellationEnabled[] =
    "ash.input_noise_cancellation_enabled";

// The following SAML-related prefs are not settings that the domain admin can
// set, but information that the SAML Identity Provider can send us:

// A time pref - when the SAML password was last set, according to the SAML IdP.
const char kSamlPasswordModifiedTime[] = "saml.password_modified_time";
// A time pref - when the SAML password did expire, or will expire, according to
// the SAML IDP.
const char kSamlPasswordExpirationTime[] = "saml.password_expiration_time";
// A string pref - the URL where the user can update their password, according
// to the SAML IdP.
const char kSamlPasswordChangeUrl[] = "saml.password_change_url";

// Boolean pref indicating whether the user has completed (or skipped) the
// out-of-box experience (OOBE) sync consent screen. Before this pref is set
// both OS and browser sync will be disabled. After this pref is set it's
// possible to check sync state to see if the user enabled it.
const char kSyncOobeCompleted[] = "sync.oobe_completed";

// A string representing the last version of Chrome that System Web Apps were
// updated for.
const char kSystemWebAppLastUpdateVersion[] =
    "web_apps.system_web_app_last_update";

// A string representing the last locale that System Web Apps were installed in.
// This is used to refresh System Web Apps i18n when the locale is changed.
const char kSystemWebAppLastInstalledLocale[] =
    "web_apps.system_web_app_last_installed_language";

// An int representing the number of failures to install SWAs for a given
// version & locale pair. After 3 failures, we'll abandon this version to avoid
// bootlooping, and wait for a new version to come along.
const char kSystemWebAppInstallFailureCount[] =
    "web_apps.system_web_app_failure_count";

// A string representing the latest Chrome version where an attempt was made
// to install. In the case of success, this and LastUpdateVersion will be the
// same. If there is an installation failure, they will diverge until a
// successful installation is made.
extern const char kSystemWebAppLastAttemptedVersion[] =
    "web_apps.system_web_app_last_attempted_update";

// A string representing the most recent locale that was attempted to be
// installed. In the case of success, this and LastUpdateVersion will be the
// same. If there is an installation failure, they will diverge until a
// successful installation is made.
extern const char kSystemWebAppLastAttemptedLocale[] =
    "web_apps.system_web_app_last_attempted_language";

// Boolean pref indicating whether a user has enabled the display password
// button on the login/lock screen.
const char kLoginDisplayPasswordButtonEnabled[] =
    "login_display_password_button_enabled";

// Boolean pref indicating whether the user has enabled Suggested Content in
// OS settings > Privacy > "Suggest new content to explore".
const char kSuggestedContentEnabled[] = "settings.suggested_content_enabled";

// Boolean value indicating the user has hidden the launcher continue section
// (usually because they want more visual space available for apps).
const char kLauncherContinueSectionHidden[] =
    "launcher.continue_section_hidden";

// Boolean value that indicates that the user has given feedback for removing
// items from the continue section.
const char kLauncherFeedbackOnContinueSectionSent[] =
    "ash.launcher.continue_section_removal_feedback_sent";

// A time pref indicating the last time a request was made to update the
// Continue section.
const char kLauncherLastContinueRequestTime[] =
    "launcher.last_continue_request_time";

// Boolean pref recording whether a search result has ever been launched from
// the Chrome OS launcher.
const char kLauncherResultEverLaunched[] = "launcher.result_ever_launched";

// Dictionary pref to store data on the distribution of provider relevance
// scores for the launcher normalizer.
const char kLauncherSearchNormalizerParameters[] =
    "launcher.search_normalizer_parameters";

// Whether or not to use a long delay for Continue section requests.
const char kLauncherUseLongContinueDelay[] = "launcher.use_long_continue_delay";

// Boolean pref indicating whether system-wide trace collection using the
// Perfetto system tracing service is allowed.
const char kDeviceSystemWideTracingEnabled[] =
    "device_system_wide_tracing_enabled";

// A boolean pref which determines whether the large cursor feature is enabled.
const char kAccessibilityLargeCursorEnabled[] =
    "settings.a11y.large_cursor_enabled";
// An integer pref that specifies the size of large cursor for accessibility.
const char kAccessibilityLargeCursorDipSize[] =
    "settings.a11y.large_cursor_dip_size";
// A boolean pref which determines whether the sticky keys feature is enabled.
const char kAccessibilityStickyKeysEnabled[] =
    "settings.a11y.sticky_keys_enabled";
// A boolean pref which determines whether spoken feedback is enabled.
const char kAccessibilitySpokenFeedbackEnabled[] = "settings.accessibility";
// A boolean pref which determines whether automatic reading for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxAutoRead[] =
    "settings.a11y.chromevox.auto_read";
// A boolean pref which determines whether announce download notifications for
// ChromeVox is enabled.
const char kAccessibilityChromeVoxAnnounceDownloadNotifications[] =
    "settings.a11y.chromevox.announce_download_notifications";
// A boolean pref which determines whether announce rich text attributes for
// ChromeVox is enabled.
const char kAccessibilityChromeVoxAnnounceRichTextAttributes[] =
    "settings.a11y.chromevox.announce_rich_text_attributes";
// A string pref which holds the current audio strategy for ChromeVox.
const char kAccessibilityChromeVoxAudioStrategy[] =
    "settings.a11y.chromevox.audio_strategy";
// A boolean pref which determines whether braille side by side for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxBrailleSideBySide[] =
    "settings.a11y.chromevox.braille_side_by_side";
// A string pref which holds the current braille table for ChromeVox.
const char kAccessibilityChromeVoxBrailleTable[] =
    "settings.a11y.chromevox.braille_table";
// A string pref which holds the current 6-dot braille for ChromeVox.
const char kAccessibilityChromeVoxBrailleTable6[] =
    "settings.a11y.chromevox.braille_table_6";
// A string pref which holds the current 8-dot braille for ChromeVox.
const char kAccessibilityChromeVoxBrailleTable8[] =
    "settings.a11y.chromevox.braille_table_8";
// A string pref which holds the current braille table type for ChromeVox.
const char kAccessibilityChromeVoxBrailleTableType[] =
    "settings.a11y.chromevox.braille_table_type";
// A boolean pref which determines whether braille word wrap for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxBrailleWordWrap[] =
    "settings.a11y.chromevox.braille_word_wrap";
// A string pref which holds the current capital strategy for ChromeVox.
const char kAccessibilityChromeVoxCapitalStrategy[] =
    "settings.a11y.chromevox.capital_strategy";
// A string pref which holds the current capital strategy backup for ChromeVox.
const char kAccessibilityChromeVoxCapitalStrategyBackup[] =
    "settings.a11y.chromevox.capital_strategy_backup";
// A boolean pref which determines whether braille logging for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxEnableBrailleLogging[] =
    "settings.a11y.chromevox.enable_braille_logging";
// A boolean pref which determines whether earcon logging for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxEnableEarconLogging[] =
    "settings.a11y.chromevox.enable_earcon_logging";
// A boolean pref which determines whether event stream logging for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxEnableEventStreamLogging[] =
    "settings.a11y.chromevox.enable_event_stream_logging";
// A boolean pref which determines whether speech logging for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxEnableSpeechLogging[] =
    "settings.a11y.chromevox.enable_speech_logging";
// A dictionary pref that defines which event stream filters in ChromeVox are
// enabled. (e.g. {clicked: true, focus: false, hover: true})
const char kAccessibilityChromeVoxEventStreamFilters[] =
    "settings.a11y.chromevox.event_stream_filters";
// A boolean pref which determines whether language switching for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxLanguageSwitching[] =
    "settings.a11y.chromevox.language_switching";
// A boolean pref which determines whether menu braille commands for ChromeVox
// are enabled.
const char kAccessibilityChromeVoxMenuBrailleCommands[] =
    "settings.a11y.chromevox.menu_braille_commands";
// A string pref which holds the current number reading style for ChromeVox.
const char kAccessibilityChromeVoxNumberReadingStyle[] =
    "settings.a11y.chromevox.number_reading_style";
// A string pref which holds the current preferred braille display address for
// ChromeVox.
const char kAccessibilityChromeVoxPreferredBrailleDisplayAddress[] =
    "settings.a11y.chromevox.preferred_braille_display_address";
// An integer pref which holds the value for the current state of punctuation
// echo for ChromeVox. (0 = None, 1 = Some, 2 = All)
const char kAccessibilityChromeVoxPunctuationEcho[] =
    "settings.a11y.chromevox.punctuation_echo";
// A boolean pref which determines whether smart sticky mode for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxSmartStickyMode[] =
    "settings.a11y.chromevox.smart_sticky_mode";
// A boolean pref which determines whether speak text under mouse for ChromeVox
// is enabled.
const char kAccessibilityChromeVoxSpeakTextUnderMouse[] =
    "settings.a11y.chromevox.speak_text_under_mouse";
// A boolean pref which determines whether use pitch changes for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxUsePitchChanges[] =
    "settings.a11y.chromevox.use_pitch_changes";
// A boolean pref which determines whether use verbose mode for ChromeVox is
// enabled.
const char kAccessibilityChromeVoxUseVerboseMode[] =
    "settings.a11y.chromevox.use_verbose_mode";
// An integer pref which holds the value for the number of virtual braille
// columns for ChromeVox.
const char kAccessibilityChromeVoxVirtualBrailleColumns[] =
    "settings.a11y.chromevox.virtual_braille_columns";
// An integer pref which holds the value for the number of virtual braille rows
// for ChromeVox.
const char kAccessibilityChromeVoxVirtualBrailleRows[] =
    "settings.a11y.chromevox.virtual_braille_rows";
// A string pref which holds the current voice name for ChromeVox.
const char kAccessibilityChromeVoxVoiceName[] =
    "settings.a11y.chromevox.voice_name";
// A boolean pref which determines whether high contrast is enabled.
const char kAccessibilityHighContrastEnabled[] =
    "settings.a11y.high_contrast_enabled";
// A boolean pref which determines whether screen magnifier is enabled.
// NOTE: We previously had prefs named settings.a11y.screen_magnifier_type and
// settings.a11y.screen_magnifier_type2, but we only shipped one type (full).
// See http://crbug.com/170850 for history.
const char kAccessibilityScreenMagnifierEnabled[] =
    "settings.a11y.screen_magnifier";
// A boolean pref which determines whether focus following for screen magnifier
// is enabled.
const char kAccessibilityScreenMagnifierFocusFollowingEnabled[] =
    "settings.a11y.screen_magnifier_focus_following";
// An integer pref which indicates the mouse following mode for screen
// magnifier. This maps to AccessibilityController::MagnifierMouseFollowingMode.
const char kAccessibilityScreenMagnifierMouseFollowingMode[] =
    "settings.a11y.screen_magnifier_mouse_following_mode";
// A boolean pref which determines whether screen magnifier should center
// the text input focus.
const char kAccessibilityScreenMagnifierCenterFocus[] =
    "settings.a11y.screen_magnifier_center_focus";
// A double pref which determines a zooming scale of the screen magnifier.
const char kAccessibilityScreenMagnifierScale[] =
    "settings.a11y.screen_magnifier_scale";
// A boolean pref which determines whether the virtual keyboard is enabled for
// accessibility.  This feature is separate from displaying an onscreen keyboard
// due to lack of a physical keyboard.
const char kAccessibilityVirtualKeyboardEnabled[] =
    "settings.a11y.virtual_keyboard";
// A pref that identifies which kind of features are enabled for the Web Kiosk
// session.
const char kAccessibilityVirtualKeyboardFeatures[] =
    "settings.a11y.virtual_keyboard_features";
// A boolean pref which determines whether the mono audio output is enabled for
// accessibility.
const char kAccessibilityMonoAudioEnabled[] = "settings.a11y.mono_audio";
// A boolean pref which determines whether autoclick is enabled.
const char kAccessibilityAutoclickEnabled[] = "settings.a11y.autoclick";
// A boolean pref which determines whether the accessibility shortcuts are
// enabled or not.
const char kAccessibilityShortcutsEnabled[] = "settings.a11y.shortcuts_enabled";
// An integer pref which determines time in ms between when the mouse cursor
// stops and when an autoclick event is triggered.
const char kAccessibilityAutoclickDelayMs[] =
    "settings.a11y.autoclick_delay_ms";
// An integer pref which determines the event type for an autoclick event. This
// maps to AccessibilityController::AutoclickEventType.
const char kAccessibilityAutoclickEventType[] =
    "settings.a11y.autoclick_event_type";
// Whether Autoclick should immediately return to left click after performing
// another event type action, or whether it should stay as the other event type.
const char kAccessibilityAutoclickRevertToLeftClick[] =
    "settings.a11y.autoclick_revert_to_left_click";
// Whether Autoclick should stabilize the cursor movement before a click occurs
// or not.
const char kAccessibilityAutoclickStabilizePosition[] =
    "settings.a11y.autoclick_stabilize_position";
// The default threshold of mouse movement, measured in DIP, that will initiate
// a new autoclick.
const char kAccessibilityAutoclickMovementThreshold[] =
    "settings.a11y.autoclick_movement_threshold";
// The Autoclick menu position on the screen, an AutoclickMenuPosition.
const char kAccessibilityAutoclickMenuPosition[] =
    "settings.a11y.autoclick_menu_position";
// How much to greyscale the display.
const char kAccessibilityGreyscaleAmount[] = "settings.a11y.greyscale_amount";
// How much to saturate the display.
const char kAccessibilitySaturationAmount[] = "settings.a11y.saturation_amount";
// How much sepia the display.
const char kAccessibilitySepiaAmount[] = "settings.a11y.sepia_amount";
// How much to rotate the hue on the display.
const char kAccessibilityHueRotationAmount[] =
    "settings.a11y.hue_rotation_amount";
// A boolean pref which determines whether caret highlighting is enabled.
const char kAccessibilityCaretHighlightEnabled[] =
    "settings.a11y.caret_highlight";
// A boolean pref which determines whether cursor highlighting is enabled.
const char kAccessibilityCursorHighlightEnabled[] =
    "settings.a11y.cursor_highlight";
// A boolean pref which determines whether custom cursor color is enabled.
const char kAccessibilityCursorColorEnabled[] =
    "settings.a11y.cursor_color_enabled";
// An integer pref which determines the custom cursor color.
const char kAccessibilityCursorColor[] = "settings.a11y.cursor_color";
// A boolean pref which determines whether floating accessibility menu is
// enabled.
const char kAccessibilityFloatingMenuEnabled[] = "settings.a11y.floating_menu";
// Floating a11y menu position, a FloatingMenuPosition;
const char kAccessibilityFloatingMenuPosition[] =
    "settings.a11y.floating_menu_position";
// A boolean pref which determines whether focus highlighting is enabled.
const char kAccessibilityFocusHighlightEnabled[] =
    "settings.a11y.focus_highlight";
// A boolean pref which determines whether Select-to-speak is enabled.
const char kAccessibilitySelectToSpeakEnabled[] =
    "settings.a11y.select_to_speak";
// A boolean pref which determines whether Switch Access is enabled.
const char kAccessibilitySwitchAccessEnabled[] =
    "settings.a11y.switch_access.enabled";
// A dictionary pref keyed on a key code mapped to a list value of device types
// for the "select" action.
const char kAccessibilitySwitchAccessSelectDeviceKeyCodes[] =
    "settings.a11y.switch_access.select.device_key_codes";
// A dictionary pref keyed on a key code mapped to a list value of device types
// for the "next" action.
const char kAccessibilitySwitchAccessNextDeviceKeyCodes[] =
    "settings.a11y.switch_access.next.device_key_codes";
// A dictionary pref keyed on a key code mapped to a list value of device types
// for the "previous" action.
const char kAccessibilitySwitchAccessPreviousDeviceKeyCodes[] =
    "settings.a11y.switch_access.previous.device_key_codes";
// A boolean pref which determines whether auto-scanning is enabled within
// Switch Access.
const char kAccessibilitySwitchAccessAutoScanEnabled[] =
    "settings.a11y.switch_access.auto_scan.enabled";
// An integer pref which determines time delay in ms before automatically
// scanning forward (when auto-scan is enabled).
const char kAccessibilitySwitchAccessAutoScanSpeedMs[] =
    "settings.a11y.switch_access.auto_scan.speed_ms";
// An integer pref which determines time delay in ms before automatically
// scanning forward while navigating the keyboard (when auto-scan is
// enabled).
const char kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs[] =
    "settings.a11y.switch_access.auto_scan.keyboard.speed_ms";
// An integer pref which determines speed in dips per second that the gliding
// point scan cursor in switch access moves across the screen.
const char kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond[] =
    "settings.a11y.switch_access.point_scan.speed_dips_per_second";
// A boolean pref which, if set, indicates that shelf navigation buttons (home,
// back and overview button) should be shown in tablet mode. Note that shelf
// buttons might be shown even if the pref value is false - for example, if
// spoken feedback, autoclick or switch access are enabled.
const char kAccessibilityTabletModeShelfNavigationButtonsEnabled[] =
    "settings.a11y.tablet_mode_shelf_nav_buttons_enabled";
// A boolean pref which determines whether dictation is enabled.
const char kAccessibilityDictationEnabled[] = "settings.a11y.dictation";
// A string pref which determines the locale used for dictation speech
// recognition. Should be BCP-47 format, e.g. "en-US" or "es-ES".
const char kAccessibilityDictationLocale[] = "settings.a11y.dictation_locale";
// A dictionary pref which keeps track of which locales the user has seen an
// offline dictation upgrade nudge. A nudge will be shown once whenever a
// new language becomes available offline in the background, without repeating
// showing nudges where the language was already available. A locale code will
// map to a value of true if the nudge has been shown, false if it needs to be
// shown upon download completion, and will be absent from the map otherwise.
// Locales match kAccessibilityDictationLocale and are in BCP-47 format.
const char kAccessibilityDictationLocaleOfflineNudge[] =
    "settings.a11y.dictation_locale_offline_nudge";
// A boolean pref which determines whether the enhanced network voices feature
// in Select-to-speak is allowed. This pref can only be set by policy.
const char kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed[] =
    "settings.a11y.enhanced_network_voices_in_select_to_speak_allowed";

// A boolean pref which determines whether Select-to-speak shades the background
// contents that aren't being read.
const char kAccessibilitySelectToSpeakBackgroundShading[] =
    "settings.a11y.select_to_speak_background_shading";

// A boolean pref which determines whether enhanced network TTS voices are
// enabled for Select-to-speak.
const char kAccessibilitySelectToSpeakEnhancedNetworkVoices[] =
    "settings.a11y.select_to_speak_enhanced_network_voices";

// A string pref which determines the user's preferred enhanced voice for
// Select-to-speak.
const char kAccessibilitySelectToSpeakEnhancedVoiceName[] =
    "settings.a11y.select_to_speak_enhanced_voice_name";

// A boolean pref which determines whether the initial popup authorizing
// enhanced network voices for Select-to-speak has been shown to the user.
const char kAccessibilitySelectToSpeakEnhancedVoicesDialogShown[] =
    "settings.a11y.select_to_speak_enhanced_voices_dialog_shown";

// A string pref which determines the user's word highlighting color preference
// for Select-to-speak, stored as a hex color string. (e.g. "#ae003f")
const char kAccessibilitySelectToSpeakHighlightColor[] =
    "settings.a11y.select_to_speak_highlight_color";

// A boolean pref which determines whether Select-to-speak shows navigation
// controls that allow the user to navigate to next/previous sentences,
// paragraphs, and more.
const char kAccessibilitySelectToSpeakNavigationControls[] =
    "settings.a11y.select_to_speak_navigation_controls";

// A string pref which determines the user's preferred voice for
// Select-to-speak.
const char kAccessibilitySelectToSpeakVoiceName[] =
    "settings.a11y.select_to_speak_voice_name";

// A boolean pref which determines whether Select-to-speak enables automatic
// voice switching between different languages.
const char kAccessibilitySelectToSpeakVoiceSwitching[] =
    "settings.a11y.select_to_speak_voice_switching";

// A boolean pref which determines whether Select-to-speak highlights each word
// as it is read.
const char kAccessibilitySelectToSpeakWordHighlight[] =
    "settings.a11y.select_to_speak_word_highlight";

// A boolean pref which determines whether the accessibility menu shows
// regardless of the state of a11y features.
const char kShouldAlwaysShowAccessibilityMenu[] = "settings.a11y.enable_menu";

// A boolean pref which determines whether alt-tab should show only windows in
// the current desk or all windows.
const char kAltTabPerDesk[] = "ash.alttab.per_desk";

// A dictionary storing the number of times and most recent time all contextual
// tooltips have been shown.
const char kContextualTooltips[] = "settings.contextual_tooltip.shown_info";

// A list containing the stored virtual desks names in the same order of the
// desks in the overview desks bar. This list will be used to restore the desks,
// their order, and their names for the primary user on first signin. If a desk
// hasn't been renamed by the user (i.e. it uses one of the default
// automatically-assigned desk names such as "Desk 1", "Desk 2", ... etc.), its
// name will appear in this list as an empty string. The desk names are stored
// as UTF8 strings.
const char kDesksNamesList[] = "ash.desks.desks_names_list";
// A list containing the stored virtual desks guids in the same order of the
// desks in the overview desks bar. This list will be used to restore desk guids
// for the primary user on first sign-in. The guids are stored as lowercase
// strings.
const char kDesksGuidsList[] = "ash.desks.desks_guids_list";
// This list stores the metrics of virtual desks. Like |kDesksNamesList|, this
// list stores entries in the same order of the desks in the overview desks bar.
// Values are stored as dictionaries.
const char kDesksMetricsList[] = "ash.desks.desks_metrics_list";
// A dict pref storing the metrics related to the weekly active desks of a user.
const char kDesksWeeklyActiveDesksMetrics[] = "ash.desks.weekly_active_desks";
// An integer index of a user's active desk.
const char kDesksActiveDesk[] = "ash.desks.active_desk";

// A boolean pref storing the enabled status of the Docked Magnifier feature.
const char kDockedMagnifierEnabled[] = "ash.docked_magnifier.enabled";
// A double pref storing the scale value of the Docked Magnifier feature by
// which the screen is magnified.
const char kDockedMagnifierScale[] = "ash.docked_magnifier.scale";
// A double pref storing the screen height divisor value of the Docked Magnifier
// feature defining what proportion of the screen the docked magnifier viewport
// occupies.
const char kDockedMagnifierScreenHeightDivisor[] =
    "ash.docked_magnifier.screen_height_divisor";

// A boolean pref which indicates whether the docked magnifier confirmation
// dialog has ever been shown.
const char kDockedMagnifierAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.docked_magnifier_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the high contrast magnifier
// confirmation dialog has ever been shown.
const char kHighContrastAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.high_contrast_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the screen magnifier confirmation
// dialog has ever been shown.
const char kScreenMagnifierAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.screen_magnifier_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the dictation confirmation dialog has
// ever been shown.
const char kDictationAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.dictation_accelerator_dialog_has_been_accepted";
// A boolean pref which indicates whether the Dictation DLC success notification
// has ever been shown.
const char kDictationDlcSuccessNotificationHasBeenShown[] =
    "settings.a11y.dictation_dlc_success_notification_has_been_shown";
// A boolean pref which indicates whether the Dictation DLC only Pumpkin
// downloaded notification has ever been shown.
const char kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown[] =
    "settings.a11y.dictation_dlc_only_pumpkin_downloaded_notification_has_been_"
    "shown";
// A boolean pref which indicates whether the Dictation DLC only SODA
// downloaded notification has ever been shown.
const char kDictationDlcOnlySodaDownloadedNotificationHasBeenShown[] =
    "settings.a11y.dictation_dlc_only_soda_downloaded_notification_has_been_"
    "shown";
// A boolean pref which indicates whether the Dictation No DLCs downloaded
// notification has ever been shown.
const char kDictationNoDlcsDownloadedNotificationHasBeenShown[] =
    "settings.a11y.dictation_dlc_no_dlcs_downloaded_notification_has_been_"
    "shown";

// A boolean pref which indicates whether the display rotation confirmation
// dialog has ever been shown.
// Renamed 10/2019 to force reset the pref to false.
const char kDisplayRotationAcceleratorDialogHasBeenAccepted2[] =
    "settings.a11y.display_rotation_accelerator_dialog_has_been_accepted2";

// A dictionary pref that stores the mixed mirror mode parameters.
const char kDisplayMixedMirrorModeParams[] =
    "settings.display.mixed_mirror_mode_param";
// Power state of the current displays from the last run.
const char kDisplayPowerState[] = "settings.display.power_state";
// A dictionary pref that stores per display preferences.
const char kDisplayProperties[] = "settings.display.properties";
// Boolean controlling whether privacy screen is enabled.
const char kDisplayPrivacyScreenEnabled[] =
    "settings.display.privacy_screen_enabled";
// A dictionary pref that specifies the state of the rotation lock, and the
// display orientation, for the internal display.
const char kDisplayRotationLock[] = "settings.display.rotation_lock";
// A dictionary pref that stores the touch associations for the device.
const char kDisplayTouchAssociations[] = "settings.display.touch_associations";
// A dictionary pref that stores the port mapping for touch devices.
const char kDisplayTouchPortAssociations[] =
    "settings.display.port_associations";
// A list pref that stores the mirror info for each external display.
const char kExternalDisplayMirrorInfo[] =
    "settings.display.external_display_mirror_info";
// A dictionary pref that specifies per-display layout/offset information.
// Its key is the ID of the display and its value is a dictionary for the
// layout/offset information.
const char kSecondaryDisplays[] = "settings.display.secondary_displays";
// A boolean pref which determines whether the display configuration set by
// managed guest session should be stored in local state.
const char kAllowMGSToStoreDisplayProperties[] =
    "settings.display.allow_mgs_to_store";

// A boolean pref that enable fullscreen alert bubble.
// TODO(zxdan): Change to an allowlist in M89.
const char kFullscreenAlertEnabled[] = "ash.fullscreen_alert_enabled";

// A boolean pref storing whether the gesture education notification has ever
// been shown to the user, which we use to stop showing it again.
const char kGestureEducationNotificationShown[] =
    "ash.gesture_education.notification_shown";

// A boolean pref which stores whether a stylus has been seen before.
const char kHasSeenStylus[] = "ash.has_seen_stylus";
// A boolean pref which stores whether a the palette warm welcome bubble
// (displayed when a user first uses a stylus) has been shown before.
const char kShownPaletteWelcomeBubble[] = "ash.shown_palette_welcome_bubble";
// A boolean pref that specifies if the stylus tools should be enabled/disabled.
const char kEnableStylusTools[] = "settings.enable_stylus_tools";
// A boolean pref that specifies if the ash palette should be launched after an
// eject input event has been received.
const char kLaunchPaletteOnEjectEvent[] =
    "settings.launch_palette_on_eject_event";

// Boolean pref indicating whether the PCI tunneling is allowed for external
// Thunderbolt/USB4 peripherals. This pref is only used if the policy
// "DevicePciPeripheralDataAccessEnabled" is set to "unset".
const char kLocalStateDevicePeripheralDataAccessEnabled[] =
    "settings.local_state_device_pci_data_access_enabled";

// The timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of the user
// pressed the shutdown button from shelf.
// static
const char kLoginShutdownTimestampPrefName[] =
    "ash.shelf.login_shutdown_timestamp";

// A boolean pref that specifies if the cellular setup notification can be
// shown or not. This notification should be shown post-OOBE if the user has a
// cellular-capable device but no available cellular networks. It should only be
// shown at most once per user.
const char kCanCellularSetupNotificationBeShown[] =
    "ash.cellular_setup.can_setup_notification_be_shown";

// Boolean pref indicating whether the privacy warning of the managed-guest
// session on both; the login screen and inside the auto-launched session,
// should be displayed or not.
const char kManagedGuestSessionPrivacyWarningsEnabled[] =
    "managed_session.privacy_warning_enabled";

// Boolean pref indicating whether the user has enabled detection of snooping
// over their shoulder, and hiding of notifications when a snooper is detected.
const char kSnoopingProtectionEnabled[] =
    "ash.privacy.snooping_protection_enabled";
const char kSnoopingProtectionNotificationSuppressionEnabled[] =
    "ash.privacy.snooping_protection_notification_suppression_enabled";

// A string pref storing the type of lock screen notification mode.
// "show" -> show notifications on the lock screen
// "hide" -> hide notifications at all on the lock screen (default)
// "hideSensitive" -> hide sensitive content on the lock screen
// (other values are treated as "hide")
const char kMessageCenterLockScreenMode[] =
    "ash.message_center.lock_screen_mode";

// Value of each options of the lock screen notification settings. They are
// used the pref of ash::prefs::kMessageCenterLockScreenMode.
const char kMessageCenterLockScreenModeShow[] = "show";
const char kMessageCenterLockScreenModeHide[] = "hide";
const char kMessageCenterLockScreenModeHideSensitive[] = "hideSensitive";

// A boolean pref storing the enabled status of the ambient color feature.
const char kAmbientColorEnabled[] = "ash.ambient_color.enabled";

// A boolean pref that indicates whether dark mode is enabled.
const char kDarkModeEnabled[] = "ash.dark_mode.enabled";

// An integer pref that indicates the color scheme used to calculate the dynamic
// color palette.
const char kDynamicColorColorScheme[] = "ash.dynamic_color.color_scheme";

// A uint64 pref that indicates the seed color used to calculate the dynamic
// color palette. It is an ARGB 32-bit unsigned integer stored as a uint64.
const char kDynamicColorSeedColor[] = "ash.dynamic_color.seed_color";

// An integer pref storing the number of times that dark/light mode educational
// can still be shown. It will be initialized to the maximum number of times
// that the nudge can be shown. And will be set to 0 if the user toggled the
// entry points of dark/light mode ("Dark theme" inside quick settings or
// personalization hub), which means the user already knows how to change the
// color mode of the system.
const char kDarkLightModeNudgeLeftToShowCount[] =
    "ash.dark_light_mode.educational_nudge";

// An integer pref storing the type of automatic scheduling of turning on and
// off the dark mode feature similar to `kNightLightScheduleType`, but
// custom scheduling (2) is the same as sunset to sunrise scheduling (1)
// because dark mode does not support custom scheduling.
const char kDarkModeScheduleType[] = "ash.dark_mode.schedule_type";

// A boolean pref storing the enabled status of the NightLight feature.
const char kNightLightEnabled[] = "ash.night_light.enabled";

// A double pref storing the screen color temperature set by the NightLight
// feature. The expected values are in the range of 0.0 (least warm) and 1.0
// (most warm).
const char kNightLightTemperature[] = "ash.night_light.color_temperature";

// An integer pref storing the type of automatic scheduling of turning on and
// off the NightLight feature. Valid values are:
// 0 -> NightLight is never turned on or off automatically.
// 1 -> NightLight is turned on and off at the sunset and sunrise times
// respectively.
// 2 -> NightLight schedule times are explicitly set by the user.
//
// See ash::NightLightController::ScheduleType.
const char kNightLightScheduleType[] = "ash.night_light.schedule_type";

// Integer prefs storing the start and end times of the automatic schedule at
// which NightLight turns on and off respectively when the schedule type is set
// to a custom schedule. The times are represented as the number of minutes from
// 00:00 (12:00 AM) regardless of the date or the timezone.
// See ash::TimeOfDayTime.
const char kNightLightCustomStartTime[] = "ash.night_light.custom_start_time";
const char kNightLightCustomEndTime[] = "ash.night_light.custom_end_time";

// Double prefs storing the most recent valid geoposition, which is only used
// when the device lacks connectivity and we're unable to retrieve a valid
// geoposition to calculate the sunset / sunrise times.
const char kNightLightCachedLatitude[] = "ash.night_light.cached_latitude";
const char kNightLightCachedLongitude[] = "ash.night_light.cached_longitude";

// A boolean pref storing whether the AutoNightLight notification has ever been
// dismissed by the user, which we use to stop showing it again.
const char kAutoNightLightNotificationDismissed[] =
    "ash.auto_night_light.notification_dismissed";

// Whether the Chrome OS lock screen is allowed.
const char kAllowScreenLock[] = "allow_screen_lock";

// A boolean pref that turns on automatic screen locking.
const char kEnableAutoScreenLock[] = "settings.enable_screen_lock";

// Screen brightness percent values to be used when running on AC power.
// Specified by the policy.
const char kPowerAcScreenBrightnessPercent[] =
    "power.ac_screen_brightness_percent";

// Inactivity time in milliseconds while the system is on AC power before
// the screen should be dimmed, turned off, or locked, before an
// IdleActionImminent D-Bus signal should be sent, or before
// kPowerAcIdleAction should be performed.  0 disables the delay (N/A for
// kPowerAcIdleDelayMs).
const char kPowerAcScreenDimDelayMs[] = "power.ac_screen_dim_delay_ms";
const char kPowerAcScreenOffDelayMs[] = "power.ac_screen_off_delay_ms";
const char kPowerAcScreenLockDelayMs[] = "power.ac_screen_lock_delay_ms";
const char kPowerAcIdleWarningDelayMs[] = "power.ac_idle_warning_delay_ms";

// Boolean pref of whether adaptive charging (i.e. holding battery at a sub-100%
// charge until necessary to extend battery life) is enabled.
const char kPowerAdaptiveChargingEnabled[] = "power.adaptive_charging_enabled";
// Boolean pref of whether adaptive charging educational nudge is shown to the
// user.
const char kPowerAdaptiveChargingNudgeShown[] =
    "power.adaptive_charging_nudge_shown";

// Screen brightness percent values to be used when running on battery power.
// Specified by the policy.
const char kPowerBatteryScreenBrightnessPercent[] =
    "power.battery_screen_brightness_percent";

// Similar delays while the system is on battery power.
const char kPowerBatteryScreenDimDelayMs[] =
    "power.battery_screen_dim_delay_ms";
const char kPowerBatteryScreenOffDelayMs[] =
    "power.battery_screen_off_delay_ms";
const char kPowerBatteryScreenLockDelayMs[] =
    "power.battery_screen_lock_delay_ms";
const char kPowerBatteryIdleWarningDelayMs[] =
    "power.battery_idle_warning_delay_ms";
const char kPowerBatteryIdleDelayMs[] = "power.battery_idle_delay_ms";
const char kPowerAcIdleDelayMs[] = "power.ac_idle_delay_ms";

// Inactivity delays used to dim the screen or turn it off while the screen is
// locked.
const char kPowerLockScreenDimDelayMs[] = "power.lock_screen_dim_delay_ms";
const char kPowerLockScreenOffDelayMs[] = "power.lock_screen_off_delay_ms";

// Action that should be performed when the idle delay is reached while the
// system is on AC power or battery power.
// Values are from the chromeos::PowerPolicyController::Action enum.
const char kPowerAcIdleAction[] = "power.ac_idle_action";
const char kPowerBatteryIdleAction[] = "power.battery_idle_action";

// Action that should be performed when the lid is closed.
// Values are from the chromeos::PowerPolicyController::Action enum.
const char kPowerLidClosedAction[] = "power.lid_closed_action";

// Should audio and video activity be used to disable the above delays?
const char kPowerUseAudioActivity[] = "power.use_audio_activity";
const char kPowerUseVideoActivity[] = "power.use_video_activity";

// Should extensions, ARC apps, and other code within Chrome be able to override
// system power management (preventing automatic actions like sleeping, locking,
// or screen dimming)?
const char kPowerAllowWakeLocks[] = "power.allow_wake_locks";

// Should extensions, ARC apps, and other code within Chrome be able to override
// display-related power management? (Disallowing wake locks in general takes
// precedence over this.)
const char kPowerAllowScreenWakeLocks[] = "power.allow_screen_wake_locks";

// Amount by which the screen-dim delay should be scaled while the system
// is in presentation mode. Values are limited to a minimum of 1.0.
const char kPowerPresentationScreenDimDelayFactor[] =
    "power.presentation_screen_dim_delay_factor";

// Amount by which the screen-dim delay should be scaled when user activity is
// observed while the screen is dimmed or soon after the screen has been turned
// off.  Values are limited to a minimum of 1.0.
const char kPowerUserActivityScreenDimDelayFactor[] =
    "power.user_activity_screen_dim_delay_factor";

// Whether the power management delays should start running only after the first
// user activity has been observed in a session.
const char kPowerWaitForInitialUserActivity[] =
    "power.wait_for_initial_user_activity";

// Boolean controlling whether the panel backlight should be forced to a
// nonzero level when user activity is observed.
const char kPowerForceNonzeroBrightnessForUserActivity[] =
    "power.force_nonzero_brightness_for_user_activity";

// Boolean controlling whether a shorter suspend delay should be used after the
// user forces the display off by pressing the power button. Provided to allow
// policy to control this behavior.
const char kPowerFastSuspendWhenBacklightsForcedOff[] =
    "power.fast_suspend_when_backlights_forced_off";

// Boolean controlling whether smart dim model is enabled.
const char kPowerSmartDimEnabled[] = "power.smart_dim_enabled";

// Boolean controlling whether ALS logging is enabled.
const char kPowerAlsLoggingEnabled[] = "power.als_logging_enabled";

// Boolean controlling whether quick dim is enabled.
const char kPowerQuickDimEnabled[] = "power.quick_dim_enabled";

// Quick lock delay is used inside powerd to control the delay time for a screen
// lock to happen if the user is detected to be absent.
const char kPowerQuickLockDelay[] = "power.quick_lock_delay.ms";

// Boolean controlling whether the settings is enabled. This pref is intended to
// be set only by policy not by user.
const char kOsSettingsEnabled[] = "os_settings_enabled";

// |kShelfAlignment| and |kShelfAutoHideBehavior| have a local variant. The
// local variant is not synced and is used if set. If the local variant is not
// set its value is set from the synced value (once prefs have been
// synced). This gives a per-machine setting that is initialized from the last
// set value.
// These values are default on the machine but can be overridden by per-display
// values in kShelfPreferences (unless overridden by managed policy).
// String value corresponding to ash::ShelfAlignment (e.g. "Bottom").
const char kShelfAlignment[] = "shelf_alignment";
const char kShelfAlignmentLocal[] = "shelf_alignment_local";
// String value corresponding to ash::ShelfAutoHideBehavior (e.g. "Never").
const char kShelfAutoHideBehavior[] = "auto_hide_behavior";
const char kShelfAutoHideBehaviorLocal[] = "auto_hide_behavior_local";
const char kShelfAutoHideTabletModeBehavior[] =
    "auto_hide_tablet_mode_behavior";
const char kShelfAutoHideTabletModeBehaviorLocal[] =
    "auto_hide_tablet_mode_behavior_local";

// Dictionary value that determines when the launcher navigation nudge should
// show to the users.
const char kShelfLauncherNudge[] = "ash.shelf.launcher_nudge";

// Dictionary value that holds per-display preference of shelf alignment and
// auto-hide behavior. Key of the dictionary is the id of the display, and
// its value is a dictionary whose keys are kShelfAlignment and
// kShelfAutoHideBehavior.
const char kShelfPreferences[] = "shelf_preferences";

// Boolean pref indicating whether to show a logout button in the system tray.
const char kShowLogoutButtonInTray[] = "show_logout_button_in_tray";

// Integer pref indicating the length of time in milliseconds for which a
// confirmation dialog should be shown when the user presses the logout button.
// A value of 0 indicates that logout should happen immediately, without showing
// a confirmation dialog.
const char kLogoutDialogDurationMs[] = "logout_dialog_duration_ms";

// A boolean pref that when set to true, displays the logout confirmation
// dialog. If set to false, it prevents showing the dialog and the subsequent
// logout after closing the last window.
const char kSuggestLogoutAfterClosingLastWindow[] =
    "suggest_logout_after_closing_last_window";

// A dictionary pref that maps usernames to wallpaper info.
const char kUserWallpaperInfo[] = "user_wallpaper_info";

// An ordered list of hashed representations of IDs of Google Photos recently
// used as wallpapers for Daily Refresh.
const char kRecentDailyGooglePhotosWallpapers[] =
    "recent_daily_google_photos_wallpapers";

// A dictionary pref that maps usernames to wallpaper info.
// This is for wallpapers that are syncable across devices.
const char kSyncableWallpaperInfo[] = "syncable_wallpaper_info";

// A dictionary pref that maps wallpaper file paths to their prominent colors.
const char kWallpaperColors[] = "ash.wallpaper.prominent_colors";

// A dictionary pref that maps wallpaper file paths to their k mean colors.
const char kWallpaperMeanColors[] = "ash.wallpaper.k_mean_colors";

// A dictionary pref that maps wallpaper file paths to their celebi colors.
const char kWallpaperCelebiColors[] = "ash.wallpaper.celebi_colors";

// Boolean pref indicating whether a user has enabled the bluetooth adapter.
const char kUserBluetoothAdapterEnabled[] =
    "ash.user.bluetooth.adapter_enabled";

// Boolean pref indicating system-wide setting for bluetooth adapter power.
const char kSystemBluetoothAdapterEnabled[] =
    "ash.system.bluetooth.adapter_enabled";

// Boolean pref to persist the expanded state of the system tray across reboots.
const char kSystemTrayExpanded[] = "ash.system_tray.expanded";

// A boolean pref indicating whether the camera is allowed to be used.
const char kUserCameraAllowed[] = "ash.user.camera_allowed";

// A boolean pref indicating whether the microphone is allowed to be used.
const char kUserMicrophoneAllowed[] = "ash.user.microphone_allowed";

// A boolean pref indicating whether the geolocation is allowed to be used.
const char kUserGeolocationAllowed[] = "ash.user.geolocation_allowed";

// A boolean pref which determines whether tap-dragging is enabled.
const char kTapDraggingEnabled[] = "settings.touchpad.enable_tap_dragging";

// Boolean prefs for the status of the touchscreen and the touchpad.
const char kTouchpadEnabled[] = "events.touch_pad.enabled";
const char kTouchscreenEnabled[] = "events.touch_screen.enabled";

// Boolean value indicating that the touchpad scroll direction screen should be
// shown to the user during oobe.
const char kShowTouchpadScrollScreenEnabled[] =
    "ash.touchpad_scroll_screen_oobe_enabled";

// Integer prefs indicating the minimum and maximum lengths of the lock screen
// pin.
const char kPinUnlockMaximumLength[] = "pin_unlock_maximum_length";
const char kPinUnlockMinimumLength[] = "pin_unlock_minimum_length";

// Boolean pref indicating whether users are allowed to set easy pins.
const char kPinUnlockWeakPinsAllowed[] = "pin_unlock_weak_pins_allowed";

// An integer pref. Indicates the number of fingerprint records registered.
const char kQuickUnlockFingerprintRecord[] = "quick_unlock.fingerprint.record";

// A list of allowed quick unlock modes. A quick unlock mode can only be used if
// its type is on this list, or if type all (all quick unlock modes enabled) is
// on this list.
const char kQuickUnlockModeAllowlist[] = "quick_unlock_mode_allowlist";

// A list of allowed WebAuthn factors. A WebAuthn factor can only be
// used if its type is on this list, or if type all (all WebAuthn factors
// enabled) is on this list.
const char kWebAuthnFactors[] = "authfactors.restrictions.webauthn";

// String pref storing the salt for the pin quick unlock mechanism.
const char kQuickUnlockPinSalt[] = "quick_unlock.pin.salt";

// The hash for the pin quick unlock mechanism.
const char kQuickUnlockPinSecret[] = "quick_unlock.pin.secret";

// Enum that specifies how often a user has to enter their password to continue
// using quick unlock. These values are the same as the ones in
// `quick_unlock::PasswordConfirmationFrequency`.
// 0 - six hours. Users will have to enter their password every six hours.
// 1 - twelve hours. Users will have to enter their password every twelve hours.
// 2 - two days. Users will have to enter their password every two days.
// 3 - week. Users will have to enter their password every week.
const char kQuickUnlockTimeout[] = "quick_unlock_timeout";

// Dictionary prefs in local state that keeps information about detachable
// bases - for example the last used base per user.
const char kDetachableBaseDevices[] = "ash.detachable_base.devices";

// Pref storing the number of sessions in which Assistant onboarding was shown.
const char kAssistantNumSessionsWhereOnboardingShown[] =
    "ash.assistant.num_sessions_where_onboarding_shown";

// Pref storing the time of the last Assistant interaction.
const char kAssistantTimeOfLastInteraction[] =
    "ash.assistant.time_of_last_interaction";

// Whether the user is allowed to disconnect and configure VPN connections.
const char kVpnConfigAllowed[] = "vpn_config_allowed";

// A boolean pref that indicates whether power peak shift is enabled.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kPowerPeakShiftEnabled[] = "ash.power.peak_shift_enabled";

// An integer pref that specifies the power peak shift battery threshold in
// percent.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kPowerPeakShiftBatteryThreshold[] =
    "ash.power.peak_shift_battery_threshold";

// A dictionary pref that specifies the power peak shift day configs.
// For details see "DevicePowerPeakShiftDayConfig" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kPowerPeakShiftDayConfig[] = "ash.power.peak_shift_day_config";

// A boolean pref that indicates whether boot on AC is enabled.
const char kBootOnAcEnabled[] = "ash.power.boot_on_ac_enabled";

// A boolean pref that indicates whether advanced battery charge mode is
// enabled.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kAdvancedBatteryChargeModeEnabled[] =
    "ash.power.advanced_battery_charge_mode_enabled";

// A dictionary pref that specifies the advanced battery charge mode day config.
// For details see "DeviceAdvancedBatteryChargeModeDayConfig" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kAdvancedBatteryChargeModeDayConfig[] =
    "ash.power.advanced_battery_charge_mode_day_config";

// An integer pref that specifies the battery charge mode.
// For details see "DeviceBatteryChargeMode" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kBatteryChargeMode[] = "ash.power.battery_charge_mode";

// An integer pref that specifies the battery charge custom start charging in
// percent.
// For details see "DeviceBatteryChargeCustomStartCharging" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kBatteryChargeCustomStartCharging[] =
    "ash.power.battery_charge_custom_start_charging";

// An integer pref that specifies the battery charge custom stop charging in
// percent.
// For details see "DeviceBatteryChargeCustomStopCharging" in
// policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kBatteryChargeCustomStopCharging[] =
    "ash.power.battery_charge_custom_stop_charging";

// A boolean pref that indicates whether USB power share is enabled.
// For details see "DeviceUsbPowerShareEnabled" in policy_templates.json.
// Ignored unless powerd is configured to honor charging-related prefs.
const char kUsbPowerShareEnabled[] = "ash.power.usb_power_share_enabled";

// A bool pref to block the USB-C cable limiting device speed notification if it
// has already been clicked by the user.
const char kUsbPeripheralCableSpeedNotificationShown[] =
    "ash.usb_peripheral_cable_speed_notification_shown";

// A dictionary value that determines whether the reorder nudge in app list
// should show to the users.
const char kAppListReorderNudge[] = "ash.launcher.app_list_reorder_nudge";

// A dictionary pref that stores information related to the privacy notice in
// the continue files section for the launcher.
const char kLauncherFilesPrivacyNotice[] =
    "ash.launcher.continue_section_privacy_notice";

// A boolean pref that indicates whether lock screen media controls are enabled.
// Controlled by user policy.
const char kLockScreenMediaControlsEnabled[] =
    "ash.lock_screen_media_controls_enabled";

// Boolean pref which determines whether key repeat is enabled.
const char kXkbAutoRepeatEnabled[] =
    "settings.language.xkb_auto_repeat_enabled_r2";

// Integer pref which determines key repeat delay (in ms).
const char kXkbAutoRepeatDelay[] = "settings.language.xkb_auto_repeat_delay_r2";

// Integer pref which determines key repeat interval (in ms).
const char kXkbAutoRepeatInterval[] =
    "settings.language.xkb_auto_repeat_interval_r2";
// "_r2" suffixes were added to the three prefs above when we changed the
// preferences to not be user-configurable or sync with the cloud. The prefs are
// now user-configurable and syncable again, but we don't want to overwrite the
// current values with the old synced values, so we continue to use this suffix.

// A boolean pref that causes top-row keys to be interpreted as function keys
// instead of as media keys.
const char kSendFunctionKeys[] = "settings.language.send_function_keys";

// A boolean pref which is true if touchpad reverse scroll is enabled.
const char kNaturalScroll[] = "settings.touchpad.natural_scroll";
// A boolean pref which is true if mouse reverse scroll is enabled.
const char kMouseReverseScroll[] = "settings.mouse.reverse_scroll";

// A dictionary storing the number of times and most recent time the multipaste
// contextual nudge was shown.
const char kMultipasteNudges[] = "ash.clipboard.multipaste_nudges";

// A boolean pref that indicates whether app badging is shown in launcher and
// shelf.
const char kAppNotificationBadgingEnabled[] =
    "ash.app_notification_badging_enabled";

// An integer pref that indicates whether global media controls is pinned to
// shelf or it's unset and need to be determined by screen size during runtime.
const char kGlobalMediaControlsPinned[] =
    "ash.system.global_media_controls_pinned";

// An integer pref that tracks how many times the user is able to click on
// PciePeripheral-related notifications before hiding new notifications.
const char kPciePeripheralDisplayNotificationRemaining[] =
    "ash.pcie_peripheral_display_notification_remaining";

// Boolean prefs storing whether various IME-related keyboard shortcut reminders
// have previously been dismissed or not.
const char kLastUsedImeShortcutReminderDismissed[] =
    "ash.shortcut_reminders.last_used_ime_dismissed";
const char kNextImeShortcutReminderDismissed[] =
    "ash.shortcut_reminders.next_ime_dismissed";

// Boolean pref to indicate whether to use i18n shortcut mapping and deprecate
// legacy shortcuts.
const char kDeviceI18nShortcutsEnabled[] = "ash.device_i18n_shortcuts_enabled";

// If a user installs an extension which controls the proxy settings in the
// primary profile of Chrome OS, this dictionary will contain information about
// the extension controlling the proxy (name, id and if it can be disabled by
// the user). Used to show the name and icon of the extension in the "Proxy"
// section of the OS Settings>Network dialog.
const char kLacrosProxyControllingExtension[] =
    "ash.lacros_proxy_controlling_extension";

// A boolean pref which is true if Fast Pair is enabled.
const char kFastPairEnabled[] = "ash.fast_pair.enabled";

// Boolean pref indicating a user entered Bluetooth pairing flow from a fast
// pair entry point.
const char kUserPairedWithFastPair[] = "ash.user.paired_with_fast_pair";

// A boolean pref that controls whether the user is allowed to use the Desk
// Templates feature - including creating Desks templates and using predefined
// Desks templates.
const char kDeskTemplatesEnabled[] = "ash.desk_templates_enabled";

// A string pref which contains download URLs and hashes for files containing
// predefined Desks templates configured by policy administrators.
const char kPreconfiguredDeskTemplates[] = "ash.preconfigured_desk_templates";

// An unsigned integer pref which contains the last used marker color for
// Projector.
const char kProjectorAnnotatorLastUsedMarkerColor[] =
    "ash.projector.annotator_last_used_marker_color";

// A boolean pref that tracks whether the user has enabled Projector creation
// flow during onboarding.
const char kProjectorCreationFlowEnabled[] =
    "ash.projector.creation_flow_enabled";

// A string pref that tracks the language installed for the Projector creation
// flow.
const char kProjectorCreationFlowLanguage[] =
    "ash.projector.creation_flow_language";

// An integer pref counting the number of times the Onboarding flow has been
// shown to the user inside the Projector Gallery.
const char kProjectorGalleryOnboardingShowCount[] =
    "ash.projector.gallery_onboarding_show_count";

// An integer pref counting the number of times the Onboarding flow has been
// shown to the user inside the Projector Viewer.
const char kProjectorViewerOnboardingShowCount[] =
    "ash.projector.viewer_onboarding_show_count";

// A boolean pref that indicates the the exclude-transcript dialog has been
// shown.
const char kProjectorExcludeTranscriptDialogShown[] =
    "ash.projector.exclude_transcript_dialog_shown";

// A boolean pref that indicates the Projector has been enabled by admin
// policy.
const char kProjectorAllowByPolicy[] = "ash.projector.allow_by_policy";

// A boolean pref that controls Projector dogfood for Family Link users.
// Set with an enterprise user policy.
const char kProjectorDogfoodForFamilyLinkEnabled[] =
    "ash.projector.dogfood_for_family_link_enabled";

// A boolean pref that indicates whether the migration of Chromad devices to
// cloud management can be started.
const char kChromadToCloudMigrationEnabled[] =
    "ash.chromad_to_cloud_migration_enabled";

// List of Drive Folder Shortcuts in the Files app. Used to sync the shortcuts
// across devices.
const char kFilesAppFolderShortcuts[] = "ash.filesapp.folder_shortcuts";

// A boolean pref that indicates if the Files app UI prefs have migrated from
// the Chrome app to System Web App.
const char kFilesAppUIPrefsMigrated[] = "ash.filesapp.ui_prefs_migrated";

// A boolean pref that indicates if files can be trashed (on a supported
// filesystem) or must be always permanently deleted.
const char kFilesAppTrashEnabled[] = "ash.filesapp.trash_enabled";

// Boolean value for the DeviceLoginScreenWebUILazyLoading device policy.
const char kLoginScreenWebUILazyLoading[] =
    "ash.login.LoginScreenWebUILazyLoading";

// Boolean value for the FloatingWorkspaceEnabled policy
const char kFloatingWorkspaceEnabled[] = "ash.floating_workspace_enabled";

// Boolean value for the FloatingWorkspaceV2Enabled policy
const char kFloatingWorkspaceV2Enabled[] = "ash.floating_workspace_v2_enabled";

// Boolean value indicating that post reboot notification should be shown to the
// user.
const char kShowPostRebootNotification[] = "ash.show_post_reboot_notification";

// Boolean value indicating that the USB device detected notification should be
// shown to the user.
const char kUsbDetectorNotificationEnabled[] =
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
const char kPersonalizationKeyboardBacklightColor[] =
    "ash.personalization.keyboard_backlight_color";

// A dictionary pref storing the color of each zone of the RGB keyboard. The key
// specifies the zone .e.g. `zone-1`, `zone-2`, whereas the value is a
// `personalization_app::mojom::BacklightColor`.
const char kPersonalizationKeyboardBacklightZoneColors[] =
    "ash.personalization.keyboard_backlight_zone_colors";

// Integer pref corresponding to the autozoom state, the value should be one of
// cros::mojom::CameraAutoFramingState.
const char kAutozoomState[] = "ash.camera.autozoom_state";

// A dictionary storing the number of times and most recent time the autozoom
// nudge was shown.
const char kAutozoomNudges[] = "ash.camera.autozoom_nudges";

// An boolean pref that specifies the recovery service activation for user.
// When the pref is set to `true`, the user data recovery is activated. When the
// pref is set to `false`, the user data recovery is not activated. The default
// value of the pref is `true`. Controlled by RecoveryFactorBehavior policy.
const char kRecoveryFactorBehavior[] = "ash.recovery.recovery_factor_behavior";

// Pref which stores ICCIDs of cellular networks that have been migrated to the
// APN Revamp feature.
const char kApnMigratedIccids[] = "ash.cellular.apn_migrated_iccids";

// An integer pref that indicates the background blur level that is applied.
// -1 means disabled.
const char kBackgroundBlur[] = "ash.camera.background_blur";

// An boolean pref that indicates whether background replacement is applied.
const char kBackgroundReplace[] = "ash.camera.background_replace";

// An boolean pref that indicates whether portrait relighting is applied.
const char kPortraitRelighting[] = "ash.camera.portrait_relighting";

// Specifies if ARC app sync metrics should be recorded, i.e. this is the
// initial session after sync consent screen.
const char kRecordArcAppSyncMetrics[] =
    "ash.should_record_arc_app_sync_metrics";

// A boolean pref set to true if primary mouse button is the left button.
const char kPrimaryMouseButtonRight[] = "settings.mouse.primary_right";

// A integer pref for the touchpad sensitivity.
const char kMouseSensitivity[] = "settings.mouse.sensitivity2";

// A boolean pref set to true if mouse acceleration is enabled. When disabled
// only simple linear scaling is applied based on sensitivity.
const char kMouseAcceleration[] = "settings.mouse.acceleration";

// A integer pref for the touchpad scroll sensitivity, in the range
// [PointerSensitivity::kLowest, PointerSensitivity::kHighest].
const char kMouseScrollSensitivity[] = "settings.mouse.scroll_sensitivity";

// A boolean pref set to true if mouse scroll acceleration is enabled. When
// disabled, only simple linear scaling is applied based on sensitivity.
const char kMouseScrollAcceleration[] = "settings.mouse.scroll_acceleration";

// NOTE: New prefs should start with the "ash." prefix. Existing prefs moved
// into this file should not be renamed, since they may be synced.

}  // namespace prefs
}  // namespace ash
