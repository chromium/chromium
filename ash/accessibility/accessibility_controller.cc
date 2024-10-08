// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accessibility/accessibility_controller.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_notification_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/accessibility/disable_trackpad_event_rewriter.h"
#include "ash/accessibility/filter_keys_event_rewriter.h"
#include "ash/accessibility/flash_screen_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/switch_access/point_scan_controller.h"
#include "ash/accessibility/ui/accessibility_highlight_controller.h"
#include "ash/accessibility/ui/accessibility_panel_layout_manager.h"
#include "ash/color_enhancement/color_enhancement_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/events/accessibility_event_rewriter.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/events/select_to_speak_event_handler.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/login_status.h"
#include "ash/policy/policy_recommendation_restorer.h"
#include "ash/public/cpp/accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/accessibility_feature_disable_dialog.h"
#include "ash/system/accessibility/dictation_bubble_controller.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/facegaze_bubble_controller.h"
#include "ash/system/accessibility/floating_accessibility_controller.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_bubble_controller.h"
#include "ash/system/accessibility/switch_access/switch_access_menu_bubble_controller.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/wm/window_util.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/animation/animation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"

using session_manager::SessionState;

namespace ash {
namespace {

// How much distance to travel with each generated scroll event.
const int kScrollDelta = 40;

AccessibilityController* g_instance = nullptr;

using FeatureType = A11yFeatureType;

// These classes are used to store the static configuration for a11y features.
struct FeatureData {
  FeatureType type;
  const char* pref;
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #global-scope
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  const int name_resource_id;
  bool toggleable_in_quicksettings = true;
  FeatureType conflicting_feature = FeatureType::kNoConflictingFeature;
};

struct FeatureDialogData {
  FeatureType type;
  const char* pref;
  int title;
  int body;
};

// A static array describing each feature.
const FeatureData kFeatures[] = {
    {FeatureType::kAutoclick, prefs::kAccessibilityAutoclickEnabled,
     &kSystemMenuAccessibilityAutoClickIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK},
    {FeatureType::kCaretHighlight, prefs::kAccessibilityCaretHighlightEnabled,
     nullptr, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_CARET_HIGHLIGHT},
    {FeatureType::kCursorHighlight, prefs::kAccessibilityCursorHighlightEnabled,
     nullptr, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_MOUSE_CURSOR},
    {FeatureType::kCursorColor, prefs::kAccessibilityCursorColorEnabled,
     nullptr, 0, /*toggleable_in_quicksettings=*/false},
    {FeatureType::kDictation, prefs::kAccessibilityDictationEnabled,
     &kDictationMenuIcon, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION},
    {FeatureType::kColorCorrection, prefs::kAccessibilityColorCorrectionEnabled,
     &kColorCorrectionIcon, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_COLOR_CORRECTION},
    {FeatureType::kFlashNotifications,
     prefs::kAccessibilityFlashNotificationsEnabled, nullptr, 0,
     /*toggleable_in_quicksettings=*/false},
    {FeatureType::kFocusHighlight, prefs::kAccessibilityFocusHighlightEnabled,
     nullptr, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_KEYBOARD_FOCUS,
     /*toggleable_in_quicksettings=*/true,
     /* conflicting_feature= */ FeatureType::kSpokenFeedback},
    {FeatureType::kFloatingMenu, prefs::kAccessibilityFloatingMenuEnabled,
     nullptr, IDS_ASH_FLOATING_ACCESSIBILITY_MAIN_MENU,
     /*toggleable_in_quicksettings=*/false},
    {FeatureType::kFullscreenMagnifier,
     prefs::kAccessibilityScreenMagnifierEnabled,
     &kSystemMenuAccessibilityFullscreenMagnifierIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER},
    {FeatureType::kDockedMagnifier, prefs::kDockedMagnifierEnabled,
     &kSystemMenuAccessibilityDockedMagnifierIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DOCKED_MAGNIFIER},
    {FeatureType::kHighContrast, prefs::kAccessibilityHighContrastEnabled,
     &kSystemMenuAccessibilityContrastIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE},
    {FeatureType::kLargeCursor, prefs::kAccessibilityLargeCursorEnabled,
     nullptr, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR},
    {FeatureType::kLiveCaption, ::prefs::kLiveCaptionEnabled,
     &vector_icons::kLiveCaptionOnIcon, IDS_ASH_STATUS_TRAY_LIVE_CAPTION},
    {FeatureType::kMonoAudio, prefs::kAccessibilityMonoAudioEnabled, nullptr,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MONO_AUDIO},
    {FeatureType::kMouseKeys, prefs::kAccessibilityMouseKeysEnabled, nullptr, 0,
     /*toggleable_in_quicksettings=*/false},
    {FeatureType::kSpokenFeedback, prefs::kAccessibilitySpokenFeedbackEnabled,
     &kSystemMenuAccessibilityChromevoxIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK},
    {FeatureType::kReducedAnimations,
     prefs::kAccessibilityReducedAnimationsEnabled, nullptr, 0,
     /*toggleable_in_quicksettings=*/false},
    {FeatureType::kSelectToSpeak, prefs::kAccessibilitySelectToSpeakEnabled,
     &kSystemMenuAccessibilitySelectToSpeakIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK},
    {FeatureType::kStickyKeys, prefs::kAccessibilityStickyKeysEnabled, nullptr,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_STICKY_KEYS,
     /*toggleable_in_quicksettings=*/true,
     /*conflicting_feature=*/FeatureType::kSpokenFeedback},
    {FeatureType::kSwitchAccess, prefs::kAccessibilitySwitchAccessEnabled,
     &kSwitchAccessIcon, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SWITCH_ACCESS},
    {FeatureType::kVirtualKeyboard, prefs::kAccessibilityVirtualKeyboardEnabled,
     &kSystemMenuKeyboardLegacyIcon,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD},
    {FeatureType::kFaceGaze, prefs::kAccessibilityFaceGazeEnabled, nullptr,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_FACEGAZE,
     /*toggleable_in_quicksettings=*/true},
    {FeatureType::kDisableTrackpad, prefs::kAccessibilityDisableTrackpadEnabled,
     nullptr, 0, /*toggleable_in_quicksettings=*/false},
};

// An array describing the confirmation dialogs for the features which have
// them.
const FeatureDialogData kFeatureDialogs[] = {
    {FeatureType::kFullscreenMagnifier,
     prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted},
    {FeatureType::kDockedMagnifier,
     prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted},
    {FeatureType::kHighContrast,
     prefs::kHighContrastAcceleratorDialogHasBeenAccepted}};

constexpr char kNotificationId[] = "chrome://settings/accessibility";
constexpr char kNotifierAccessibility[] = "ash.accessibility";
constexpr char kDictationLanguageUpgradedNudgeId[] =
    "dictation_language_upgraded.nudge_id";

// TODO(warx): Signin screen has more controllable accessibility prefs. We may
// want to expand this to a complete list. If so, merge this with
// |kCopiedOnSigninAccessibilityPrefs|.
constexpr const char* const kA11yPrefsForRecommendedValueOnSignin[]{
    prefs::kAccessibilityLargeCursorEnabled,
    prefs::kAccessibilityHighContrastEnabled,
    prefs::kAccessibilityScreenMagnifierEnabled,
    prefs::kAccessibilitySpokenFeedbackEnabled,
    prefs::kAccessibilityVirtualKeyboardEnabled,
};

// List of accessibility prefs that are to be copied (if changed by the user) on
// signin screen profile to a newly created user profile or a guest session.
constexpr const char* const kCopiedOnSigninAccessibilityPrefs[]{
    prefs::kAccessibilityAutoclickDelayMs,
    prefs::kAccessibilityAutoclickEnabled,
    prefs::kAccessibilityCaretHighlightEnabled,
    prefs::kAccessibilityChromeVoxAutoRead,
    prefs::kAccessibilityChromeVoxAnnounceDownloadNotifications,
    prefs::kAccessibilityChromeVoxAnnounceRichTextAttributes,
    prefs::kAccessibilityChromeVoxAudioStrategy,
    prefs::kAccessibilityChromeVoxBrailleSideBySide,
    prefs::kAccessibilityChromeVoxBrailleTable,
    prefs::kAccessibilityChromeVoxBrailleTable6,
    prefs::kAccessibilityChromeVoxBrailleTable8,
    prefs::kAccessibilityChromeVoxBrailleTableType,
    prefs::kAccessibilityChromeVoxBrailleWordWrap,
    prefs::kAccessibilityChromeVoxCapitalStrategy,
    prefs::kAccessibilityChromeVoxCapitalStrategyBackup,
    prefs::kAccessibilityChromeVoxEnableBrailleLogging,
    prefs::kAccessibilityChromeVoxEnableEarconLogging,
    prefs::kAccessibilityChromeVoxEnableEventStreamLogging,
    prefs::kAccessibilityChromeVoxEnableSpeechLogging,
    prefs::kAccessibilityChromeVoxEventStreamFilters,
    prefs::kAccessibilityChromeVoxLanguageSwitching,
    prefs::kAccessibilityChromeVoxMenuBrailleCommands,
    prefs::kAccessibilityChromeVoxNumberReadingStyle,
    prefs::kAccessibilityChromeVoxPreferredBrailleDisplayAddress,
    prefs::kAccessibilityChromeVoxPunctuationEcho,
    prefs::kAccessibilityChromeVoxSmartStickyMode,
    prefs::kAccessibilityChromeVoxSpeakTextUnderMouse,
    prefs::kAccessibilityChromeVoxUsePitchChanges,
    prefs::kAccessibilityChromeVoxUseVerboseMode,
    prefs::kAccessibilityChromeVoxVirtualBrailleColumns,
    prefs::kAccessibilityChromeVoxVirtualBrailleRows,
    prefs::kAccessibilityChromeVoxVoiceName,
    prefs::kAccessibilityColorCorrectionEnabled,
    prefs::kAccessibilityCursorHighlightEnabled,
    prefs::kAccessibilityCursorColorEnabled,
    prefs::kAccessibilityCursorColor,
    prefs::kAccessibilityDictationEnabled,
    prefs::kAccessibilityDictationLocale,
    prefs::kAccessibilityDictationLocaleOfflineNudge,
    prefs::kAccessibilityDisableTrackpadEnabled,
    prefs::kAccessibilityDisableTrackpadMode,
    prefs::kAccessibilityFocusHighlightEnabled,
    prefs::kAccessibilityHighContrastEnabled,
    prefs::kAccessibilityLargeCursorEnabled,
    prefs::kAccessibilityFaceGazeEnabled,
    prefs::kAccessibilityMonoAudioEnabled,
    prefs::kAccessibilityReducedAnimationsEnabled,
    prefs::kAccessibilityMouseKeysEnabled,
    prefs::kAccessibilityMouseKeysAcceleration,
    prefs::kAccessibilityMouseKeysMaxSpeed,
    prefs::kAccessibilityMouseKeysUsePrimaryKeys,
    prefs::kAccessibilityMouseKeysDominantHand,
    prefs::kAccessibilityScreenMagnifierEnabled,
    prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled,
    prefs::kAccessibilityMagnifierFollowsChromeVox,
    prefs::kAccessibilityMagnifierFollowsSts,
    prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
    prefs::kAccessibilityScreenMagnifierScale,
    prefs::kAccessibilitySelectToSpeakEnabled,
    prefs::kAccessibilitySpokenFeedbackEnabled,
    prefs::kAccessibilityStickyKeysEnabled,
    prefs::kAccessibilityShortcutsEnabled,
    prefs::kAccessibilitySwitchAccessEnabled,
    prefs::kAccessibilityVirtualKeyboardEnabled,
    prefs::kDockedMagnifierEnabled,
    prefs::kDockedMagnifierScale,
    prefs::kDockedMagnifierScreenHeightDivisor,
    prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    prefs::kDictationAcceleratorDialogHasBeenAccepted,
    prefs::kDictationDlcSuccessNotificationHasBeenShown,
    prefs::kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown,
    prefs::kDictationDlcOnlySodaDownloadedNotificationHasBeenShown,
    prefs::kDictationNoDlcsDownloadedNotificationHasBeenShown,
    prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2,
    prefs::kSelectToSpeakAcceleratorDialogHasBeenAccepted,
    prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted,
    prefs::kFaceGazeDlcSuccessNotificationHasBeenShown,
    prefs::kFaceGazeDlcFailureNotificationHasBeenShown,
};

// List of switch access accessibility prefs that are to be copied (if changed
// by the user) from the current user to the signin screen profile. That way
// if a switch access user signs out, their switch continues to function.
constexpr const char* const kSwitchAccessPrefsCopiedToSignin[]{
    prefs::kAccessibilitySwitchAccessAutoScanEnabled,
    prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
    prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
    prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond,
    prefs::kAccessibilitySwitchAccessEnabled,
    prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes,
    prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes,
    prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes,
};

// Helper function that is used to verify the validity of kFeatures and
// kFeatureDialogs.
bool VerifyFeaturesData() {
  // All feature prefs must be unique.
  std::set<const char*> feature_prefs;
  for (auto feature_data : kFeatures) {
    if (base::Contains(feature_prefs, feature_data.pref)) {
      return false;
    }
    feature_prefs.insert(feature_data.pref);
  }

  for (auto dialog_data : kFeatureDialogs) {
    if (base::Contains(feature_prefs, dialog_data.pref)) {
      return false;
    }
    feature_prefs.insert(dialog_data.pref);
  }

  return true;
}

// Returns true if |pref_service| is the one used for the signin screen.
bool IsSigninPrefService(PrefService* pref_service) {
  const PrefService* signin_pref_service =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(signin_pref_service);
  return pref_service == signin_pref_service;
}

// Returns true if the current session is the guest session.
bool IsCurrentSessionGuest() {
  const std::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return user_type && *user_type == user_manager::UserType::kGuest;
}

bool IsUserFirstLogin() {
  return Shell::Get()->session_controller()->IsUserFirstLogin();
}

// The copying of any modified accessibility prefs on the signin prefs happens
// when the |previous_pref_service| is of the signin profile, and the
// |current_pref_service| is of a newly created profile first logged in, or if
// the current session is the guest session.
bool ShouldCopySigninPrefs(PrefService* previous_pref_service,
                           PrefService* current_pref_service) {
  DCHECK(previous_pref_service);
  if (IsUserFirstLogin() && IsSigninPrefService(previous_pref_service) &&
      !IsSigninPrefService(current_pref_service)) {
    // If the user set a pref value on the login screen and is now starting a
    // session with a new profile, copy the pref value to the profile.
    return true;
  }

  if (IsCurrentSessionGuest()) {
    // Guest sessions don't have their own prefs, so always copy.
    return true;
  }

  return false;
}

// On a user's first login into a device, any a11y features enabled/disabled
// by the user on the login screen are enabled/disabled in the user's profile.
// This function copies settings from the signin prefs into the user's prefs
// when it detects a login with a newly created profile.
void CopySigninPrefsIfNeeded(PrefService* previous_pref_service,
                             PrefService* current_pref_service) {
  DCHECK(current_pref_service);
  if (!ShouldCopySigninPrefs(previous_pref_service, current_pref_service)) {
    return;
  }

  PrefService* signin_prefs =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(signin_prefs);
  for (const auto* pref_path : kCopiedOnSigninAccessibilityPrefs) {
    const PrefService::Preference* pref =
        signin_prefs->FindPreference(pref_path);

    // Ignore if the pref has not been set by the user.
    if (!pref || !pref->IsUserControlled()) {
      continue;
    }

    // Copy the pref value from the signin profile.
    const base::Value* value_on_login = pref->GetValue();
    current_pref_service->Set(pref_path, *value_on_login);
  }
}

// Returns notification icon based on the A11yNotificationType.
const gfx::VectorIcon& GetNotificationIcon(A11yNotificationType type) {
  switch (type) {
    case A11yNotificationType::kSpokenFeedbackBrailleEnabled:
    case A11yNotificationType::kTrackpadDisabled:
      return kNotificationAccessibilityIcon;
    case A11yNotificationType::kBrailleDisplayConnected:
      return kNotificationAccessibilityBrailleIcon;
    case A11yNotificationType::kSwitchAccessEnabled:
      return kSwitchAccessIcon;
    case A11yNotificationType::kDictationAllDlcsDownloaded:
    case A11yNotificationType::kDictationNoDlcsDownloaded:
    case A11yNotificationType::kDicationOnlyPumpkinDownloaded:
    case A11yNotificationType::kDictationOnlySodaDownloaded:
      return kDictationMenuIcon;
    default:
      return kNotificationChromevoxIcon;
  }
}

void ShowAccessibilityNotification(
    const AccessibilityController::A11yNotificationWrapper& wrapper) {
  A11yNotificationType type = wrapper.type;
  const auto& replacements = wrapper.replacements;
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kNotificationId, false /* by_user */);

  if (type == A11yNotificationType::kNone) {
    return;
  }

  std::u16string text;
  std::u16string title;
  std::u16string display_source;
  auto catalog_name = NotificationCatalogName::kNone;
  bool pinned = true;
  message_center::SystemNotificationWarningLevel warning =
      message_center::SystemNotificationWarningLevel::NORMAL;

  message_center::RichNotificationData options;
  scoped_refptr<message_center::NotificationDelegate> delegate;

  if (wrapper.callback.has_value()) {
    delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            wrapper.callback.value());
  }

  if (type == A11yNotificationType::kBrailleDisplayConnected) {
    title = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BRAILLE_DISPLAY_CONNECTED);
    catalog_name = NotificationCatalogName::kBrailleDisplayConnected;
  } else if (type == A11yNotificationType::kDictationAllDlcsDownloaded) {
    display_source =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION);
    title = l10n_util::GetStringFUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_ALL_DLCS_DOWNLOADED_TITLE,
        replacements, nullptr);
    text = l10n_util::GetStringUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_ALL_DLCS_DOWNLOADED_DESC);
    catalog_name = NotificationCatalogName::kDictationAllDlcsDownloaded;
    pinned = false;
  } else if (type == A11yNotificationType::kDictationNoDlcsDownloaded) {
    display_source =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION);
    title = l10n_util::GetStringFUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_NO_DLCS_DOWNLOADED_TITLE,
        replacements, nullptr);
    text = l10n_util::GetStringUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_NO_DLCS_DOWNLOADED_DESC);
    catalog_name = NotificationCatalogName::kDictationNoDlcsDownloaded;
    pinned = false;
    // Use CRITICAL_WARNING to force the notification color to red.
    warning = message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else if (type == A11yNotificationType::kDicationOnlyPumpkinDownloaded) {
    display_source =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION);

    title = l10n_util::GetStringFUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_ONLY_PUMPKIN_DOWNLOADED_TITLE,
        replacements, nullptr);
    text = l10n_util::GetStringUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_ONLY_PUMPKIN_DOWNLOADED_DESC);

    catalog_name = NotificationCatalogName::kDicationOnlyPumpkinDownloaded;
    pinned = false;
    // Use CRITICAL_WARNING to force the notification color to red.
    warning = message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else if (type == A11yNotificationType::kDictationOnlySodaDownloaded) {
    display_source =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION);
    title = l10n_util::GetStringFUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_ONLY_SODA_DOWNLOADED_TITLE,
        replacements, nullptr);
    text = l10n_util::GetStringUTF16(
        IDS_ASH_A11Y_DICTATION_NOTIFICATION_ONLY_SODA_DOWNLOADED_DESC);
    catalog_name = NotificationCatalogName::kDictationOnlySodaDownloaded;
    pinned = false;
    // Use CRITICAL_WARNING to force the notification color to red.
    warning = message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else if (type == A11yNotificationType::kFaceGazeAssetsDownloaded) {
    title = l10n_util::GetStringUTF16(
        IDS_ASH_A11Y_FACEGAZE_ASSETS_DOWNLOADED_TITLE);
    text =
        l10n_util::GetStringUTF16(IDS_ASH_A11Y_FACEGAZE_ASSETS_DOWNLOADED_DESC);
    catalog_name = NotificationCatalogName::kFaceGazeAssetsDownloaded;
    pinned = false;
  } else if (type == A11yNotificationType::kFaceGazeAssetsFailed) {
    title =
        l10n_util::GetStringUTF16(IDS_ASH_A11Y_FACEGAZE_ASSETS_FAILED_TITLE);
    text = l10n_util::GetStringUTF16(IDS_ASH_A11Y_FACEGAZE_ASSETS_FAILED_DESC);
    catalog_name = NotificationCatalogName::kFaceGazeAssetsFailed;
    pinned = false;
    // Use CRITICAL_WARNING to force the notification color to red.
    warning = message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else if (type == A11yNotificationType::kSwitchAccessEnabled) {
    title = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SWITCH_ACCESS_ENABLED_TITLE);
    text = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SWITCH_ACCESS_ENABLED);
    catalog_name = NotificationCatalogName::kSwitchAccessEnabled;
  } else if (type == A11yNotificationType::kTrackpadDisabled) {
    title =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_TRACKPAD_DISABLED_TITLE);
    text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_TRACKPAD_DISABLED_DESCRIPTION);
    catalog_name = NotificationCatalogName::kTrackpadDisabled;
    options.pinned = true;
    options.buttons.emplace_back(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_TRACKPAD_DISABLED_TURN_ON));

  } else {
    bool is_tablet = display::Screen::GetScreen()->InTabletMode();

    title = l10n_util::GetStringUTF16(
        type == A11yNotificationType::kSpokenFeedbackBrailleEnabled
            ? IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_BRAILLE_ENABLED_TITLE
            : IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_TITLE);
    text = l10n_util::GetStringUTF16(
        is_tablet ? IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_TABLET
                  : IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED);
    catalog_name = type == A11yNotificationType::kSpokenFeedbackBrailleEnabled
                       ? NotificationCatalogName::kSpokenFeedbackBrailleEnabled
                       : NotificationCatalogName::kSpokenFeedbackEnabled;
  }

  options.should_make_spoken_feedback_for_popup_updates = false;
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title,
          text, display_source, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierAccessibility, catalog_name),
          options, delegate, GetNotificationIcon(type), warning);
  notification->set_pinned(pinned);
  message_center->AddNotification(std::move(notification));
}

void RemoveAccessibilityNotification() {
  ShowAccessibilityNotification(
      AccessibilityController::A11yNotificationWrapper(
          A11yNotificationType::kNone, std::vector<std::u16string>()));
}

AccessibilityPanelLayoutManager* GetLayoutManager() {
  // The accessibility panel is only shown on the primary display.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::Window* container =
      Shell::GetContainer(root, kShellWindowId_AccessibilityPanelContainer);
  // TODO(jamescook): Avoid this cast by moving ash::AccessibilityObserver
  // ownership to this class and notifying it on accessibility panel fullscreen
  // updates.
  return static_cast<AccessibilityPanelLayoutManager*>(
      container->layout_manager());
}

std::string PrefKeyForSwitchAccessCommand(SwitchAccessCommand command) {
  switch (command) {
    case SwitchAccessCommand::kSelect:
      return prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes;
    case SwitchAccessCommand::kNext:
      return prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes;
    case SwitchAccessCommand::kPrevious:
      return prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes;
    case SwitchAccessCommand::kNone:
      NOTREACHED();
  }
}

std::string UmaNameForSwitchAccessCommand(SwitchAccessCommand command) {
  switch (command) {
    case SwitchAccessCommand::kSelect:
      return "Accessibility.CrosSwitchAccess.SelectKeyCode";
    case SwitchAccessCommand::kNext:
      return "Accessibility.CrosSwitchAccess.NextKeyCode";
    case SwitchAccessCommand::kPrevious:
      return "Accessibility.CrosSwitchAccess.PreviousKeyCode";
    case SwitchAccessCommand::kNone:
      NOTREACHED();
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SwitchAccessKeyCode {
  kUnknown = 0,
  kKeycode1 = 1,
  kKeycode2 = 2,
  kKeycode3 = 3,
  kKeycode4 = 4,
  kKeycode5 = 5,
  kKeycode6 = 6,
  kKeycode7 = 7,
  kBackspace = 8,
  kTab = 9,
  kKeycode10 = 10,
  kKeycode11 = 11,
  kClear = 12,
  kReturn = 13,
  kKeycode14 = 14,
  kKeycode15 = 15,
  kShift = 16,
  kControl = 17,
  kAlt = 18,
  kPause = 19,
  kCapital = 20,
  kKana = 21,
  kKeycode22 = 22,
  kJunja = 23,
  kFinal = 24,
  kHanja = 25,
  kKeycode26 = 26,
  kEscape = 27,
  kConvert = 28,
  kNonconvert = 29,
  kAccept = 30,
  kModechange = 31,
  kSpace = 32,
  kPrior = 33,
  kNext = 34,
  kEnd = 35,
  kHome = 36,
  kLeft = 37,
  kUp = 38,
  kRight = 39,
  kDown = 40,
  kSelect = 41,
  kPrint = 42,
  kExecute = 43,
  kSnapshot = 44,
  kInsert = 45,
  kKeyDelete = 46,
  kHelp = 47,
  kNum0 = 48,
  kNum1 = 49,
  kNum2 = 50,
  kNum3 = 51,
  kNum4 = 52,
  kNum5 = 53,
  kNum6 = 54,
  kNum7 = 55,
  kNum8 = 56,
  kNum9 = 57,
  kKeycode58 = 58,
  kKeycode59 = 59,
  kKeycode60 = 60,
  kKeycode61 = 61,
  kKeycode62 = 62,
  kKeycode63 = 63,
  kKeycode64 = 64,
  kA = 65,
  kB = 66,
  kC = 67,
  kD = 68,
  kE = 69,
  kF = 70,
  kG = 71,
  kH = 72,
  kI = 73,
  kJ = 74,
  kK = 75,
  kL = 76,
  kM = 77,
  kN = 78,
  kO = 79,
  kP = 80,
  kQ = 81,
  kR = 82,
  kS = 83,
  kT = 84,
  kU = 85,
  kV = 86,
  kW = 87,
  kX = 88,
  kY = 89,
  kZ = 90,
  kLwin = 91,
  kRwin = 92,
  kApps = 93,
  kKeycode94 = 94,
  kSleep = 95,
  kNumpad0 = 96,
  kNumpad1 = 97,
  kNumpad2 = 98,
  kNumpad3 = 99,
  kNumpad4 = 100,
  kNumpad5 = 101,
  kNumpad6 = 102,
  kNumpad7 = 103,
  kNumpad8 = 104,
  kNumpad9 = 105,
  kMultiply = 106,
  kAdd = 107,
  kSeparator = 108,
  kSubtract = 109,
  kDecimal = 110,
  kDivide = 111,
  kF1 = 112,
  kF2 = 113,
  kF3 = 114,
  kF4 = 115,
  kF5 = 116,
  kF6 = 117,
  kF7 = 118,
  kF8 = 119,
  kF9 = 120,
  kF10 = 121,
  kF11 = 122,
  kF12 = 123,
  kF13 = 124,
  kF14 = 125,
  kF15 = 126,
  kF16 = 127,
  kF17 = 128,
  kF18 = 129,
  kF19 = 130,
  kF20 = 131,
  kF21 = 132,
  kF22 = 133,
  kF23 = 134,
  kF24 = 135,
  kKeycode136 = 136,
  kKeycode137 = 137,
  kKeycode138 = 138,
  kKeycode139 = 139,
  kKeycode140 = 140,
  kKeycode141 = 141,
  kKeycode142 = 142,
  kKeycode143 = 143,
  kNumlock = 144,
  kScroll = 145,
  kKeycode146 = 146,
  kKeycode147 = 147,
  kKeycode148 = 148,
  kKeycode149 = 149,
  kKeycode150 = 150,
  kWlan = 151,
  kPower = 152,
  kAssistant = 153,
  kKeycode154 = 154,
  kKeycode155 = 155,
  kKeycode156 = 156,
  kKeycode157 = 157,
  kKeycode158 = 158,
  kKeycode159 = 159,
  kLshift = 160,
  kRshift = 161,
  kLcontrol = 162,
  kRcontrol = 163,
  kLmenu = 164,
  kRmenu = 165,
  kBrowserBack = 166,
  kBrowserForward = 167,
  kBrowserRefresh = 168,
  kBrowserStop = 169,
  kBrowserSearch = 170,
  kBrowserFavorites = 171,
  kBrowserHome = 172,
  kVolumeMute = 173,
  kVolumeDown = 174,
  kVolumeUp = 175,
  kMediaNextTrack = 176,
  kMediaPrevTrack = 177,
  kMediaStop = 178,
  kMediaPlayPause = 179,
  kMediaLaunchMail = 180,
  kMediaLaunchMediaSelect = 181,
  kMediaLaunchApp1 = 182,
  kMediaLaunchApp2 = 183,
  kKeycode184 = 184,
  kKeycode185 = 185,
  kOem1 = 186,
  kOemPlus = 187,
  kOemComma = 188,
  kOemMinus = 189,
  kOemPeriod = 190,
  kOem2 = 191,
  kOem3 = 192,
  kKeycode193 = 193,
  kKeycode194 = 194,
  kKeycode195 = 195,
  kKeycode196 = 196,
  kKeycode197 = 197,
  kKeycode198 = 198,
  kKeycode199 = 199,
  kKeycode200 = 200,
  kKeycode201 = 201,
  kKeycode202 = 202,
  kKeycode203 = 203,
  kKeycode204 = 204,
  kKeycode205 = 205,
  kKeycode206 = 206,
  kKeycode207 = 207,
  kKeycode208 = 208,
  kKeycode209 = 209,
  kKeycode210 = 210,
  kKeycode211 = 211,
  kKeycode212 = 212,
  kKeycode213 = 213,
  kKeycode214 = 214,
  kKeycode215 = 215,
  kBrightnessDown = 216,
  kBrightnessUp = 217,
  kKbdBrightnessDown = 218,
  kOem4 = 219,
  kOem5 = 220,
  kOem6 = 221,
  kOem7 = 222,
  kOem8 = 223,
  kKeycode224 = 224,
  kAltgr = 225,
  kOem102 = 226,
  kKeycode227 = 227,
  kKeycode228 = 228,
  kProcesskey = 229,
  kCompose = 230,
  kPacket = 231,
  kKbdBrightnessUp = 232,
  kKeycode233 = 233,
  kKeycode234 = 234,
  kKeycode235 = 235,
  kKeycode236 = 236,
  kKeycode237 = 237,
  kKeycode238 = 238,
  kKeycode239 = 239,
  kKeycode240 = 240,
  kKeycode241 = 241,
  kKeycode242 = 242,
  kDbeSbcschar = 243,
  kDbeDbcschar = 244,
  kKeycode245 = 245,
  kAttn = 246,
  kCrsel = 247,
  kExsel = 248,
  kEreof = 249,
  kPlay = 250,
  kZoom = 251,
  kNoname = 252,
  kPa1 = 253,
  kOemClear = 254,
  kKeycode255 = 255,
  kNone = 256,
  kMaxValue = kNone,
};

}  // namespace

AccessibilityController::Feature::Feature(
    FeatureType type,
    const std::string& pref_name,
    const gfx::VectorIcon* icon,
    const int name_resource_id,
    const bool toggleable_in_quicksettings,
    AccessibilityController* controller)
    : type_(type),
      pref_name_(pref_name),
      icon_(icon),
      name_resource_id_(name_resource_id),
      toggleable_in_quicksettings_(toggleable_in_quicksettings),
      owner_(controller) {
  // If a feature is toggleable in quicksettings it must have a
  // `name_resource_id` so it's name can be looked up.
  if (toggleable_in_quicksettings_) {
    CHECK(name_resource_id);
  }
}

AccessibilityController::Feature::~Feature() = default;

void AccessibilityController::Feature::SetEnabled(bool enabled) {
  PrefService* prefs = owner_->active_user_prefs_;
  if (!prefs) {
    return;
  }
  prefs->SetBoolean(pref_name_, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::Feature::IsVisibleInTray() const {
  return (conflicting_feature_ == FeatureType::kNoConflictingFeature ||
          !owner_->GetFeature(conflicting_feature_).enabled()) &&
         owner_->IsAccessibilityFeatureVisibleInTrayMenu(pref_name_);
}

bool AccessibilityController::Feature::IsEnterpriseIconVisible() const {
  return owner_->IsEnterpriseIconVisibleInTrayMenu(pref_name_);
}

const gfx::VectorIcon& AccessibilityController::Feature::icon() const {
  DCHECK(icon_);
  if (icon_) {
    return *icon_;
  }
  return kPaletteTrayIconDefaultIcon;
}

void AccessibilityController::Feature::UpdateFromPref() {
  PrefService* prefs = owner_->active_user_prefs_;
  DCHECK(prefs);

  bool enabled = prefs->GetBoolean(pref_name_);

  if (conflicting_feature_ != FeatureType::kNoConflictingFeature &&
      owner_->GetFeature(conflicting_feature_).enabled()) {
    enabled = false;
  }

  if (enabled) {
    // If it was turned on and we are in a active logged in session,
    // prepare to record duration metrics.
    session_manager::SessionState session_state =
        Shell::Get()->session_controller()->GetSessionState();
    if (session_state == session_manager::SessionState::ACTIVE) {
      enabled_time_ = base::Time::Now();
    }
  } else {
    // Disabled. Log the duration since it was enabled, if needed.
    LogDurationMetric();
  }

  if (enabled == enabled_) {
    return;
  }

  enabled_ = enabled;
  owner_->UpdateFeatureFromPref(type_);
}

// don't pass prefservice here because it might be old.
// instead save the session state type from when enabled_time_ was set.
// maybe don't bother logging user type. just have this be for logged in??
// is session state more interesting?
// duration if session state is ACTIVE
void AccessibilityController::Feature::LogDurationMetric() {
  if (enabled_time_ == base::Time()) {
    return;
  }

  std::string feature_duration_metric = "Accessibility.";
  switch (type_) {
    case FeatureType::kAutoclick:
      feature_duration_metric += "CrosAutoclick";
      break;
    case FeatureType::kCaretHighlight:
      feature_duration_metric += "CrosCaretHighlight";
      break;
    case FeatureType::kColorCorrection:
      feature_duration_metric += "CrosColorCorrection";
      break;
    case FeatureType::kCursorColor:
      feature_duration_metric += "CrosCursorColor";
      break;
    case FeatureType::kCursorHighlight:
      feature_duration_metric += "CrosCursorHighlight";
      break;
    case FeatureType::kDictation:
      feature_duration_metric += "CrosDictation";
      break;
    case FeatureType::kDisableTrackpad:
      feature_duration_metric += "CrosDisableTrackpad";
      break;
    case FeatureType::kDockedMagnifier:
      feature_duration_metric += "CrosDockedMagnifier";
      break;
    case FeatureType::kFaceGaze:
      feature_duration_metric += "CrosFaceGaze";
      break;
    case FeatureType::kFlashNotifications:
      feature_duration_metric += "CrosFlashNotifications";
      break;
    case FeatureType::kFocusHighlight:
      feature_duration_metric += "CrosFocusHighlight";
      break;
    case FeatureType::kFullscreenMagnifier:
      feature_duration_metric += "CrosScreenMagnifier";
      break;
    case FeatureType::kHighContrast:
      feature_duration_metric += "CrosHighContrast";
      break;
    case FeatureType::kLargeCursor:
      feature_duration_metric += "CrosLargeCursor";
      break;
    case FeatureType::kLiveCaption:
      feature_duration_metric += "CrosLiveCaption";
      break;
    case FeatureType::kMonoAudio:
      feature_duration_metric += "CrosMonoAudio";
      break;
    case FeatureType::kMouseKeys:
      feature_duration_metric += "CrosMouseKeys";
      break;
    case FeatureType::kReducedAnimations:
      feature_duration_metric += "CrosReducedAnimations";
      break;
    case FeatureType::kSelectToSpeak:
      feature_duration_metric += "CrosSelectToSpeak";
      break;
    case FeatureType::kSpokenFeedback:
      feature_duration_metric += "CrosSpokenFeedback";
      break;
    case FeatureType::kStickyKeys:
      feature_duration_metric += "CrosStickyKeys";
      break;
    case FeatureType::kSwitchAccess:
      feature_duration_metric += "CrosSwitchAccess";
      break;
    case FeatureType::kVirtualKeyboard:
      feature_duration_metric += "CrosVirtualKeyboard";
      break;
    default:
      return;
  }

  feature_duration_metric += ".SessionDuration";

  base::TimeDelta duration = base::Time::Now() - enabled_time_;
  base::UmaHistogramCustomCounts(feature_duration_metric, duration.InSeconds(),
                                 1, base::Days(1) / base::Seconds(1), 100);

  // Reset enabled time as this duration is now logged and accounted for.
  enabled_time_ = base::Time();
}

void AccessibilityController::Feature::SetConflictingFeature(
    FeatureType feature) {
  DCHECK_EQ(conflicting_feature_, FeatureType::kNoConflictingFeature);
  conflicting_feature_ = feature;
}

void AccessibilityController::Feature::ObserveConflictingFeature() {
  std::string conflicting_pref_name = "";
  switch (conflicting_feature_) {
    case A11yFeatureType::kSpokenFeedback:
      conflicting_pref_name = prefs::kAccessibilitySpokenFeedbackEnabled;
      break;
    default:
      // No other features are used as conflicting features at the moment,
      // but this could be populated if needed in the future.
      NOTREACHED() << "No pref name for conflicting feature "
                   << static_cast<int>(conflicting_feature_);
  }
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(owner_->active_user_prefs_);
  pref_change_registrar_->Add(
      conflicting_pref_name,
      base::BindRepeating(&AccessibilityController::Feature::UpdateFromPref,
                          base::Unretained(this)));
}

AccessibilityController::FeatureWithDialog::FeatureWithDialog(
    FeatureType type,
    const std::string& pref_name,
    const gfx::VectorIcon* icon,
    const int name_resource_id,
    const bool toggleable_in_quicksettings,
    const std::string& dialog_pref,
    AccessibilityController* controller)
    : AccessibilityController::Feature(type,
                                       pref_name,
                                       icon,
                                       name_resource_id,
                                       toggleable_in_quicksettings,
                                       controller),
      dialog_pref_(dialog_pref) {}
AccessibilityController::FeatureWithDialog::~FeatureWithDialog() = default;

void AccessibilityController::FeatureWithDialog::SetDialogAccepted() {
  PrefService* prefs = owner_->active_user_prefs_;
  if (!prefs) {
    return;
  }
  prefs->SetBoolean(dialog_pref_, true);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::FeatureWithDialog::WasDialogAccepted() const {
  PrefService* prefs = owner_->active_user_prefs_;
  DCHECK(prefs);
  return prefs->GetBoolean(dialog_pref_);
}

// static
AccessibilityController* AccessibilityController::Get() {
  return g_instance;
}

AccessibilityController::AccessibilityController()
    : autoclick_delay_(AutoclickController::GetDefaultAutoclickDelay()) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;

  Shell::Get()->session_controller()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  CreateAccessibilityFeatures();

  accessibility_notification_controller_ =
      std::make_unique<AccessibilityNotificationController>();

  flash_screen_controller_ = std::make_unique<FlashScreenController>();
}

AccessibilityController::~AccessibilityController() {
  floating_menu_controller_.reset();
  accessibility_notification_controller_.reset();

  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void AccessibilityController::CreateAccessibilityFeatures() {
  // First, build all features with dialog.
  std::map<FeatureType, std::string> dialogs;
  for (auto dialog_data : kFeatureDialogs) {
    dialogs[dialog_data.type] = dialog_data.pref;
  }
  for (auto feature_data : kFeatures) {
    size_t feature_index = static_cast<size_t>(feature_data.type);
    DCHECK(!features_[feature_index]);
    auto it = dialogs.find(feature_data.type);
    if (it == dialogs.end()) {
      features_[feature_index] = std::make_unique<Feature>(
          feature_data.type, feature_data.pref, feature_data.icon,
          feature_data.name_resource_id,
          feature_data.toggleable_in_quicksettings, this);
    } else {
      features_[feature_index] = std::make_unique<FeatureWithDialog>(
          feature_data.type, feature_data.pref, feature_data.icon,
          feature_data.name_resource_id,
          feature_data.toggleable_in_quicksettings, it->second, this);
    }
    if (feature_data.conflicting_feature !=
        FeatureType::kNoConflictingFeature) {
      features_[feature_index]->SetConflictingFeature(
          feature_data.conflicting_feature);
    }
  }
}

// static
void AccessibilityController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  //
  // Non-syncable prefs.
  //
  // These prefs control whether an accessibility feature is enabled. They are
  // not synced due to the impact they have on device interaction.
  registry->RegisterBooleanPref(prefs::kAccessibilityAutoclickEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityCursorColorEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityCaretHighlightEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityCursorHighlightEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityDictationEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityFloatingMenuEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityFocusHighlightEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityHighContrastEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityLargeCursorEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityMonoAudioEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityMouseKeysEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityScreenMagnifierEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilitySpokenFeedbackEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilitySelectToSpeakEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityStickyKeysEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityShortcutsEnabled, true);
  registry->RegisterBooleanPref(prefs::kAccessibilitySwitchAccessEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAccessibilityVirtualKeyboardEnabled,
                                false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityFaceGazeEnabled, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityDisableTrackpadEnabled,
                                false);
  registry->RegisterIntegerPref(prefs::kAccessibilityDisableTrackpadMode,
                                static_cast<int>(DisableTrackpadMode::kNever));

  // Not syncable because it might change depending on application locale,
  // user settings, and because different languages can cause speech recognition
  // files to download.
  registry->RegisterStringPref(prefs::kAccessibilityDictationLocale,
                               std::string());
  registry->RegisterDictionaryPref(
      prefs::kAccessibilityDictationLocaleOfflineNudge);

  // A pref in this list is associated with accepting for the first time,
  // enabling of some pref above. Non-syncable like all of the above prefs.
  registry->RegisterBooleanPref(
      prefs::kHighContrastAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kSelectToSpeakAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kDictationDlcSuccessNotificationHasBeenShown, false);
  registry->RegisterBooleanPref(
      prefs::kDictationDlcOnlyPumpkinDownloadedNotificationHasBeenShown, false);
  registry->RegisterBooleanPref(
      prefs::kDictationDlcOnlySodaDownloadedNotificationHasBeenShown, false);
  registry->RegisterBooleanPref(
      prefs::kDictationNoDlcsDownloadedNotificationHasBeenShown, false);
  registry->RegisterBooleanPref(
      prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2, false);
  registry->RegisterBooleanPref(prefs::kShouldAlwaysShowAccessibilityMenu,
                                false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kFaceGazeDlcSuccessNotificationHasBeenShown, false);
  registry->RegisterBooleanPref(
      prefs::kFaceGazeDlcFailureNotificationHasBeenShown, false);

  registry->RegisterBooleanPref(prefs::kAccessibilityColorCorrectionEnabled,
                                false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityColorCorrectionHasBeenSetup, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityFlashNotificationsEnabled,
                                false);

  registry->RegisterBooleanPref(prefs::kAccessibilityReducedAnimationsEnabled,
                                false);

  // TODO(b/266816160): Make ChromeVox prefs are syncable, to so that ChromeOS
  // backs up users' ChromeVox settings and reflects across their devices.
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxAutoRead, false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxAnnounceDownloadNotifications, true);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxAnnounceRichTextAttributes, true);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxAudioStrategy,
                               kDefaultAccessibilityChromeVoxAudioStrategy);
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxBrailleSideBySide,
                                true);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxBrailleTable,
                               kDefaultAccessibilityChromeVoxBrailleTable);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxBrailleTable6,
                               kDefaultAccessibilityChromeVoxBrailleTable6);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxBrailleTable8,
                               kDefaultAccessibilityChromeVoxBrailleTable8);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxBrailleTableType,
                               kDefaultAccessibilityChromeVoxBrailleTableType);
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxBrailleWordWrap,
                                true);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxCapitalStrategy,
                               kDefaultAccessibilityChromeVoxCapitalStrategy);
  registry->RegisterStringPref(
      prefs::kAccessibilityChromeVoxCapitalStrategyBackup,
      kDefaultAccessibilityChromeVoxCapitalStrategyBackup);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxEnableBrailleLogging, false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxEnableEarconLogging, false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxEnableEventStreamLogging, false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxEnableSpeechLogging, false);
  registry->RegisterDictionaryPref(
      prefs::kAccessibilityChromeVoxEventStreamFilters, base::Value::Dict());
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxLanguageSwitching,
                                false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxMenuBrailleCommands, false);
  registry->RegisterStringPref(
      prefs::kAccessibilityChromeVoxNumberReadingStyle,
      kDefaultAccessibilityChromeVoxNumberReadingStyle);
  registry->RegisterStringPref(
      prefs::kAccessibilityChromeVoxPreferredBrailleDisplayAddress,
      kDefaultAccessibilityChromeVoxPreferredBrailleDisplayAddress);
  registry->RegisterIntegerPref(prefs::kAccessibilityChromeVoxPunctuationEcho,
                                kDefaultAccessibilityChromeVoxPunctuationEcho);
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxSmartStickyMode,
                                true);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityChromeVoxSpeakTextUnderMouse, false);
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxUsePitchChanges,
                                true);
  registry->RegisterBooleanPref(prefs::kAccessibilityChromeVoxUseVerboseMode,
                                true);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityChromeVoxVirtualBrailleColumns,
      kDefaultAccessibilityChromeVoxVirtualBrailleColumns);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityChromeVoxVirtualBrailleRows,
      kDefaultAccessibilityChromeVoxVirtualBrailleRows);
  registry->RegisterStringPref(prefs::kAccessibilityChromeVoxVoiceName,
                               kDefaultAccessibilityChromeVoxVoiceName);

  // TODO(b/259372916): Enable sync for Mouse Keys settings before launch.
  registry->RegisterDoublePref(prefs::kAccessibilityMouseKeysAcceleration,
                               MouseKeysController::kDefaultAcceleration);
  registry->RegisterDoublePref(prefs::kAccessibilityMouseKeysMaxSpeed,
                               MouseKeysController::kDefaultMaxSpeed);
  registry->RegisterBooleanPref(prefs::kAccessibilityMouseKeysUsePrimaryKeys,
                                true);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityMouseKeysDominantHand,
      static_cast<int>(MouseKeysDominantHand::kRightHandDominant));

  //
  // Syncable prefs.
  //
  // These prefs pertain to specific features. They are synced to preserve
  // behaviors tied to user accounts once that user enables a feature.
  registry->RegisterIntegerPref(
      prefs::kAccessibilityAutoclickDelayMs, kDefaultAutoclickDelayMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityAutoclickEventType,
      static_cast<int>(kDefaultAutoclickEventType),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAutoclickRevertToLeftClick, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAutoclickStabilizePosition, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityAutoclickMovementThreshold,
      kDefaultAutoclickMovementThreshold,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityAutoclickMenuPosition,
      static_cast<int>(kDefaultAutoclickMenuPosition),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterIntegerPref(
      prefs::kAccessibilityCursorColor, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterIntegerPref(
      prefs::kAccessibilityFloatingMenuPosition,
      static_cast<int>(kDefaultFloatingMenuPosition),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  registry->RegisterIntegerPref(prefs::kAccessibilityLargeCursorDipSize,
                                kDefaultLargeCursorSize);

  registry->RegisterIntegerPref(
      prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
      static_cast<int>(MagnifierMouseFollowingMode::kEdge),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDoublePref(prefs::kAccessibilityScreenMagnifierScale,
                               std::numeric_limits<double>::min());
  registry->RegisterDictionaryPref(
      prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
      kDefaultSwitchAccessAutoScanSpeed.InMilliseconds(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
      kDefaultSwitchAccessAutoScanSpeed.InMilliseconds(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond,
      kDefaultSwitchAccessPointScanSpeedDipsPerSecond,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed,
      kDefaultAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakBackgroundShading,
      kDefaultAccessibilitySelectToSpeakBackgroundShading,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakEnhancedNetworkVoices,
      kDefaultAccessibilitySelectToSpeakEnhancedNetworkVoices,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown,
      kDefaultAccessibilitySelectToSpeakEnhancedVoicesDialogShown,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakNavigationControls,
      kDefaultAccessibilitySelectToSpeakNavigationControls,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakVoiceSwitching,
      kDefaultAccessibilitySelectToSpeakVoiceSwitching,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakWordHighlight,
      kDefaultAccessibilitySelectToSpeakWordHighlight,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(
      prefs::kAccessibilitySelectToSpeakEnhancedVoiceName,
      kDefaultAccessibilitySelectToSpeakEnhancedVoiceName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(
      prefs::kAccessibilitySelectToSpeakHighlightColor,
      kDefaultAccessibilitySelectToSpeakHighlightColor,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(
      prefs::kAccessibilitySelectToSpeakVoiceName,
      kDefaultAccessibilitySelectToSpeakVoiceName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityColorVisionCorrectionAmount, 100,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityColorVisionCorrectionType,
      ColorVisionCorrectionType::kDeuteranomaly,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  if (::features::IsAccessibilityFaceGazeEnabled()) {
    registry->RegisterIntegerPref(
        prefs::kAccessibilityFaceGazeCursorSpeedUp, kDefaultFaceGazeCursorSpeed,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityFaceGazeCursorSpeedDown,
        kDefaultFaceGazeCursorSpeed,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityFaceGazeCursorSpeedLeft,
        kDefaultFaceGazeCursorSpeed,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityFaceGazeCursorSpeedRight,
        kDefaultFaceGazeCursorSpeed,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityFaceGazeCursorSmoothing,
        kDefaultFaceGazeCursorSmoothing,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterBooleanPref(
        prefs::kAccessibilityFaceGazeCursorUseAcceleration,
        kDefaultFaceGazeCursorUseAcceleration,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterDictionaryPref(
        prefs::kAccessibilityFaceGazeGesturesToKeyCombos,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterDictionaryPref(
        prefs::kAccessibilityFaceGazeGesturesToMacros,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterDictionaryPref(
        prefs::kAccessibilityFaceGazeGesturesToConfidence,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterBooleanPref(
        prefs::kAccessibilityFaceGazeCursorControlEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterBooleanPref(
        prefs::kAccessibilityFaceGazeActionsEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterBooleanPref(
        prefs::kAccessibilityFaceGazeAdjustSpeedSeparately, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityFaceGazeVelocityThreshold,
        kDefaultFaceGazeVelocityThreshold,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  }

  if (::features::IsAccessibilityMagnifierFollowsChromeVoxEnabled()) {
    registry->RegisterBooleanPref(
        prefs::kAccessibilityMagnifierFollowsChromeVox, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  }

  if (::features::IsAccessibilityMagnifierFollowsStsEnabled()) {
    registry->RegisterBooleanPref(
        prefs::kAccessibilityMagnifierFollowsSts, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  }

  if (::features::IsAccessibilityCaretBlinkIntervalSettingEnabled()) {
    registry->RegisterIntegerPref(prefs::kAccessibilityCaretBlinkInterval,
                                  kDefaultCaretBlinkIntervalMs);
  }

  if (::features::IsAccessibilityFlashScreenFeatureEnabled()) {
    registry->RegisterIntegerPref(prefs::kAccessibilityFlashNotificationsColor,
                                  kDefaultFlashNotificationsColor);
  }
}

void AccessibilityController::Shutdown() {
  // Log metrics at shutdown.
  for (auto& feature : features_) {
    feature->LogDurationMetric();
  }

  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);

  // Clean up any child windows and widgets that might be animating out.
  dictation_bubble_controller_.reset();
  facegaze_bubble_controller_.reset();

  for (auto& observer : observers_) {
    observer.OnAccessibilityControllerShutdown();
  }
}

bool AccessibilityController::HasDisplayRotationAcceleratorDialogBeenAccepted()
    const {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(
             prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2);
}

void AccessibilityController::
    SetDisplayRotationAcceleratorDialogBeenAccepted() {
  if (!active_user_prefs_) {
    return;
  }
  active_user_prefs_->SetBoolean(
      prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2, true);
  active_user_prefs_->CommitPendingWrite();
}

void AccessibilityController::AddObserver(AccessibilityObserver* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityController::RemoveObserver(AccessibilityObserver* observer) {
  observers_.RemoveObserver(observer);
}

AccessibilityController::Feature& AccessibilityController::GetFeature(
    FeatureType type) const {
  size_t feature_index = static_cast<size_t>(type);
  DCHECK(features_[feature_index].get());
  return *features_[feature_index].get();
}

std::vector<AccessibilityController::Feature*>
AccessibilityController::GetEnabledFeaturesInQuickSettings() const {
  std::vector<Feature*> enabled_features;

  for (auto& feature : features_) {
    if (feature->enabled() && feature->toggleable_in_quicksettings()) {
      enabled_features.push_back(feature.get());
    }
  }
  return enabled_features;
}

base::WeakPtr<AccessibilityController> AccessibilityController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AccessibilityController::Feature& AccessibilityController::autoclick() const {
  return GetFeature(FeatureType::kAutoclick);
}

AccessibilityController::Feature& AccessibilityController::caret_highlight()
    const {
  return GetFeature(FeatureType::kCaretHighlight);
}

AccessibilityController::Feature& AccessibilityController::cursor_highlight()
    const {
  return GetFeature(FeatureType::kCursorHighlight);
}

AccessibilityController::Feature& AccessibilityController::cursor_color()
    const {
  return GetFeature(FeatureType::kCursorColor);
}

AccessibilityController::Feature& AccessibilityController::dictation() const {
  return GetFeature(FeatureType::kDictation);
}

AccessibilityController::Feature& AccessibilityController::disable_trackpad()
    const {
  return GetFeature(FeatureType::kDisableTrackpad);
}

AccessibilityController::Feature& AccessibilityController::color_correction()
    const {
  return GetFeature(FeatureType::kColorCorrection);
}

AccessibilityController::Feature& AccessibilityController::face_gaze() const {
  return GetFeature(FeatureType::kFaceGaze);
}

AccessibilityController::Feature& AccessibilityController::flash_notifications()
    const {
  return GetFeature(FeatureType::kFlashNotifications);
}

AccessibilityController::Feature& AccessibilityController::focus_highlight()
    const {
  return GetFeature(FeatureType::kFocusHighlight);
}

AccessibilityController::Feature& AccessibilityController::floating_menu()
    const {
  return GetFeature(FeatureType::kFloatingMenu);
}

AccessibilityController::FeatureWithDialog&
AccessibilityController::fullscreen_magnifier() const {
  return static_cast<FeatureWithDialog&>(
      GetFeature(FeatureType::kFullscreenMagnifier));
}

AccessibilityController::FeatureWithDialog&
AccessibilityController::docked_magnifier() const {
  return static_cast<FeatureWithDialog&>(
      GetFeature(FeatureType::kDockedMagnifier));
}

AccessibilityController::FeatureWithDialog&
AccessibilityController::high_contrast() const {
  return static_cast<FeatureWithDialog&>(
      GetFeature(FeatureType::kHighContrast));
}

AccessibilityController::Feature& AccessibilityController::large_cursor()
    const {
  return GetFeature(FeatureType::kLargeCursor);
}

AccessibilityController::Feature& AccessibilityController::live_caption()
    const {
  return GetFeature(FeatureType::kLiveCaption);
}

AccessibilityController::Feature& AccessibilityController::mono_audio() const {
  return GetFeature(FeatureType::kMonoAudio);
}

AccessibilityController::Feature& AccessibilityController::mouse_keys() const {
  return GetFeature(FeatureType::kMouseKeys);
}

AccessibilityController::Feature& AccessibilityController::reduced_animations()
    const {
  return GetFeature(FeatureType::kReducedAnimations);
}

AccessibilityController::Feature& AccessibilityController::spoken_feedback()
    const {
  return GetFeature(FeatureType::kSpokenFeedback);
}

AccessibilityController::Feature& AccessibilityController::select_to_speak()
    const {
  return GetFeature(FeatureType::kSelectToSpeak);
}

AccessibilityController::Feature& AccessibilityController::sticky_keys() const {
  return GetFeature(FeatureType::kStickyKeys);
}

AccessibilityController::Feature& AccessibilityController::switch_access()
    const {
  return GetFeature(FeatureType::kSwitchAccess);
}

AccessibilityController::Feature& AccessibilityController::virtual_keyboard()
    const {
  return GetFeature(FeatureType::kVirtualKeyboard);
}

bool AccessibilityController::IsAutoclickSettingVisibleInTray() {
  return autoclick().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForAutoclick() {
  return autoclick().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsCaretHighlightSettingVisibleInTray() {
  return caret_highlight().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForCaretHighlight() {
  return caret_highlight().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsCursorHighlightSettingVisibleInTray() {
  return cursor_highlight().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForCursorHighlight() {
  return cursor_highlight().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsDictationSettingVisibleInTray() {
  return dictation().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForDictation() {
  return dictation().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsFaceGazeSettingVisibleInTray() {
  return face_gaze().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForFaceGaze() {
  return face_gaze().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsFocusHighlightSettingVisibleInTray() {
  return focus_highlight().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForFocusHighlight() {
  return focus_highlight().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsFullScreenMagnifierSettingVisibleInTray() {
  return fullscreen_magnifier().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForFullScreenMagnifier() {
  return fullscreen_magnifier().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsDockedMagnifierSettingVisibleInTray() {
  return docked_magnifier().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForDockedMagnifier() {
  return docked_magnifier().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsHighContrastSettingVisibleInTray() {
  return high_contrast().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForHighContrast() {
  return high_contrast().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsColorCorrectionSettingVisibleInTray() {
  if (!color_correction().enabled() &&
      Shell::Get()->session_controller()->login_status() ==
          ash::LoginStatus::NOT_LOGGED_IN) {
    // Don't allow users to enable this on not logged in profiles because it
    // requires set-up in settings the first time it is run.
    return false;
  }
  return color_correction().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForColorCorrection() {
  return color_correction().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsLargeCursorSettingVisibleInTray() {
  return large_cursor().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForLargeCursor() {
  return large_cursor().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsLiveCaptionSettingVisibleInTray() {
  return captions::IsLiveCaptionFeatureSupported() &&
         live_caption().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForLiveCaption() {
  return captions::IsLiveCaptionFeatureSupported() &&
         live_caption().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsMonoAudioSettingVisibleInTray() {
  return mono_audio().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForMonoAudio() {
  return mono_audio().IsEnterpriseIconVisible();
}

void AccessibilityController::SetSpokenFeedbackEnabled(
    bool enabled,
    AccessibilityNotificationVisibility notify) {
  spoken_feedback().SetEnabled(enabled);

  // Value could be left unchanged because of higher-priority pref source, eg.
  // policy. See crbug.com/953245.
  const bool actual_enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  A11yNotificationType type = A11yNotificationType::kNone;
  if (enabled && actual_enabled && notify == A11Y_NOTIFICATION_SHOW) {
    type = A11yNotificationType::kSpokenFeedbackEnabled;
  }
  ShowAccessibilityNotification(
      A11yNotificationWrapper(type, std::vector<std::u16string>()));
}

bool AccessibilityController::IsSpokenFeedbackSettingVisibleInTray() {
  return spoken_feedback().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForSpokenFeedback() {
  return spoken_feedback().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsSelectToSpeakSettingVisibleInTray() {
  return select_to_speak().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForSelectToSpeak() {
  return select_to_speak().IsEnterpriseIconVisible();
}

void AccessibilityController::RequestSelectToSpeakStateChange() {
  client_->RequestSelectToSpeakStateChange();
}

void AccessibilityController::OnTrackpadNotificationClicked(
    std::optional<int> button_index) {
  if (!button_index) {
    return;
  }

  EnableInternalTrackpad();
}

void AccessibilityController::RecordSelectToSpeakSpeechDuration(
    SelectToSpeakState old_state,
    SelectToSpeakState new_state) {
  if (new_state != SelectToSpeakState::kSelectToSpeakStateSpeaking &&
      select_to_speak_speech_start_time_ == base::Time()) {
    select_to_speak_speech_start_time_ = base::Time::Now();
  }
  if (old_state != SelectToSpeakState::kSelectToSpeakStateSpeaking &&
      new_state != old_state &&
      select_to_speak_speech_start_time_ != base::Time()) {
    base::TimeDelta duration =
        base::Time::Now() - select_to_speak_speech_start_time_;
    base::UmaHistogramCustomCounts(
        "Accessibility.CrosSelectToSpeak.SpeechDuration", duration.InSeconds(),
        /*min=*/1, /*max=*/base::Minutes(20) / base::Seconds(1),
        /*buckets=*/100);
    select_to_speak_speech_start_time_ = base::Time();
  }
}

void AccessibilityController::SetSelectToSpeakState(SelectToSpeakState state) {
  RecordSelectToSpeakSpeechDuration(select_to_speak_state_, state);
  select_to_speak_state_ = state;

  // Forward the state change event to select_to_speak_event_handler_.
  // The extension may have requested that the handler enter SELECTING state.
  // Prepare to start capturing events from stylus, mouse or touch.
  if (select_to_speak_event_handler_) {
    select_to_speak_event_handler_->SetSelectToSpeakStateSelecting(
        state == SelectToSpeakState::kSelectToSpeakStateSelecting);
  }
  NotifyAccessibilityStatusChanged();
}

void AccessibilityController::SetSelectToSpeakEventHandlerDelegate(
    SelectToSpeakEventHandlerDelegate* delegate) {
  select_to_speak_event_handler_delegate_ = delegate;
  MaybeCreateSelectToSpeakEventHandler();
}

SelectToSpeakState AccessibilityController::GetSelectToSpeakState() const {
  return select_to_speak_state_;
}

void AccessibilityController::ShowSelectToSpeakPanel(const gfx::Rect& anchor,
                                                     bool is_paused,
                                                     double speech_rate) {
  if (!select_to_speak_bubble_controller_) {
    select_to_speak_bubble_controller_ =
        std::make_unique<SelectToSpeakMenuBubbleController>();
  }
  select_to_speak_bubble_controller_->Show(anchor, is_paused, speech_rate);
}

void AccessibilityController::HideSelectToSpeakPanel() {
  if (!select_to_speak_bubble_controller_) {
    return;
  }
  select_to_speak_bubble_controller_->Hide();
}

void AccessibilityController::OnSelectToSpeakPanelAction(
    SelectToSpeakPanelAction action,
    double value) {
  if (!client_) {
    return;
  }
  client_->OnSelectToSpeakPanelAction(action, value);
}

bool AccessibilityController::IsSwitchAccessRunning() const {
  return switch_access().enabled() || switch_access_disable_dialog_showing_;
}

bool AccessibilityController::IsSwitchAccessSettingVisibleInTray() {
  // Switch Access cannot be enabled on the sign-in page because there is no way
  // to configure switches while the device is locked.
  if (!switch_access().enabled() &&
      Shell::Get()->session_controller()->login_status() ==
          ash::LoginStatus::NOT_LOGGED_IN) {
    return false;
  }
  return switch_access().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForSwitchAccess() {
  return switch_access().IsEnterpriseIconVisible();
}

void AccessibilityController::SetAccessibilityEventRewriter(
    AccessibilityEventRewriter* accessibility_event_rewriter) {
  accessibility_event_rewriter_ = accessibility_event_rewriter;
}

void AccessibilityController::SetDisableTrackpadEventRewriter(
    DisableTrackpadEventRewriter* rewriter) {
  disable_trackpad_event_rewriter_ = rewriter;
}

void AccessibilityController::EnableInternalTrackpad() {
  active_user_prefs_->SetInteger(prefs::kAccessibilityDisableTrackpadMode,
                                 static_cast<int>(DisableTrackpadMode::kNever));
}

void AccessibilityController::SetFilterKeysEventRewriter(
    FilterKeysEventRewriter* rewriter) {
  filter_keys_event_rewriter_ = rewriter;
}

void AccessibilityController::HideSwitchAccessBackButton() {
  if (IsSwitchAccessRunning()) {
    switch_access_bubble_controller_->HideBackButton();
  }
}

void AccessibilityController::HideSwitchAccessMenu() {
  if (IsSwitchAccessRunning()) {
    switch_access_bubble_controller_->HideMenuBubble();
  }
}

void AccessibilityController::ShowSwitchAccessBackButton(
    const gfx::Rect& anchor) {
  switch_access_bubble_controller_->ShowBackButton(anchor);
}

void AccessibilityController::ShowSwitchAccessMenu(
    const gfx::Rect& anchor,
    std::vector<std::string> actions_to_show) {
  switch_access_bubble_controller_->ShowMenu(anchor, actions_to_show);
}

bool AccessibilityController::IsPointScanEnabled() {
  return point_scan_controller_.get() &&
         point_scan_controller_->IsPointScanEnabled();
}

void AccessibilityController::StartPointScan() {
  point_scan_controller_->Start();
}

void AccessibilityController::SetA11yOverrideWindow(
    aura::Window* a11y_override_window) {
  if (client_) {
    client_->SetA11yOverrideWindow(a11y_override_window);
  }
}

void AccessibilityController::StopPointScan() {
  if (point_scan_controller_) {
    point_scan_controller_->HideAll();
  }
}

void AccessibilityController::SetPointScanSpeedDipsPerSecond(
    int point_scan_speed_dips_per_second) {
  if (point_scan_controller_) {
    point_scan_controller_->SetSpeedDipsPerSecond(
        point_scan_speed_dips_per_second);
  }
}

void AccessibilityController::DisablePolicyRecommendationRestorerForTesting() {
  Shell::Get()->policy_recommendation_restorer()->DisableForTesting();
}

bool AccessibilityController::IsStickyKeysSettingVisibleInTray() {
  return sticky_keys().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForStickyKeys() {
  return sticky_keys().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsReducedAnimationsSettingVisibleInTray() {
  if (!::features::IsAccessibilityReducedAnimationsInKioskEnabled()) {
    return false;
  }

  // Only visible in kiosk mode.
  if (!Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return false;
  }
  return reduced_animations().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForReducedAnimations() {
  return reduced_animations().IsEnterpriseIconVisible();
}

bool AccessibilityController::IsVirtualKeyboardSettingVisibleInTray() {
  return virtual_keyboard().IsVisibleInTray();
}

bool AccessibilityController::IsEnterpriseIconVisibleForVirtualKeyboard() {
  return virtual_keyboard().IsEnterpriseIconVisible();
}

void AccessibilityController::ShowFloatingMenuIfEnabled() {
  if (floating_menu().enabled() && !floating_menu_controller_) {
    floating_menu_controller_ =
        std::make_unique<FloatingAccessibilityController>(this);
    floating_menu_controller_->Show(GetFloatingMenuPosition());
  } else {
    always_show_floating_menu_when_enabled_ = true;
  }
}

FloatingAccessibilityController*
AccessibilityController::GetFloatingMenuController() {
  return floating_menu_controller_.get();
}

PointScanController* AccessibilityController::GetPointScanController() {
  return point_scan_controller_.get();
}

void AccessibilityController::SetTabletModeShelfNavigationButtonsEnabled(
    bool enabled) {
  if (!active_user_prefs_) {
    return;
  }

  active_user_prefs_->SetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, enabled);
  active_user_prefs_->CommitPendingWrite();
}

void AccessibilityController::TriggerAccessibilityAlert(
    AccessibilityAlert alert) {
  if (client_) {
    client_->TriggerAccessibilityAlert(alert);
  }
}

void AccessibilityController::TriggerAccessibilityAlertWithMessage(
    const std::string& message) {
  if (client_) {
    client_->TriggerAccessibilityAlertWithMessage(message);
  }
}

void AccessibilityController::PlayEarcon(Sound sound_key) {
  if (client_) {
    client_->PlayEarcon(sound_key);
  }
}

base::TimeDelta AccessibilityController::PlayShutdownSound() {
  return client_ ? client_->PlayShutdownSound() : base::TimeDelta();
}

void AccessibilityController::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  if (client_) {
    client_->HandleAccessibilityGesture(gesture, location);
  }
}

void AccessibilityController::ToggleDictation() {
  // Do nothing if dictation is not enabled.
  if (!dictation().enabled()) {
    return;
  }

  if (client_) {
    const bool is_active = client_->ToggleDictation();
    SetDictationActive(is_active);
    if (is_active) {
      Shell::Get()->OnDictationStarted();
    } else {
      Shell::Get()->OnDictationEnded();
    }
  }
}

void AccessibilityController::SetDictationActive(bool is_active) {
  dictation_active_ = is_active;
}

void AccessibilityController::ToggleDictationFromSource(
    DictationToggleSource source) {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Dictation"));
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosDictation.ToggleDictationMethod",
                            source);

  dictation().SetEnabled(true);
  ToggleDictation();
}

void AccessibilityController::EnableSelectToSpeakWithDialog() {
  if (!::features::IsAccessibilitySelectToSpeakShortcutEnabled() ||
      select_to_speak().enabled()) {
    return;
  }

  if (active_user_prefs_
          ->FindPreference(prefs::kAccessibilitySelectToSpeakEnabled)
          ->IsManaged() &&
      !active_user_prefs_->GetBoolean(
          prefs::kAccessibilitySelectToSpeakEnabled)) {
    // Don't show the dialog if Select to speak has been disabled by a policy.
    return;
  }

  if (active_user_prefs_->GetBoolean(
          prefs::kSelectToSpeakAcceleratorDialogHasBeenAccepted)) {
    // Enable Select to Speak if the confirmation dialog has been previously
    // accepted.
    OnSelectToSpeakKeyboardDialogAccepted();
  } else {
    // Show the confirmation dialog if it hasn't been accepted yet.
    ShowSelectToSpeakKeyboardDialog();
  }
}

void AccessibilityController::EnableOrToggleDictationFromSource(
    DictationToggleSource source) {
  if (dictation().enabled()) {
    ToggleDictationFromSource(source);
  } else if (source == DictationToggleSource::kKeyboard) {
    // Only allow direct-enabling of Dictation from the keyboard. Show the
    // confirmation dialog if it hasn't been accepted yet.
    if (active_user_prefs_->GetBoolean(
            prefs::kDictationAcceleratorDialogHasBeenAccepted)) {
      OnDictationKeyboardDialogAccepted();
    } else {
      ShowDictationKeyboardDialog();
    }
  }
}

void AccessibilityController::ShowDictationKeyboardDialog() {
  if (!client_) {
    return;
  }

  dictation_keyboard_dialog_showing_for_testing_ = true;

  std::string dictation_locale;
  if (active_user_prefs_->GetString(prefs::kAccessibilityDictationLocale)
          .empty()) {
    dictation_locale = client_->GetDictationDefaultLocale(/*new_user=*/true);
  } else {
    dictation_locale =
        active_user_prefs_->GetString(prefs::kAccessibilityDictationLocale);
  }

  std::u16string display_locale = l10n_util::GetDisplayNameForLocale(
      /*locale=*/dictation_locale, /*display_locale=*/dictation_locale,
      /*is_for_ui=*/true);
  std::vector<std::u16string> replacements{display_locale};
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_DICTATION_KEYBOARD_DIALOG_TITLE);
  std::u16string description =
      ::features::IsDictationOfflineAvailable()
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_DICTATION_KEYBOARD_DIALOG_DESCRIPTION_SODA_AVAILABLE,
                replacements, nullptr)
          : l10n_util::GetStringFUTF16(
                IDS_ASH_DICTATION_KEYBOARD_DIALOG_DESCRIPTION_SODA_NOT_AVAILABLE,
                replacements, nullptr);
  ShowConfirmationDialog(
      title, description, l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON),
      l10n_util::GetStringUTF16(IDS_APP_CANCEL),
      base::BindOnce(
          &AccessibilityController::OnDictationKeyboardDialogAccepted,
          GetWeakPtr()),
      base::BindOnce(
          &AccessibilityController::OnDictationKeyboardDialogDismissed,
          GetWeakPtr()),
      base::BindOnce(
          &AccessibilityController::OnDictationKeyboardDialogDismissed,
          GetWeakPtr()));
}

void AccessibilityController::OnDictationKeyboardDialogAccepted() {
  dictation_keyboard_dialog_showing_for_testing_ = false;
  active_user_prefs_->SetBoolean(
      prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  confirmation_dialog_.reset();
  base::RecordAction(base::UserMetricsAction("Accel_Enable_Dictation"));
  dictation().SetEnabled(true);
}

void AccessibilityController::OnDictationKeyboardDialogDismissed() {
  dictation_keyboard_dialog_showing_for_testing_ = false;
}

void AccessibilityController::ShowSelectToSpeakKeyboardDialog() {
  if (!client_ || !::features::IsAccessibilitySelectToSpeakShortcutEnabled()) {
    return;
  }

  std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_SELECT_TO_SPEAK_KEYBOARD_DIALOG_TITLE);

  std::u16string modifier_key;
  if (Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()) {
    modifier_key = l10n_util::GetStringUTF16(IDS_KSV_MODIFIER_LAUNCHER);
  } else {
    modifier_key = l10n_util::GetStringUTF16(IDS_KSV_MODIFIER_SEARCH);
  }
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_ASH_SELECT_TO_SPEAK_KEYBOARD_DIALOG_DESCRIPTION, modifier_key);
  ShowConfirmationDialog(
      title, description, l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON),
      l10n_util::GetStringUTF16(IDS_APP_CANCEL),
      base::BindOnce(
          &AccessibilityController::OnSelectToSpeakKeyboardDialogAccepted,
          GetWeakPtr()),
      base::BindOnce(
          &AccessibilityController::OnSelectToSpeakKeyboardDialogDismissed,
          GetWeakPtr()),
      base::BindOnce(
          &AccessibilityController::OnSelectToSpeakKeyboardDialogDismissed,
          GetWeakPtr()));
}

void AccessibilityController::OnSelectToSpeakKeyboardDialogAccepted() {
  active_user_prefs_->SetBoolean(
      prefs::kSelectToSpeakAcceleratorDialogHasBeenAccepted, true);
  confirmation_dialog_.reset();
  select_to_speak().SetEnabled(true);
}

void AccessibilityController::OnSelectToSpeakKeyboardDialogDismissed() {
  confirmation_dialog_.reset();
}

void AccessibilityController::ShowDictationLanguageUpgradedNudge(
    const std::string& dictation_locale,
    const std::string& application_locale) {
  const std::u16string language_name = l10n_util::GetDisplayNameForLocale(
      dictation_locale, application_locale, /*is_for_ui=*/true);
  const std::u16string body_text = l10n_util::GetStringFUTF16(
      IDS_ASH_DICTATION_LANGUAGE_SUPPORTED_OFFLINE_NUDGE, language_name);

  AnchoredNudgeData nudge_data(kDictationLanguageUpgradedNudgeId,
                               NudgeCatalogName::kDictation, body_text);
  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void AccessibilityController::SilenceSpokenFeedback() {
  if (client_) {
    client_->SilenceSpokenFeedback();
  }
}

bool AccessibilityController::ShouldToggleSpokenFeedbackViaTouch() const {
  return client_ && client_->ShouldToggleSpokenFeedbackViaTouch();
}

void AccessibilityController::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  if (client_) {
    client_->PlaySpokenFeedbackToggleCountdown(tick_count);
  }
}

bool AccessibilityController::IsEnterpriseIconVisibleInTrayMenu(
    const std::string& path) {
  return active_user_prefs_ &&
         active_user_prefs_->FindPreference(path)->IsManaged();
}

void AccessibilityController::SetClient(AccessibilityControllerClient* client) {
  client_ = client;
}

void AccessibilityController::SetDarkenScreen(bool darken) {
  if (darken && !scoped_backlights_forced_off_) {
    scoped_backlights_forced_off_ =
        Shell::Get()->backlights_forced_off_setter()->ForceBacklightsOff();
  } else if (!darken && scoped_backlights_forced_off_) {
    scoped_backlights_forced_off_.reset();
  }
}

void AccessibilityController::BrailleDisplayStateChanged(bool connected) {
  A11yNotificationType type = A11yNotificationType::kNone;
  if (connected && spoken_feedback().enabled()) {
    type = A11yNotificationType::kBrailleDisplayConnected;
  } else if (connected && !spoken_feedback().enabled()) {
    type = A11yNotificationType::kSpokenFeedbackBrailleEnabled;
  }

  if (connected) {
    SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  }
  NotifyAccessibilityStatusChanged();

  ShowAccessibilityNotification(
      A11yNotificationWrapper(type, std::vector<std::u16string>()));
}

void AccessibilityController::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {
  if (!accessibility_highlight_controller_) {
    return;
  }
  accessibility_highlight_controller_->SetFocusHighlightRect(bounds_in_screen);
}

void AccessibilityController::SetCaretBounds(
    const gfx::Rect& bounds_in_screen) {
  if (!accessibility_highlight_controller_) {
    return;
  }
  accessibility_highlight_controller_->SetCaretBounds(bounds_in_screen);
}

void AccessibilityController::SetAccessibilityPanelAlwaysVisible(
    bool always_visible) {
  GetLayoutManager()->SetAlwaysVisible(always_visible);
}

void AccessibilityController::SetAccessibilityPanelBounds(
    const gfx::Rect& bounds,
    AccessibilityPanelState state) {
  GetLayoutManager()->SetPanelBounds(bounds, state);
}

void AccessibilityController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  // Make |kA11yPrefsForRecommendedValueOnSignin| observing recommended values
  // on signin screen. See PolicyRecommendationRestorer.
  PolicyRecommendationRestorer* policy_recommendation_restorer =
      Shell::Get()->policy_recommendation_restorer();
  for (auto* const pref_name : kA11yPrefsForRecommendedValueOnSignin) {
    policy_recommendation_restorer->ObservePref(pref_name);
  }

  // Observe user settings. This must happen after PolicyRecommendationRestorer.
  ObservePrefs(prefs);
}

void AccessibilityController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  // This is guaranteed to be received after
  // OnSigninScreenPrefServiceInitialized() so only copy the signin prefs if
  // needed here.
  CopySigninPrefsIfNeeded(active_user_prefs_, prefs);
  ObservePrefs(prefs);
}

void AccessibilityController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state != SessionState::ACTIVE) {
    // Log metrics for how long the features were enabled if needed.
    for (auto& feature : features_) {
      feature->LogDurationMetric();
    }
  }
  // Everything behind the lock screen is in
  // kShellWindowId_NonLockScreenContainersContainer. If the session state is
  // changed to block the user session due to the lock screen or similar,
  // everything in that window should be made invisible for accessibility.
  // This keeps a11y features from being able to access parts of the tree
  // that are visibly hidden behind the lock screen.
  aura::Window* container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_NonLockScreenContainersContainer);
  container->SetProperty(
      ui::kAXConsiderInvisibleAndIgnoreChildren,
      Shell::Get()->session_controller()->IsUserSessionBlocked());
}

AccessibilityEventRewriter*
AccessibilityController::GetAccessibilityEventRewriterForTest() {
  return accessibility_event_rewriter_;
}

DisableTrackpadEventRewriter*
AccessibilityController::GetDisableTrackpadEventRewriterForTest() {
  return disable_trackpad_event_rewriter_;
}

FilterKeysEventRewriter*
AccessibilityController::GetFilterKeysEventRewriterForTest() {
  return filter_keys_event_rewriter_;
}

void AccessibilityController::DisableAutoClickConfirmationDialogForTest() {
  no_auto_click_confirmation_dialog_for_testing_ = true;
}

void AccessibilityController::
    DisableSwitchAccessDisableConfirmationDialogTesting() {
  no_switch_access_disable_confirmation_dialog_for_testing_ = true;
}

void AccessibilityController::DisableSwitchAccessEnableNotificationTesting() {
  skip_switch_access_notification_ = true;
}

void AccessibilityController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (spoken_feedback().enabled()) {
    // Show accessibility notification when tablet mode transition is completed.
    if (state == display::TabletState::kInTabletMode ||
        state == display::TabletState::kInClamshellMode) {
      ShowAccessibilityNotification(
          A11yNotificationWrapper(A11yNotificationType::kSpokenFeedbackEnabled,
                                  std::vector<std::u16string>()));
    }
  }
}

void AccessibilityController::ObservePrefs(PrefService* prefs) {
  DCHECK(prefs);

  active_user_prefs_ = prefs;

  // Watch for pref updates from webui settings and policy.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  // It is safe to use base::Unreatined since we own pref_change_registrar.
  for (const std::unique_ptr<Feature>& feature : features_) {
    DCHECK(feature);
    pref_change_registrar_->Add(
        feature->pref_name(),
        base::BindRepeating(&AccessibilityController::Feature::UpdateFromPref,
                            base::Unretained(feature.get())));
    if (feature->conflicting_feature() != FeatureType::kNoConflictingFeature) {
      feature->ObserveConflictingFeature();
    }
    // Features will be initialized from current prefs later.
  }

  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickDelayMs,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickDelayFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickEventType,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickEventTypeFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickRevertToLeftClick,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickRevertToLeftClickFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickStabilizePosition,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickStabilizePositionFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickMovementThreshold,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickMovementThresholdFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickMenuPosition,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickMenuPositionFromPref,
          base::Unretained(this)));
  if (::features::IsAccessibilityMouseKeysEnabled()) {
    pref_change_registrar_->Add(
        prefs::kAccessibilityMouseKeysAcceleration,
        base::BindRepeating(
            &AccessibilityController::UpdateMouseKeysAccelerationFromPref,
            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityMouseKeysMaxSpeed,
        base::BindRepeating(
            &AccessibilityController::UpdateMouseKeysMaxSpeedFromPref,
            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityMouseKeysUsePrimaryKeys,
        base::BindRepeating(
            &AccessibilityController::UpdateMouseKeysUsePrimaryKeysFromPref,
            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityMouseKeysDominantHand,
        base::BindRepeating(
            &AccessibilityController::UpdateMouseKeysDominantHandFromPref,
            base::Unretained(this)));
  }
  pref_change_registrar_->Add(
      prefs::kAccessibilityFloatingMenuPosition,
      base::BindRepeating(
          &AccessibilityController::UpdateFloatingMenuPositionFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorDipSize,
      base::BindRepeating(&AccessibilityController::UpdateLargeCursorFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityShortcutsEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateShortcutsEnabledFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes,
      base::BindRepeating(
          &AccessibilityController::UpdateSwitchAccessKeyCodesFromPref,
          base::Unretained(this), SwitchAccessCommand::kSelect));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes,
      base::BindRepeating(
          &AccessibilityController::UpdateSwitchAccessKeyCodesFromPref,
          base::Unretained(this), SwitchAccessCommand::kNext));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes,
      base::BindRepeating(
          &AccessibilityController::UpdateSwitchAccessKeyCodesFromPref,
          base::Unretained(this), SwitchAccessCommand::kPrevious));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateSwitchAccessAutoScanEnabledFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
      base::BindRepeating(
          &AccessibilityController::UpdateSwitchAccessAutoScanSpeedFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
      base::BindRepeating(&AccessibilityController::
                              UpdateSwitchAccessAutoScanKeyboardSpeedFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond,
      base::BindRepeating(
          &AccessibilityController::UpdateSwitchAccessPointScanSpeedFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled,
      base::BindRepeating(&AccessibilityController::
                              UpdateTabletModeShelfNavigationButtonsFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityCursorColor,
      base::BindRepeating(&AccessibilityController::UpdateCursorColorFromPrefs,
                          base::Unretained(this), /*notify*/ true));
  pref_change_registrar_->Add(
      prefs::kAccessibilityColorVisionCorrectionAmount,
      base::BindRepeating(
          &AccessibilityController::UpdateColorCorrectionFromPrefs,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityColorVisionCorrectionType,
      base::BindRepeating(
          &AccessibilityController::UpdateColorCorrectionFromPrefs,
          base::Unretained(this)));
  if (::features::IsAccessibilityCaretBlinkIntervalSettingEnabled()) {
    pref_change_registrar_->Add(
        prefs::kAccessibilityCaretBlinkInterval,
        base::BindRepeating(
            &AccessibilityController::UpdateCaretBlinkIntervalFromPrefs,
            base::Unretained(this)));
  }
  if (::features::IsAccessibilityFlashScreenFeatureEnabled()) {
    pref_change_registrar_->Add(
        prefs::kAccessibilityFlashNotificationsColor,
        base::BindRepeating(
            &AccessibilityController::UpdateFlashNotificationsFromPrefs,
            base::Unretained(this)));
  }
  if (::features::IsAccessibilityDisableTrackpadEnabled()) {
    pref_change_registrar_->Add(
        prefs::kAccessibilityDisableTrackpadMode,
        base::BindRepeating(
            &AccessibilityController::UpdateDisableTrackpadFromPrefs,
            base::Unretained(this)));
  }

  for (const std::unique_ptr<Feature>& feature : features_) {
    // Log previous duration and clear duration metric if necessary
    // when the profile has changed.
    feature->LogDurationMetric();

    // Load current state.
    feature->UpdateFromPref();
  }

  // Load current state of other prefs.
  UpdateAutoclickDelayFromPref();
  UpdateAutoclickEventTypeFromPref();
  UpdateAutoclickRevertToLeftClickFromPref();
  UpdateAutoclickStabilizePositionFromPref();
  UpdateAutoclickMovementThresholdFromPref();
  UpdateAutoclickMenuPositionFromPref();
  if (::features::IsAccessibilityMouseKeysEnabled()) {
    UpdateMouseKeysAccelerationFromPref();
    UpdateMouseKeysMaxSpeedFromPref();
    UpdateMouseKeysUsePrimaryKeysFromPref();
    UpdateMouseKeysDominantHandFromPref();
  }
  UpdateFloatingMenuPositionFromPref();
  UpdateLargeCursorFromPref();
  UpdateCursorColorFromPrefs(/*notify=*/true);
  UpdateShortcutsEnabledFromPref();
  UpdateTabletModeShelfNavigationButtonsFromPref();
  UpdateColorCorrectionFromPrefs();
  UpdateCaretBlinkIntervalFromPrefs();

  if (::features::IsAccessibilityFaceGazeEnabled()) {
    UpdateFaceGazeFromPrefs();
  }
  if (::features::IsAccessibilityFlashScreenFeatureEnabled()) {
    UpdateFlashNotificationsFromPrefs();
  }
  if (::features::IsAccessibilityDisableTrackpadEnabled()) {
    UpdateDisableTrackpadFromPrefs();
  }
}

void AccessibilityController::UpdateAutoclickDelayFromPref() {
  DCHECK(active_user_prefs_);
  base::TimeDelta autoclick_delay = base::Milliseconds(int64_t{
      active_user_prefs_->GetInteger(prefs::kAccessibilityAutoclickDelayMs)});

  if (autoclick_delay_ == autoclick_delay) {
    return;
  }
  autoclick_delay_ = autoclick_delay;

  Shell::Get()->autoclick_controller()->SetAutoclickDelay(autoclick_delay_);
}

void AccessibilityController::UpdateAutoclickEventTypeFromPref() {
  Shell::Get()->autoclick_controller()->SetAutoclickEventType(
      GetAutoclickEventType());
}

void AccessibilityController::SetAutoclickEventType(
    AutoclickEventType event_type) {
  if (!active_user_prefs_) {
    return;
  }
  active_user_prefs_->SetInteger(prefs::kAccessibilityAutoclickEventType,
                                 static_cast<int>(event_type));
  active_user_prefs_->CommitPendingWrite();
  Shell::Get()->autoclick_controller()->SetAutoclickEventType(event_type);
}

AutoclickEventType AccessibilityController::GetAutoclickEventType() {
  DCHECK(active_user_prefs_);
  return static_cast<AutoclickEventType>(
      active_user_prefs_->GetInteger(prefs::kAccessibilityAutoclickEventType));
}

void AccessibilityController::UpdateAutoclickRevertToLeftClickFromPref() {
  DCHECK(active_user_prefs_);
  bool revert_to_left_click = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityAutoclickRevertToLeftClick);

  Shell::Get()->autoclick_controller()->set_revert_to_left_click(
      revert_to_left_click);
}

void AccessibilityController::UpdateAutoclickStabilizePositionFromPref() {
  DCHECK(active_user_prefs_);
  bool stabilize_position = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityAutoclickStabilizePosition);

  Shell::Get()->autoclick_controller()->set_stabilize_click_position(
      stabilize_position);
}

void AccessibilityController::UpdateAutoclickMovementThresholdFromPref() {
  DCHECK(active_user_prefs_);
  int movement_threshold = active_user_prefs_->GetInteger(
      prefs::kAccessibilityAutoclickMovementThreshold);

  Shell::Get()->autoclick_controller()->SetMovementThreshold(
      movement_threshold);
}

void AccessibilityController::UpdateAutoclickMenuPositionFromPref() {
  Shell::Get()->autoclick_controller()->SetMenuPosition(
      GetAutoclickMenuPosition());
}

void AccessibilityController::UpdateMouseKeysAccelerationFromPref() {
  DCHECK(active_user_prefs_);
  double acceleration =
      active_user_prefs_->GetDouble(prefs::kAccessibilityMouseKeysAcceleration);
  Shell::Get()->mouse_keys_controller()->set_acceleration(acceleration);
}

void AccessibilityController::UpdateMouseKeysMaxSpeedFromPref() {
  DCHECK(active_user_prefs_);
  double max_speed =
      active_user_prefs_->GetDouble(prefs::kAccessibilityMouseKeysMaxSpeed);
  Shell::Get()->mouse_keys_controller()->SetMaxSpeed(max_speed);
}

void AccessibilityController::UpdateMouseKeysUsePrimaryKeysFromPref() {
  DCHECK(active_user_prefs_);
  bool value = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityMouseKeysUsePrimaryKeys);
  Shell::Get()->mouse_keys_controller()->set_use_primary_keys(value);
}

void AccessibilityController::UpdateMouseKeysDominantHandFromPref() {
  DCHECK(active_user_prefs_);
  MouseKeysDominantHand dominant_hand =
      static_cast<MouseKeysDominantHand>(active_user_prefs_->GetInteger(
          prefs::kAccessibilityMouseKeysDominantHand));
  Shell::Get()->mouse_keys_controller()->set_left_handed(
      dominant_hand == MouseKeysDominantHand::kLeftHandDominant);
}

void AccessibilityController::SetAutoclickMenuPosition(
    FloatingMenuPosition position) {
  if (!active_user_prefs_) {
    return;
  }
  active_user_prefs_->SetInteger(prefs::kAccessibilityAutoclickMenuPosition,
                                 static_cast<int>(position));
  active_user_prefs_->CommitPendingWrite();
  Shell::Get()->autoclick_controller()->SetMenuPosition(position);
}

FloatingMenuPosition AccessibilityController::GetAutoclickMenuPosition() {
  DCHECK(active_user_prefs_);
  return static_cast<FloatingMenuPosition>(active_user_prefs_->GetInteger(
      prefs::kAccessibilityAutoclickMenuPosition));
}

void AccessibilityController::RequestAutoclickScrollableBoundsForPoint(
    const gfx::Point& point_in_screen) {
  if (client_) {
    client_->RequestAutoclickScrollableBoundsForPoint(point_in_screen);
  }
}

void AccessibilityController::MagnifierBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  if (client_) {
    client_->MagnifierBoundsChanged(bounds_in_screen);
  }
}

void AccessibilityController::UpdateFloatingPanelBoundsIfNeeded() {
  Shell* shell = Shell::Get();
  if (shell->accessibility_controller()->autoclick().enabled()) {
    shell->autoclick_controller()->UpdateAutoclickMenuBoundsIfNeeded();
  }
  if (shell->accessibility_controller()->sticky_keys().enabled()) {
    shell->sticky_keys_controller()->UpdateStickyKeysOverlayBoundsIfNeeded();
  }
}

void AccessibilityController::UpdateAutoclickMenuBoundsIfNeeded() {
  Shell::Get()->autoclick_controller()->UpdateAutoclickMenuBoundsIfNeeded();
}

void AccessibilityController::HandleAutoclickScrollableBoundsFound(
    const gfx::Rect& bounds_in_screen) {
  Shell::Get()->autoclick_controller()->HandleAutoclickScrollableBoundsFound(
      bounds_in_screen);
}

void AccessibilityController::SetFloatingMenuPosition(
    FloatingMenuPosition position) {
  if (!active_user_prefs_) {
    return;
  }
  active_user_prefs_->SetInteger(prefs::kAccessibilityFloatingMenuPosition,
                                 static_cast<int>(position));
  active_user_prefs_->CommitPendingWrite();
}

void AccessibilityController::UpdateFloatingMenuPositionFromPref() {
  if (floating_menu_controller_) {
    floating_menu_controller_->SetMenuPosition(GetFloatingMenuPosition());
  }
}

FloatingMenuPosition AccessibilityController::GetFloatingMenuPosition() {
  DCHECK(active_user_prefs_);
  return static_cast<FloatingMenuPosition>(active_user_prefs_->GetInteger(
      prefs::kAccessibilityFloatingMenuPosition));
}

void AccessibilityController::UpdateLargeCursorFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = large_cursor().enabled();
  const int size = enabled ? active_user_prefs_->GetInteger(
                                 prefs::kAccessibilityLargeCursorDipSize)
                           : kDefaultLargeCursorSize;

  if (large_cursor_size_in_dip_ == size) {
    return;
  }

  large_cursor_size_in_dip_ = size;

  NotifyAccessibilityStatusChanged();

  Shell* shell = Shell::Get();
  shell->cursor_manager()->SetCursorSize(enabled ? ui::CursorSize::kLarge
                                                 : ui::CursorSize::kNormal);
  shell->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
  shell->UpdateCursorCompositingEnabled();
}

void AccessibilityController::UpdateCursorColorFromPrefs(bool notify) {
  DCHECK(active_user_prefs_);

  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityCursorColorEnabled);
  Shell* shell = Shell::Get();
  shell->SetCursorColor(
      enabled ? active_user_prefs_->GetInteger(prefs::kAccessibilityCursorColor)
              : kDefaultCursorColor);
  if (notify) {
    NotifyAccessibilityStatusChanged();
  }
  shell->UpdateCursorCompositingEnabled();
}

void AccessibilityController::UpdateFaceGazeFromPrefs() {
  if (!::features::IsAccessibilityFaceGazeEnabled()) {
    return;
  }
}

void AccessibilityController::UpdateFlashNotificationsFromPrefs() {
  if (!::features::IsAccessibilityFlashScreenFeatureEnabled()) {
    return;
  }
  flash_screen_controller_->set_enabled(active_user_prefs_->GetBoolean(
      prefs::kAccessibilityFlashNotificationsEnabled));
  flash_screen_controller_->set_color(active_user_prefs_->GetInteger(
      prefs::kAccessibilityFlashNotificationsColor));
}

void AccessibilityController::UpdateDisableTrackpadFromPrefs() {
  if (!disable_trackpad_event_rewriter_ ||
      !::features::IsAccessibilityDisableTrackpadEnabled()) {
    return;
  }

  DisableTrackpadWithDialog();
}

void AccessibilityController::DisableTrackpadWithDialog() {
  const DisableTrackpadMode trackpad_mode = static_cast<DisableTrackpadMode>(
      active_user_prefs_->GetInteger(prefs::kAccessibilityDisableTrackpadMode));

  switch (trackpad_mode) {
    case DisableTrackpadMode::kAlways:
      ShowDisableTrackpadDialog();
      break;

    case DisableTrackpadMode::kOnExternalMouseConnected:
      if (Shell::Get()
              ->input_device_settings_controller()
              ->GetConnectedMice()
              .size() > 0) {
        ShowDisableTrackpadDialog();
      }
      break;

    case DisableTrackpadMode::kNever:
      ShowToast(AccessibilityToastType::kTrackpadDisabled);
      disable_trackpad_event_rewriter_->SetEnabled(false);
      break;
  }
}

void AccessibilityController::OnMouseConnected(const mojom::Mouse& mouse) {
  ExternalDeviceConnected();
}

void AccessibilityController::OnTouchpadConnected(
    const mojom::Touchpad& touchpad) {
  ExternalDeviceConnected();
}

void AccessibilityController::ExternalDeviceConnected() {
  if (!disable_trackpad_event_rewriter_) {
    return;
  }

  const DisableTrackpadMode trackpad_mode = static_cast<DisableTrackpadMode>(
      active_user_prefs_->GetInteger(prefs::kAccessibilityDisableTrackpadMode));

  const bool trackpad_disabled = disable_trackpad_event_rewriter_->IsEnabled();
  if (trackpad_mode == DisableTrackpadMode::kOnExternalMouseConnected &&
      !trackpad_disabled) {
    DisableTrackpadWithDialog();
  }
}

void AccessibilityController::ShowDisableTrackpadDialog() {
  // The internal trackpad should be disabled before the user clicks "Accept",
  // This is done to ensure the user can navigate with their keyboard.
  disable_trackpad_event_rewriter_->SetEnabled(true);
  const DisableTrackpadMode disable_trackpad_mode =
      static_cast<DisableTrackpadMode>(active_user_prefs_->GetInteger(
          prefs::kAccessibilityDisableTrackpadMode));
  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_DISABLE_TRACKPAD_DIALOG_TITLE);
  const std::u16string description =
      disable_trackpad_mode == DisableTrackpadMode::kAlways
          ? l10n_util::GetStringUTF16(
                IDS_ASH_DISABLE_TRACKPAD_DIALOG_DESCRIPTION)
          : l10n_util::GetStringUTF16(
                IDS_ASH_DISABLE_TRACKPAD_DIALOG_EXTERNAL_MOUSE_DESCRIPTION);

  ShowConfirmationDialog(
      title, description, l10n_util::GetStringUTF16(IDS_ASH_CONFIRM_BUTTON),
      l10n_util::GetStringUTF16(IDS_APP_CANCEL),
      base::BindOnce(&AccessibilityController::OnDisableTrackpadDialogAccepted,
                     GetWeakPtr()),
      base::BindOnce(&AccessibilityController::OnDisableTrackpadDialogDismissed,
                     GetWeakPtr()),
      base::BindOnce(&AccessibilityController::OnDisableTrackpadDialogDismissed,
                     GetWeakPtr()));
}

void AccessibilityController::OnDisableTrackpadDialogAccepted() {
  confirmation_dialog_.reset();
  ShowAccessibilityNotification(A11yNotificationWrapper(
      A11yNotificationType::kTrackpadDisabled, std::vector<std::u16string>(),
      base::BindRepeating(
          &AccessibilityController::OnTrackpadNotificationClicked,
          GetWeakPtr())));
}

void AccessibilityController::OnDisableTrackpadDialogDismissed() {
  confirmation_dialog_.reset();
  active_user_prefs_->SetInteger(prefs::kAccessibilityDisableTrackpadMode,
                                 static_cast<int>(DisableTrackpadMode::kNever));
}

DisableTrackpadMode AccessibilityController::GetDisableTrackpadMode() {
  return static_cast<DisableTrackpadMode>(
      active_user_prefs_->GetInteger(prefs::kAccessibilityDisableTrackpadMode));
}

void AccessibilityController::UpdateColorCorrectionFromPrefs() {
  DCHECK(active_user_prefs_);

  auto* color_enhancement_controller =
      Shell::Get()->color_enhancement_controller();

  if (!active_user_prefs_->GetBoolean(
          prefs::kAccessibilityColorCorrectionEnabled)) {
    color_enhancement_controller->SetColorCorrectionEnabledAndUpdateDisplays(
        false);
    return;
  }

  const float cvd_correction_amount =
      active_user_prefs_->GetInteger(
          prefs::kAccessibilityColorVisionCorrectionAmount) /
      100.0f;
  ColorVisionCorrectionType type =
      static_cast<ColorVisionCorrectionType>(active_user_prefs_->GetInteger(
          prefs::kAccessibilityColorVisionCorrectionType));
  color_enhancement_controller->SetColorVisionCorrectionFilter(
      type, cvd_correction_amount);

  // Ensure displays get updated.
  color_enhancement_controller->SetColorCorrectionEnabledAndUpdateDisplays(
      true);
}

void AccessibilityController::UpdateCaretBlinkIntervalFromPrefs() const {
  if (!::features::IsAccessibilityCaretBlinkIntervalSettingEnabled()) {
    return;
  }
  base::TimeDelta caret_blink_interval = base::Milliseconds(
      active_user_prefs_->GetInteger(prefs::kAccessibilityCaretBlinkInterval));
  bool notify_dark = false;
  bool notify_web = false;
  bool notify_native = false;
  auto* native_theme_dark = ui::NativeTheme::GetInstanceForDarkUI();
  if (native_theme_dark->GetCaretBlinkInterval() != caret_blink_interval) {
    notify_dark = true;
    native_theme_dark->set_caret_blink_interval(caret_blink_interval);
  }
  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  if (native_theme_web->GetCaretBlinkInterval() != caret_blink_interval) {
    notify_web = true;
    native_theme_web->set_caret_blink_interval(caret_blink_interval);
  }
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  if (native_theme->GetCaretBlinkInterval() != caret_blink_interval) {
    notify_native = true;
    native_theme->set_caret_blink_interval(caret_blink_interval);
  }
  // Avoid unnecessary notifications.
  if (notify_dark) {
    native_theme_dark->NotifyOnNativeThemeUpdated();
  }
  if (notify_web) {
    native_theme_web->NotifyOnNativeThemeUpdated();
  }
  if (notify_native) {
    native_theme->NotifyOnNativeThemeUpdated();
  }
}

std::optional<ui::KeyboardCode>
AccessibilityController::GetCaretBrowsingActionKey() {
  const std::vector<ui::KeyboardDevice>& keyboards =
      ui::DeviceDataManager::GetInstance()->GetKeyboardDevices();
  std::optional<ui::TopRowActionKey> key;
  if (keyboards.size() > 0) {
    if (ash::Shell::Get()
            ->event_rewriter_controller()
            ->event_rewriter_ash_delegate()
            ->TopRowKeysAreFunctionKeys(keyboards[0].id)) {
      return ui::VKEY_F7;
    }
    key = ash::Shell::Get()
              ->keyboard_capability()
              ->GetCorrespondingActionKeyForFKey(keyboards[0], ui::VKEY_F7);
  }
  if (key) {
    return ui::KeyboardCapability::ConvertToKeyboardCode(*key);
  }
  return std::nullopt;
}

void AccessibilityController::UpdateAccessibilityHighlightingFromPrefs() {
  if (!caret_highlight().enabled() && !cursor_highlight().enabled() &&
      !focus_highlight().enabled()) {
    accessibility_highlight_controller_.reset();
    return;
  }

  if (!accessibility_highlight_controller_) {
    accessibility_highlight_controller_ =
        std::make_unique<AccessibilityHighlightController>();
  }

  accessibility_highlight_controller_->HighlightCaret(
      caret_highlight().enabled());
  accessibility_highlight_controller_->HighlightCursor(
      cursor_highlight().enabled());
  accessibility_highlight_controller_->HighlightFocus(
      focus_highlight().enabled());
}

void AccessibilityController::MaybeCreateSelectToSpeakEventHandler() {
  // Construct the handler as needed when Select-to-Speak is enabled and the
  // delegate is set. Otherwise, destroy the handler when Select-to-Speak is
  // disabled or the delegate has been destroyed.
  if (!select_to_speak().enabled() ||
      !select_to_speak_event_handler_delegate_) {
    select_to_speak_event_handler_.reset();
    return;
  }

  if (select_to_speak_event_handler_) {
    return;
  }

  select_to_speak_event_handler_ = std::make_unique<SelectToSpeakEventHandler>(
      select_to_speak_event_handler_delegate_);
}

void AccessibilityController::UpdateSwitchAccessKeyCodesFromPref(
    SwitchAccessCommand command) {
  if (!active_user_prefs_) {
    return;
  }

  SyncSwitchAccessPrefsToSignInProfile();

  if (!accessibility_event_rewriter_) {
    return;
  }

  std::string pref_key = PrefKeyForSwitchAccessCommand(command);
  const base::Value::Dict& key_codes_pref =
      active_user_prefs_->GetDict(pref_key);
  std::map<int, std::set<std::string>> key_codes;
  for (const auto v : key_codes_pref) {
    int key_code;
    if (!base::StringToInt(v.first, &key_code)) {
      NOTREACHED();
    }

    key_codes[key_code] = std::set<std::string>();

    for (const base::Value& device_type : v.second.GetList()) {
      key_codes[key_code].insert(device_type.GetString());
    }

    DCHECK(!key_codes[key_code].empty());
  }

  std::string uma_name = UmaNameForSwitchAccessCommand(command);
  if (key_codes.size() == 0) {
    base::UmaHistogramEnumeration(uma_name, SwitchAccessKeyCode::kNone);
  }
  for (const auto& key_code : key_codes) {
    base::UmaHistogramEnumeration(
        uma_name, static_cast<SwitchAccessKeyCode>(key_code.first));
  }

  accessibility_event_rewriter_->SetKeyCodesForSwitchAccessCommand(key_codes,
                                                                   command);
}

void AccessibilityController::UpdateSwitchAccessAutoScanEnabledFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled);

  base::UmaHistogramBoolean("Accessibility.CrosSwitchAccess.AutoScan", enabled);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityController::UpdateSwitchAccessAutoScanSpeedFromPref() {
  DCHECK(active_user_prefs_);
  const int speed_ms = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessAutoScanSpeedMs);

  base::UmaHistogramCustomCounts(
      "Accessibility.CrosSwitchAccess.AutoScan.SpeedMs", speed_ms, 1 /* min */,
      10000 /* max */, 100 /* buckets */);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityController::
    UpdateSwitchAccessAutoScanKeyboardSpeedFromPref() {
  DCHECK(active_user_prefs_);
  const int speed_ms = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs);

  base::UmaHistogramCustomCounts(
      "Accessibility.CrosSwitchAccess.AutoScan.KeyboardSpeedMs", speed_ms,
      1 /* min */, 10000 /* max */, 100 /* buckets */);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityController::UpdateSwitchAccessPointScanSpeedFromPref() {
  // TODO(accessibility): Log histogram for point scan speed
  DCHECK(active_user_prefs_);
  const int point_scan_speed_dips_per_second = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond);

  SetPointScanSpeedDipsPerSecond(point_scan_speed_dips_per_second);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityController::SwitchAccessDisableDialogClosed(
    bool disable_dialog_accepted) {
  switch_access_disable_dialog_showing_ = false;
  // Always deactivate switch access. Turning switch access off ensures it is
  // re-activated correctly.
  // The pref was already disabled, but we left switch access on so the user
  // could interact with the dialog.
  DeactivateSwitchAccess();
  if (disable_dialog_accepted) {
    RemoveAccessibilityNotification();
    NotifyAccessibilityStatusChanged();
    SyncSwitchAccessPrefsToSignInProfile();
  } else {
    // Reset the preference (which was already set to false). Doing so turns
    // switch access back on.
    skip_switch_access_notification_ = true;
    switch_access().SetEnabled(true);
  }
}

void AccessibilityController::UpdateKeyCodesAfterSwitchAccessEnabled() {
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kSelect);
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kNext);
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kPrevious);
}

void AccessibilityController::ActivateSwitchAccess() {
  switch_access_bubble_controller_ =
      std::make_unique<SwitchAccessMenuBubbleController>();
  point_scan_controller_ = std::make_unique<PointScanController>();
  UpdateKeyCodesAfterSwitchAccessEnabled();
  UpdateSwitchAccessPointScanSpeedFromPref();
  if (skip_switch_access_notification_) {
    skip_switch_access_notification_ = false;
    return;
  }

  ShowAccessibilityNotification(
      A11yNotificationWrapper(A11yNotificationType::kSwitchAccessEnabled,
                              std::vector<std::u16string>()));
}

void AccessibilityController::DeactivateSwitchAccess() {
  if (client_) {
    client_->OnSwitchAccessDisabled();
  }
  point_scan_controller_.reset();
  switch_access_bubble_controller_.reset();
}

void AccessibilityController::SyncSwitchAccessPrefsToSignInProfile() {
  if (!active_user_prefs_ || IsSigninPrefService(active_user_prefs_)) {
    return;
  }

  PrefService* signin_prefs =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(signin_prefs);

  for (const auto* pref_path : kSwitchAccessPrefsCopiedToSignin) {
    const PrefService::Preference* pref =
        active_user_prefs_->FindPreference(pref_path);

    // Ignore if the pref has not been set by the user.
    if (!pref || !pref->IsUserControlled()) {
      continue;
    }

    // Copy the pref value to the signin profile.
    const base::Value* value = pref->GetValue();
    signin_prefs->Set(pref_path, *value);
  }
}

void AccessibilityController::UpdateShortcutsEnabledFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityShortcutsEnabled);

  if (shortcuts_enabled_ == enabled) {
    return;
  }

  shortcuts_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
}

void AccessibilityController::UpdateTabletModeShelfNavigationButtonsFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled);

  if (tablet_mode_shelf_navigation_buttons_enabled_ == enabled) {
    return;
  }

  tablet_mode_shelf_navigation_buttons_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
}

std::u16string AccessibilityController::GetBatteryDescription() const {
  // Pass battery status as string to callback function.
  return PowerStatus::Get()->GetAccessibleNameString(
      /*full_description=*/true);
}

void AccessibilityController::SetVirtualKeyboardVisible(bool is_visible) {
  if (is_visible) {
    Shell::Get()->keyboard_controller()->ShowKeyboard();
  } else {
    Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kUser);
  }

  if (set_virtual_keyboard_visible_callback_) {
    set_virtual_keyboard_visible_callback_.Run();
  }
}

void AccessibilityController::ToggleMouseKeys() {
  if (::features::IsAccessibilityMouseKeysEnabled() && mouse_keys().enabled()) {
    Shell::Get()->mouse_keys_controller()->Toggle();
  }
}

void AccessibilityController::PerformAccessibilityAction() {
  // TODO(b/335456364): Add UMA.
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  StatusAreaWidget* status_area_widget =
      RootWindowController::ForWindow(target_root)->GetStatusAreaWidget();
  if (!status_area_widget) {
    // TODO(b/335456364): Support Kiosk mode.
  }

  UnifiedSystemTray* tray = status_area_widget->unified_system_tray();
  if (tray->IsBubbleShown()) {
    if (tray->bubble()
            ->unified_system_tray_controller()
            ->showing_accessibility_detailed_view()) {
      tray->CloseBubble();
      return;
    }
  } else {
    tray->ShowBubble();
  }
  tray->bubble()
      ->unified_system_tray_controller()
      ->ShowAccessibilityDetailedView();
}

void AccessibilityController::PerformAcceleratorAction(
    AcceleratorAction accelerator_action) {
  AcceleratorController::Get()->PerformActionIfEnabled(accelerator_action,
                                                       /* accelerator = */ {});
}

void AccessibilityController::NotifyAccessibilityStatusChanged() {
  for (auto& observer : observers_) {
    observer.OnAccessibilityStatusChanged();
  }
}

bool AccessibilityController::IsAccessibilityFeatureVisibleInTrayMenu(
    const std::string& path) {
  if (!active_user_prefs_) {
    return true;
  }
  if (active_user_prefs_->FindPreference(path)->IsManaged() &&
      !active_user_prefs_->GetBoolean(path)) {
    return false;
  }
  return true;
}

void AccessibilityController::SuspendSwitchAccessKeyHandling(bool suspend) {
  accessibility_event_rewriter_->set_suspend_switch_access_key_handling(
      suspend);
}

void AccessibilityController::EnableChromeVoxVolumeSlideGesture() {
  enable_chromevox_volume_slide_gesture_ = true;
}

void AccessibilityController::ShowConfirmationDialog(
    const std::u16string& title,
    const std::u16string& description,
    const std::u16string& confirm_name,
    const std::u16string& cancel_name,
    base::OnceClosure on_accept_callback,
    base::OnceClosure on_cancel_callback,
    base::OnceClosure on_close_callback) {
  if (confirmation_dialog_) {
    // If a dialog is already being shown we do not show a new one.
    // Instead, run the on_close_callback on the new dialog to indicate
    // it was closed without the user taking any action.
    // This is consistent with AcceleratorController.
    std::move(on_close_callback).Run();
    return;
  }
  auto* dialog = new AccessibilityConfirmationDialog(
      title, description, confirm_name, cancel_name,
      std::move(on_accept_callback), std::move(on_cancel_callback),
      std::move(on_close_callback));
  // Save the dialog so it doesn't go out of scope before it is
  // used and closed.
  confirmation_dialog_ = dialog->GetWeakPtr();
  if (show_confirmation_dialog_callback_for_testing_) {
    show_confirmation_dialog_callback_for_testing_.Run();
  }
}

gfx::Rect AccessibilityController::GetConfirmationDialogBoundsInScreen() {
  if (!confirmation_dialog_.get()) {
    return gfx::Rect();
  }
  return confirmation_dialog_.get()->GetWidget()->GetWindowBoundsInScreen();
}

void AccessibilityController::PreviewFlashNotification() const {
  flash_screen_controller_->PreviewFlash();
}

void AccessibilityController::
    UpdateDictationButtonOnSpeechRecognitionDownloadChanged(
        int download_progress) {
  dictation_soda_download_progress_ = download_progress;
  Shell::Get()
      ->GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->dictation_button_tray()
      ->UpdateOnSpeechRecognitionDownloadChanged(download_progress);
}

void AccessibilityController::ShowNotificationForDictation(
    DictationNotificationType type,
    const std::u16string& display_language) {
  A11yNotificationType notification_type;
  switch (type) {
    case DictationNotificationType::kAllDlcsDownloaded:
      notification_type = A11yNotificationType::kDictationAllDlcsDownloaded;
      break;
    case DictationNotificationType::kNoDlcsDownloaded:
      notification_type = A11yNotificationType::kDictationNoDlcsDownloaded;
      break;
    case DictationNotificationType::kOnlySodaDownloaded:
      notification_type = A11yNotificationType::kDictationOnlySodaDownloaded;
      break;
    case DictationNotificationType::kOnlyPumpkinDownloaded:
      notification_type = A11yNotificationType::kDicationOnlyPumpkinDownloaded;
      break;
  }

  ShowAccessibilityNotification(A11yNotificationWrapper(
      notification_type, std::vector<std::u16string>{display_language}));
}

void AccessibilityController::ShowNotificationForFaceGaze(
    FaceGazeNotificationType type) {
  A11yNotificationType notification_type;
  std::string notification_shown_pref;
  switch (type) {
    case FaceGazeNotificationType::kDlcSucceeded:
      notification_type = A11yNotificationType::kFaceGazeAssetsDownloaded;
      notification_shown_pref =
          prefs::kFaceGazeDlcSuccessNotificationHasBeenShown;
      break;
    case FaceGazeNotificationType::kDlcFailed:
      notification_type = A11yNotificationType::kFaceGazeAssetsFailed;
      notification_shown_pref =
          prefs::kFaceGazeDlcFailureNotificationHasBeenShown;
      break;
  }

  if (active_user_prefs_->GetBoolean(notification_shown_pref)) {
    // Do not show notifications more than once.
    return;
  }

  active_user_prefs_->SetBoolean(notification_shown_pref, true);
  ShowAccessibilityNotification(A11yNotificationWrapper(
      notification_type, std::vector<std::u16string>()));
}

AccessibilityController::A11yNotificationWrapper::A11yNotificationWrapper() =
    default;
AccessibilityController::A11yNotificationWrapper::A11yNotificationWrapper(
    A11yNotificationType type_in,
    std::vector<std::u16string> replacements_in)
    : type(type_in), replacements(replacements_in) {}
AccessibilityController::A11yNotificationWrapper::A11yNotificationWrapper(
    A11yNotificationType type_in,
    std::vector<std::u16string> replacements_in,
    std::optional<base::RepeatingCallback<void(std::optional<int>)>>
        callback_in)
    : type(type_in),
      replacements(replacements_in),
      callback(std::move(callback_in)) {}
AccessibilityController::A11yNotificationWrapper::~A11yNotificationWrapper() =
    default;
AccessibilityController::A11yNotificationWrapper::A11yNotificationWrapper(
    const A11yNotificationWrapper&) = default;

void AccessibilityController::UpdateFeatureFromPref(FeatureType feature) {
  size_t feature_index = static_cast<size_t>(feature);
  bool enabled = features_[feature_index]->enabled();
  bool is_managed = active_user_prefs_->IsManagedPreference(
      features_[feature_index]->pref_name());

  switch (feature) {
    case FeatureType::kAutoclick:
      Shell::Get()->autoclick_controller()->SetEnabled(
          enabled, /*show_confirmation_dialog=*/
          !no_auto_click_confirmation_dialog_for_testing_ && !is_managed);
      break;
    case FeatureType::kCaretHighlight:
      UpdateAccessibilityHighlightingFromPrefs();
      break;
    case FeatureType::kCursorHighlight:
      UpdateAccessibilityHighlightingFromPrefs();
      break;
    case FeatureType::kDictation:
      if (enabled) {
        if (!dictation_bubble_controller_) {
          dictation_bubble_controller_ =
              std::make_unique<DictationBubbleController>();
        }
      } else {
        dictation_bubble_controller_.reset();
      }
      break;
    case FeatureType::kDisableTrackpad:
      if (!::features::IsAccessibilityDisableTrackpadEnabled() ||
          !disable_trackpad_event_rewriter_) {
        return;
      }

      disable_trackpad_event_rewriter_->SetEnabled(enabled);
      break;
    case FeatureType::kFloatingMenu:
      if (enabled && always_show_floating_menu_when_enabled_) {
        ShowFloatingMenuIfEnabled();
      } else {
        floating_menu_controller_.reset();
      }
      break;
    case FeatureType::kFocusHighlight:
      UpdateAccessibilityHighlightingFromPrefs();
      break;
    case FeatureType::kFullscreenMagnifier:
      break;
    case FeatureType::kDockedMagnifier:
      break;
    case FeatureType::kHighContrast:
      Shell::Get()->color_enhancement_controller()->SetHighContrastEnabled(
          enabled);
      break;
    case FeatureType::kLargeCursor:
      Shell::Get()->cursor_manager()->SetCursorSize(
          large_cursor().enabled() ? ui::CursorSize::kLarge
                                   : ui::CursorSize::kNormal);
      Shell::Get()->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
      Shell::Get()->UpdateCursorCompositingEnabled();
      break;
    case FeatureType::kLiveCaption:
      live_caption().SetEnabled(enabled);
      break;
    case FeatureType::kMonoAudio:
      CrasAudioHandler::Get()->SetOutputMonoEnabled(enabled);
      break;
    case FeatureType::kMouseKeys:
      if (::features::IsAccessibilityMouseKeysEnabled()) {
        // TODO(b/259372916): Consider creating/deleting MouseKeysController
        // here.
        Shell::Get()->mouse_keys_controller()->set_enabled(enabled);
      }
      break;
    case FeatureType::kSpokenFeedback:
      message_center::MessageCenter::Get()->SetSpokenFeedbackEnabled(enabled);
      // TODO(warx): ChromeVox loading/unloading requires browser process
      // started, thus it is still handled on Chrome side.

      // ChromeVox focus highlighting overrides the other focus highlighting.
      focus_highlight().UpdateFromPref();
      break;
    case FeatureType::kReducedAnimations:
      gfx::Animation::SetPrefersReducedMotionForA11y(
          reduced_animations().enabled());
      break;
    case FeatureType::kSelectToSpeak:
      select_to_speak_state_ = SelectToSpeakState::kSelectToSpeakStateInactive;
      if (enabled) {
        MaybeCreateSelectToSpeakEventHandler();
      } else {
        select_to_speak_event_handler_.reset();
        HideSelectToSpeakPanel();
        select_to_speak_bubble_controller_.reset();
      }
      break;
    case FeatureType::kStickyKeys:
      Shell::Get()->sticky_keys_controller()->Enable(enabled);
      break;
    case FeatureType::kSwitchAccess:
      if (!enabled) {
        if (no_switch_access_disable_confirmation_dialog_for_testing_) {
          SwitchAccessDisableDialogClosed(true);
        } else {
          // Show a dialog before disabling Switch Access.
          new AccessibilityFeatureDisableDialog(
              IDS_ASH_SWITCH_ACCESS_DISABLE_CONFIRMATION_TEXT,
              base::BindOnce(
                  &AccessibilityController::SwitchAccessDisableDialogClosed,
                  weak_ptr_factory_.GetWeakPtr(), true),
              base::BindOnce(
                  &AccessibilityController::SwitchAccessDisableDialogClosed,
                  weak_ptr_factory_.GetWeakPtr(), false));
          switch_access_disable_dialog_showing_ = true;
        }
        // Return early. We will call NotifyAccessibilityStatusChanged() if the
        // user accepts the dialog.
        return;
      } else {
        ActivateSwitchAccess();
      }
      SyncSwitchAccessPrefsToSignInProfile();
      break;
    case FeatureType::kVirtualKeyboard:
      keyboard::SetAccessibilityKeyboardEnabled(enabled);
      break;
    case FeatureType::kCursorColor:
      // The notification will already come via UpdateFeatureFromPref
      // so we don't need to run it twice.
      UpdateCursorColorFromPrefs(/*notify=*/false);
      break;
    case FeatureType::kColorCorrection:
      if (enabled && !active_user_prefs_->GetBoolean(
                         prefs::kAccessibilityColorCorrectionHasBeenSetup)) {
        Shell::Get()
            ->system_tray_model()
            ->client()
            ->ShowColorCorrectionSettings();
        active_user_prefs_->SetBoolean(
            prefs::kAccessibilityColorCorrectionHasBeenSetup, true);
      }
      UpdateColorCorrectionFromPrefs();
      break;
    case FeatureType::kFaceGaze:
      if (enabled && ::features::IsAccessibilityFaceGazeEnabled()) {
        if (!facegaze_bubble_controller_) {
          facegaze_bubble_controller_ =
              std::make_unique<FaceGazeBubbleController>();
        }
      } else {
        facegaze_bubble_controller_.reset();
      }
      UpdateFaceGazeFromPrefs();
      break;
    case FeatureType::kFlashNotifications:
      UpdateFlashNotificationsFromPrefs();
      break;
    case FeatureType::kFeatureCount:
    case FeatureType::kNoConflictingFeature:
      NOTREACHED();
  }
  NotifyAccessibilityStatusChanged();
}

void AccessibilityController::UpdateDictationBubble(
    bool visible,
    DictationBubbleIconType icon,
    const std::optional<std::u16string>& text,
    const std::optional<std::vector<DictationBubbleHintType>>& hints) {
  DCHECK(dictation().enabled());
  DCHECK(dictation_bubble_controller_);

  dictation_bubble_controller_->UpdateBubble(visible, icon, text, hints);
}

DictationBubbleController*
AccessibilityController::GetDictationBubbleControllerForTest() {
  if (!dictation_bubble_controller_) {
    dictation_bubble_controller_ =
        std::make_unique<DictationBubbleController>();
  }

  return dictation_bubble_controller_.get();
}

void AccessibilityController::ShowToast(AccessibilityToastType type) {
  accessibility_notification_controller_->ShowToast(type);
}

void AccessibilityController::AddShowToastCallbackForTesting(
    base::RepeatingCallback<void(AccessibilityToastType)> callback) {
  accessibility_notification_controller_->AddShowToastCallbackForTesting(
      std::move(callback));
}

void AccessibilityController::AddShowConfirmationDialogCallbackForTesting(
    base::RepeatingCallback<void()> callback) {
  show_confirmation_dialog_callback_for_testing_ = std::move(callback);
}

bool AccessibilityController::VerifyFeaturesDataForTesting() {
  return VerifyFeaturesData();
}

void AccessibilityController::SetVirtualKeyboardVisibleCallbackForTesting(
    base::RepeatingCallback<void()> callback) {
  set_virtual_keyboard_visible_callback_ = std::move(callback);
}

void AccessibilityController::ScrollAtPoint(
    const gfx::Point& target,
    AccessibilityScrollDirection direction) {
  float scroll_x = 0.0f;
  float scroll_y = 0.0f;
  switch (direction) {
    case AccessibilityScrollDirection::kUp:
      scroll_y = kScrollDelta;
      break;
    case AccessibilityScrollDirection::kDown:
      scroll_y = -kScrollDelta;
      break;
    case AccessibilityScrollDirection::kLeft:
      scroll_x = kScrollDelta;
      break;
    case AccessibilityScrollDirection::kRight:
      scroll_x = -kScrollDelta;
  }

  // Generate a scroll event at the target location.
  aura::Window* root_window = window_util::GetRootWindowAt(target);
  gfx::Point location_in_pixels(target);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);
  ui::ScrollEvent scroll(
      ui::EventType::kScroll, gfx::PointF(location_in_pixels),
      gfx::PointF(location_in_pixels), ui::EventTimeForNow(),
      ui::EF_IS_SYNTHESIZED, scroll_x, scroll_y, 0 /* x_offset_ordinal */,
      0 /* y_offset_ordinal */, 2 /* finger_count */);
  ui::MouseWheelEvent wheel(scroll);
  std::ignore = host->GetEventSink()->OnEventFromSource(&wheel);
}

void AccessibilityController::UpdateFaceGazeBubble(const std::u16string& text) {
  if (!facegaze_bubble_controller_ ||
      !::features::IsAccessibilityFaceGazeEnabled()) {
    return;
  }

  facegaze_bubble_controller_->UpdateBubble(text);
}

FaceGazeBubbleController*
AccessibilityController::GetFaceGazeBubbleControllerForTest() {
  if (!facegaze_bubble_controller_) {
    facegaze_bubble_controller_ = std::make_unique<FaceGazeBubbleController>();
  }

  return facegaze_bubble_controller_.get();
}

void AccessibilityController::ObserveInputDeviceSettings() {
  if (!input_device_settings_observer_.IsObservingSource(
          Shell::Get()->input_device_settings_controller())) {
    input_device_settings_observer_.Observe(
        Shell::Get()->input_device_settings_controller());
  }
}
}  // namespace ash
