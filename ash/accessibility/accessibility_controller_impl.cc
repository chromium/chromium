// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_highlight_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/accessibility_panel_layout_manager.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/events/select_to_speak_event_handler.h"
#include "ash/events/switch_access_event_handler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/policy/policy_recommendation_restorer.h"
#include "ash/public/cpp/accessibility_controller_client.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/wm/core/cursor_manager.h"

using session_manager::SessionState;

namespace ash {
namespace {

constexpr char kNotificationId[] = "chrome://settings/accessibility";
constexpr char kNotifierAccessibility[] = "ash.accessibility";

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
    prefs::kAccessibilityCursorHighlightEnabled,
    prefs::kAccessibilityDictationEnabled,
    prefs::kAccessibilityFocusHighlightEnabled,
    prefs::kAccessibilityHighContrastEnabled,
    prefs::kAccessibilityLargeCursorEnabled,
    prefs::kAccessibilityMonoAudioEnabled,
    prefs::kAccessibilityScreenMagnifierEnabled,
    prefs::kAccessibilityScreenMagnifierScale,
    prefs::kAccessibilitySelectToSpeakEnabled,
    prefs::kAccessibilitySpokenFeedbackEnabled,
    prefs::kAccessibilityStickyKeysEnabled,
    prefs::kAccessibilitySwitchAccessEnabled,
    prefs::kAccessibilityVirtualKeyboardEnabled,
    prefs::kDockedMagnifierEnabled,
    prefs::kDockedMagnifierScale,
    prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
    prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
    prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
    prefs::kDictationAcceleratorDialogHasBeenAccepted,
    prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2,
};

// Returns true if |pref_service| is the one used for the signin screen.
bool IsSigninPrefService(PrefService* pref_service) {
  const PrefService* signin_pref_service =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(signin_pref_service);
  return pref_service == signin_pref_service;
}

// Returns true if the current session is the guest session.
bool IsCurrentSessionGuest() {
  const base::Optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return user_type && *user_type == user_manager::USER_TYPE_GUEST;
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
  if (!ShouldCopySigninPrefs(previous_pref_service, current_pref_service))
    return;

  PrefService* signin_prefs =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(signin_prefs);
  for (const auto* pref_path : kCopiedOnSigninAccessibilityPrefs) {
    const PrefService::Preference* pref =
        signin_prefs->FindPreference(pref_path);

    // Ignore if the pref has not been set by the user.
    if (!pref || !pref->IsUserControlled())
      continue;

    // Copy the pref value from the signin profile.
    const base::Value* value_on_login = pref->GetValue();
    current_pref_service->Set(pref_path, *value_on_login);
  }
}

// Used to indicate which accessibility notification should be shown.
enum class A11yNotificationType {
  // No accessibility notification.
  kNone,
  // Shown when spoken feedback is set enabled with A11Y_NOTIFICATION_SHOW.
  kSpokenFeedbackEnabled,
  // Shown when braille display is connected while spoken feedback is enabled.
  kBrailleDisplayConnected,
  // Shown when braille display is connected while spoken feedback is not
  // enabled yet. Note: in this case braille display connected would enable
  // spoken feeback.
  kSpokenFeedbackBrailleEnabled,
};

// Returns notification icon based on the A11yNotificationType.
const gfx::VectorIcon& GetNotificationIcon(A11yNotificationType type) {
  if (type == A11yNotificationType::kSpokenFeedbackBrailleEnabled)
    return kNotificationAccessibilityIcon;
  if (type == A11yNotificationType::kBrailleDisplayConnected)
    return kNotificationAccessibilityBrailleIcon;
  return kNotificationChromevoxIcon;
}

void ShowAccessibilityNotification(A11yNotificationType type) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kNotificationId, false /* by_user */);

  if (type == A11yNotificationType::kNone)
    return;

  base::string16 text;
  base::string16 title;
  if (type == A11yNotificationType::kBrailleDisplayConnected) {
    text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BRAILLE_DISPLAY_CONNECTED);
  } else {
    bool is_tablet = Shell::Get()->tablet_mode_controller()->InTabletMode();

    title = l10n_util::GetStringUTF16(
        type == A11yNotificationType::kSpokenFeedbackBrailleEnabled
            ? IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_BRAILLE_ENABLED_TITLE
            : IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_TITLE);
    text = l10n_util::GetStringUTF16(
        is_tablet ? IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_TABLET
                  : IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED);
  }
  message_center::RichNotificationData options;
  options.should_make_spoken_feedback_for_popup_updates = false;
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title,
          text, base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierAccessibility),
          options, nullptr, GetNotificationIcon(type),
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);
  message_center->AddNotification(std::move(notification));
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
      return prefs::kAccessibilitySwitchAccessSelectKeyCodes;
    case SwitchAccessCommand::kNext:
      return prefs::kAccessibilitySwitchAccessNextKeyCodes;
    case SwitchAccessCommand::kPrevious:
      return prefs::kAccessibilitySwitchAccessPreviousKeyCodes;
    case SwitchAccessCommand::kNone:
      NOTREACHED();
      return "";
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
      return "";
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SwitchAccessCommandKeyCode {
  kUnknown = 0,
  kNone = 1,
  kSpace = 2,
  kEnter = 3,
  kMaxValue = kEnter,
};

SwitchAccessCommandKeyCode UmaValueForKeyCode(int key_code) {
  switch (key_code) {
    case 0:
      return SwitchAccessCommandKeyCode::kNone;
    case 13:
      return SwitchAccessCommandKeyCode::kEnter;
    case 32:
      return SwitchAccessCommandKeyCode::kSpace;
    default:
      return SwitchAccessCommandKeyCode::kUnknown;
  }
}

}  // namespace

AccessibilityControllerImpl::AccessibilityControllerImpl()
    : autoclick_delay_(AutoclickController::GetDefaultAutoclickDelay()) {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

AccessibilityControllerImpl::~AccessibilityControllerImpl() = default;

// static
void AccessibilityControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAutoclickEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
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
  registry->RegisterBooleanPref(
      prefs::kAccessibilityCaretHighlightEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityCursorHighlightEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityDictationEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityFocusHighlightEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityHighContrastEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityLargeCursorEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(prefs::kAccessibilityLargeCursorDipSize,
                                kDefaultLargeCursorSize);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityMonoAudioEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityScreenMagnifierCenterFocus, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityScreenMagnifierEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDoublePref(prefs::kAccessibilityScreenMagnifierScale,
                               std::numeric_limits<double>::min());
  registry->RegisterBooleanPref(prefs::kAccessibilitySpokenFeedbackEnabled,
                                false);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySelectToSpeakEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityStickyKeysEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySwitchAccessEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterListPref(
      prefs::kAccessibilitySwitchAccessSelectKeyCodes,
      base::Value(std::vector<base::Value>()),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilitySwitchAccessSelectSetting,
      kSwitchAccessAssignmentNone,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterListPref(
      prefs::kAccessibilitySwitchAccessNextKeyCodes,
      base::Value(std::vector<base::Value>()),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilitySwitchAccessNextSetting, kSwitchAccessAssignmentNone,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterListPref(
      prefs::kAccessibilitySwitchAccessPreviousKeyCodes,
      base::Value(std::vector<base::Value>()),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilitySwitchAccessPreviousSetting,
      kSwitchAccessAssignmentNone,
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
  registry->RegisterBooleanPref(
      prefs::kAccessibilityVirtualKeyboardEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kHighContrastAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
  registry->RegisterBooleanPref(
      prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2, false);

  registry->RegisterBooleanPref(
      prefs::kShouldAlwaysShowAccessibilityMenu, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void AccessibilityControllerImpl::Shutdown() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);

  for (auto& observer : observers_)
    observer.OnAccessibilityControllerShutdown();
}

void AccessibilityControllerImpl::SetHighContrastAcceleratorDialogAccepted() {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(
      prefs::kHighContrastAcceleratorDialogHasBeenAccepted, true);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::HasHighContrastAcceleratorDialogBeenAccepted()
    const {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(
             prefs::kHighContrastAcceleratorDialogHasBeenAccepted);
}

void AccessibilityControllerImpl::
    SetScreenMagnifierAcceleratorDialogAccepted() {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(
      prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted, true);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::
    HasScreenMagnifierAcceleratorDialogBeenAccepted() const {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(
             prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted);
}

void AccessibilityControllerImpl::
    SetDockedMagnifierAcceleratorDialogAccepted() {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(
      prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted, true);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::
    HasDisplayRotationAcceleratorDialogBeenAccepted() const {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(
             prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2);
}

void AccessibilityControllerImpl::
    SetDisplayRotationAcceleratorDialogBeenAccepted() {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(
      prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2, true);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::
    HasDockedMagnifierAcceleratorDialogBeenAccepted() const {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(
             prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted);
}

void AccessibilityControllerImpl::SetDictationAcceleratorDialogAccepted() {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(
      prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::HasDictationAcceleratorDialogBeenAccepted()
    const {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(
             prefs::kDictationAcceleratorDialogHasBeenAccepted);
}

void AccessibilityControllerImpl::AddObserver(AccessibilityObserver* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityControllerImpl::RemoveObserver(
    AccessibilityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AccessibilityControllerImpl::SetAutoclickEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityAutoclickEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsAutoclickSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityAutoclickEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForAutoclick() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityAutoclickEnabled);
}

bool AccessibilityControllerImpl::IsPrimarySettingsViewVisibleInTray() {
  return (IsSpokenFeedbackSettingVisibleInTray() ||
          IsSelectToSpeakSettingVisibleInTray() ||
          IsDictationSettingVisibleInTray() ||
          IsHighContrastSettingVisibleInTray() ||
          IsFullScreenMagnifierSettingVisibleInTray() ||
          IsDockedMagnifierSettingVisibleInTray() ||
          IsAutoclickSettingVisibleInTray() ||
          IsVirtualKeyboardSettingVisibleInTray() ||
          (base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kEnableExperimentalAccessibilitySwitchAccess) &&
           IsSwitchAccessSettingVisibleInTray()));
}

bool AccessibilityControllerImpl::IsAdditionalSettingsViewVisibleInTray() {
  return (IsLargeCursorSettingVisibleInTray() ||
          IsMonoAudioSettingVisibleInTray() ||
          IsCaretHighlightSettingVisibleInTray() ||
          IsCursorHighlightSettingVisibleInTray() ||
          IsFocusHighlightSettingVisibleInTray() ||
          IsStickyKeysSettingVisibleInTray());
}

bool AccessibilityControllerImpl::IsAdditionalSettingsSeparatorVisibleInTray() {
  return IsPrimarySettingsViewVisibleInTray() &&
         IsAdditionalSettingsViewVisibleInTray();
}

void AccessibilityControllerImpl::SetCaretHighlightEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsCaretHighlightSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityCaretHighlightEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForCaretHighlight() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityCaretHighlightEnabled);
}

void AccessibilityControllerImpl::SetCursorHighlightEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityCursorHighlightEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsCursorHighlightSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityCursorHighlightEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForCursorHighlight() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityCursorHighlightEnabled);
}

void AccessibilityControllerImpl::SetDictationEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;

  const bool dialog_ever_accepted = HasDictationAcceleratorDialogBeenAccepted();

  if (enabled && !dialog_ever_accepted) {
    Shell::Get()->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_DICTATION_CONFIRMATION_TITLE,
        IDS_ASH_DICTATION_CONFIRMATION_BODY,
        // Callback for if the user accepts the dialog
        base::BindOnce([]() {
          AccessibilityControllerImpl* controller =
              Shell::Get()->accessibility_controller();
          controller->SetDictationAcceleratorDialogAccepted();
          // If they accept, try again to set dictation_enabled to true
          controller->SetDictationEnabled(true);
        }),
        base::DoNothing());
    return;
  }
  active_user_prefs_->SetBoolean(prefs::kAccessibilityDictationEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsDictationSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityDictationEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForDictation() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityDictationEnabled);
}

void AccessibilityControllerImpl::SetFocusHighlightEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsFocusHighlightSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityFocusHighlightEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForFocusHighlight() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityFocusHighlightEnabled);
}

void AccessibilityControllerImpl::SetFullscreenMagnifierEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsFullscreenMagnifierEnabledForTesting() {
  return active_user_prefs_ && active_user_prefs_->GetBoolean(
                                   prefs::kAccessibilityScreenMagnifierEnabled);
}

bool AccessibilityControllerImpl::IsFullScreenMagnifierSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityScreenMagnifierEnabled);
}

bool AccessibilityControllerImpl::
    IsEnterpriseIconVisibleForFullScreenMagnifier() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityScreenMagnifierEnabled);
}

void AccessibilityControllerImpl::SetDockedMagnifierEnabledForTesting(
    bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kDockedMagnifierEnabled, enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsDockedMagnifierEnabledForTesting() {
  return active_user_prefs_ &&
         active_user_prefs_->GetBoolean(prefs::kDockedMagnifierEnabled);
}

bool AccessibilityControllerImpl::IsDockedMagnifierSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kDockedMagnifierEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForDockedMagnifier() {
  return IsEnterpriseIconVisibleInTrayMenu(prefs::kDockedMagnifierEnabled);
}

void AccessibilityControllerImpl::SetHighContrastEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityHighContrastEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsHighContrastSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityHighContrastEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForHighContrast() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityHighContrastEnabled);
}

void AccessibilityControllerImpl::SetLargeCursorEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityLargeCursorEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsLargeCursorSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityLargeCursorEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForLargeCursor() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityLargeCursorEnabled);
}

void AccessibilityControllerImpl::SetMonoAudioEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityMonoAudioEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsMonoAudioSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityMonoAudioEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForMonoAudio() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityMonoAudioEnabled);
}

void AccessibilityControllerImpl::SetSpokenFeedbackEnabled(
    bool enabled,
    AccessibilityNotificationVisibility notify) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();

  // Value could be left unchanged because of higher-priority pref source, eg.
  // policy. See crbug.com/953245.
  const bool actual_enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  A11yNotificationType type = A11yNotificationType::kNone;
  if (enabled && actual_enabled && notify == A11Y_NOTIFICATION_SHOW)
    type = A11yNotificationType::kSpokenFeedbackEnabled;
  ShowAccessibilityNotification(type);
}

bool AccessibilityControllerImpl::IsSpokenFeedbackSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilitySpokenFeedbackEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForSpokenFeedback() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilitySpokenFeedbackEnabled);
}

void AccessibilityControllerImpl::SetSelectToSpeakEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilitySelectToSpeakEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsSelectToSpeakSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilitySelectToSpeakEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForSelectToSpeak() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilitySelectToSpeakEnabled);
}

void AccessibilityControllerImpl::RequestSelectToSpeakStateChange() {
  client_->RequestSelectToSpeakStateChange();
}

void AccessibilityControllerImpl::SetSelectToSpeakState(
    SelectToSpeakState state) {
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

void AccessibilityControllerImpl::SetSelectToSpeakEventHandlerDelegate(
    SelectToSpeakEventHandlerDelegate* delegate) {
  select_to_speak_event_handler_delegate_ = delegate;
  MaybeCreateSelectToSpeakEventHandler();
}

SelectToSpeakState AccessibilityControllerImpl::GetSelectToSpeakState() const {
  return select_to_speak_state_;
}

void AccessibilityControllerImpl::SetSwitchAccessEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilitySwitchAccessEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsSwitchAccessSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilitySwitchAccessEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForSwitchAccess() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilitySwitchAccessEnabled);
}

void AccessibilityControllerImpl::
    SetSwitchAccessIgnoreVirtualKeyEventForTesting(bool should_ignore) {
  switch_access_event_handler_->set_ignore_virtual_key_events(should_ignore);
}

void AccessibilityControllerImpl::ForwardKeyEventsToSwitchAccess(
    bool should_forward) {
  switch_access_event_handler_->set_forward_key_events(should_forward);
}

void AccessibilityControllerImpl::SetSwitchAccessEventHandlerDelegate(
    SwitchAccessEventHandlerDelegate* delegate) {
  switch_access_event_handler_delegate_ = delegate;
  MaybeCreateSwitchAccessEventHandler();
}

void AccessibilityControllerImpl::SetStickyKeysEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityStickyKeysEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsStickyKeysSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityStickyKeysEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForStickyKeys() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityStickyKeysEnabled);
}

void AccessibilityControllerImpl::SetVirtualKeyboardEnabled(bool enabled) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                                 enabled);
  active_user_prefs_->CommitPendingWrite();
}

bool AccessibilityControllerImpl::IsVirtualKeyboardSettingVisibleInTray() {
  return IsAccessibilityFeatureVisibleInTrayMenu(
      prefs::kAccessibilityVirtualKeyboardEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForVirtualKeyboard() {
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilityVirtualKeyboardEnabled);
}

void AccessibilityControllerImpl::TriggerAccessibilityAlert(
    AccessibilityAlert alert) {
  if (client_)
    client_->TriggerAccessibilityAlert(alert);
}

void AccessibilityControllerImpl::TriggerAccessibilityAlertWithMessage(
    const std::string& message) {
  if (client_)
    client_->TriggerAccessibilityAlertWithMessage(message);
}

void AccessibilityControllerImpl::PlayEarcon(int32_t sound_key) {
  if (client_)
    client_->PlayEarcon(sound_key);
}

base::TimeDelta AccessibilityControllerImpl::PlayShutdownSound() {
  return client_ ? client_->PlayShutdownSound() : base::TimeDelta();
}

void AccessibilityControllerImpl::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  if (client_)
    client_->HandleAccessibilityGesture(gesture);
}

void AccessibilityControllerImpl::ToggleDictation() {
  // Do nothing if dictation is not enabled.
  if (!dictation_enabled())
    return;

  if (client_) {
    const bool is_active = client_->ToggleDictation();
    SetDictationActive(is_active);
    if (is_active)
      Shell::Get()->OnDictationStarted();
    else
      Shell::Get()->OnDictationEnded();
  }
}

void AccessibilityControllerImpl::SetDictationActive(bool is_active) {
  dictation_active_ = is_active;
}

void AccessibilityControllerImpl::ToggleDictationFromSource(
    DictationToggleSource source) {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Dictation"));
  UserMetricsRecorder::RecordUserToggleDictation(source);

  SetDictationEnabled(true);
  ToggleDictation();
}

void AccessibilityControllerImpl::SilenceSpokenFeedback() {
  if (client_)
    client_->SilenceSpokenFeedback();
}

void AccessibilityControllerImpl::OnTwoFingerTouchStart() {
  if (client_)
    client_->OnTwoFingerTouchStart();
}

void AccessibilityControllerImpl::OnTwoFingerTouchStop() {
  if (client_)
    client_->OnTwoFingerTouchStop();
}

bool AccessibilityControllerImpl::ShouldToggleSpokenFeedbackViaTouch() const {
  return client_ && client_->ShouldToggleSpokenFeedbackViaTouch();
}

void AccessibilityControllerImpl::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  if (client_)
    client_->PlaySpokenFeedbackToggleCountdown(tick_count);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleInTrayMenu(
    const std::string& path) {
  return active_user_prefs_ &&
         active_user_prefs_->FindPreference(path)->IsManaged();
}

void AccessibilityControllerImpl::SetClient(
    AccessibilityControllerClient* client) {
  client_ = client;
}

void AccessibilityControllerImpl::SetDarkenScreen(bool darken) {
  if (darken && !scoped_backlights_forced_off_) {
    scoped_backlights_forced_off_ =
        Shell::Get()->backlights_forced_off_setter()->ForceBacklightsOff();
  } else if (!darken && scoped_backlights_forced_off_) {
    scoped_backlights_forced_off_.reset();
  }
}

void AccessibilityControllerImpl::BrailleDisplayStateChanged(bool connected) {
  A11yNotificationType type = A11yNotificationType::kNone;
  if (connected && spoken_feedback_enabled_)
    type = A11yNotificationType::kBrailleDisplayConnected;
  else if (connected && !spoken_feedback_enabled_)
    type = A11yNotificationType::kSpokenFeedbackBrailleEnabled;

  if (connected)
    SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  NotifyAccessibilityStatusChanged();

  ShowAccessibilityNotification(type);
}

void AccessibilityControllerImpl::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {
  if (!accessibility_highlight_controller_)
    return;
  accessibility_highlight_controller_->SetFocusHighlightRect(bounds_in_screen);
}

void AccessibilityControllerImpl::SetCaretBounds(
    const gfx::Rect& bounds_in_screen) {
  if (!accessibility_highlight_controller_)
    return;
  accessibility_highlight_controller_->SetCaretBounds(bounds_in_screen);
}

void AccessibilityControllerImpl::SetAccessibilityPanelAlwaysVisible(
    bool always_visible) {
  GetLayoutManager()->SetAlwaysVisible(always_visible);
}

void AccessibilityControllerImpl::SetAccessibilityPanelBounds(
    const gfx::Rect& bounds,
    AccessibilityPanelState state) {
  GetLayoutManager()->SetPanelBounds(bounds, state);
}

void AccessibilityControllerImpl::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  // Make |kA11yPrefsForRecommendedValueOnSignin| observing recommended values
  // on signin screen. See PolicyRecommendationRestorer.
  PolicyRecommendationRestorer* policy_recommendation_restorer =
      Shell::Get()->policy_recommendation_restorer();
  for (auto* const pref_name : kA11yPrefsForRecommendedValueOnSignin)
    policy_recommendation_restorer->ObservePref(pref_name);

  // Observe user settings. This must happen after PolicyRecommendationRestorer.
  ObservePrefs(prefs);
}

void AccessibilityControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  // This is guaranteed to be received after
  // OnSigninScreenPrefServiceInitialized() so only copy the signin prefs if
  // needed here.
  CopySigninPrefsIfNeeded(active_user_prefs_, prefs);
  ObservePrefs(prefs);
}

SwitchAccessEventHandler*
AccessibilityControllerImpl::GetSwitchAccessEventHandlerForTest() {
  if (switch_access_event_handler_)
    return switch_access_event_handler_.get();
  return nullptr;
}

void AccessibilityControllerImpl::OnTabletModeStarted() {
  if (spoken_feedback_enabled())
    ShowAccessibilityNotification(A11yNotificationType::kSpokenFeedbackEnabled);
}

void AccessibilityControllerImpl::OnTabletModeEnded() {
  if (spoken_feedback_enabled())
    ShowAccessibilityNotification(A11yNotificationType::kSpokenFeedbackEnabled);
}

void AccessibilityControllerImpl::ObservePrefs(PrefService* prefs) {
  DCHECK(prefs);

  active_user_prefs_ = prefs;

  // Watch for pref updates from webui settings and policy.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickEnabled,
      base::BindRepeating(&AccessibilityControllerImpl::UpdateAutoclickFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickDelayMs,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateAutoclickDelayFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickEventType,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateAutoclickEventTypeFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickRevertToLeftClick,
      base::BindRepeating(&AccessibilityControllerImpl::
                              UpdateAutoclickRevertToLeftClickFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickStabilizePosition,
      base::BindRepeating(&AccessibilityControllerImpl::
                              UpdateAutoclickStabilizePositionFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickMovementThreshold,
      base::BindRepeating(&AccessibilityControllerImpl::
                              UpdateAutoclickMovementThresholdFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickMenuPosition,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateAutoclickMenuPositionFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityCaretHighlightEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateCaretHighlightFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityCursorHighlightEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateCursorHighlightFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityDictationEnabled,
      base::BindRepeating(&AccessibilityControllerImpl::UpdateDictationFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityFocusHighlightEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateFocusHighlightFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityHighContrastEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateHighContrastFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateLargeCursorFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorDipSize,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateLargeCursorFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityMonoAudioEnabled,
      base::BindRepeating(&AccessibilityControllerImpl::UpdateMonoAudioFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSpokenFeedbackFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySelectToSpeakEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSelectToSpeakFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityStickyKeysEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateStickyKeysFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSwitchAccessFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessSelectKeyCodes,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSwitchAccessKeyCodesFromPref,
          base::Unretained(this), SwitchAccessCommand::kSelect));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessNextKeyCodes,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSwitchAccessKeyCodesFromPref,
          base::Unretained(this), SwitchAccessCommand::kNext));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessPreviousKeyCodes,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSwitchAccessKeyCodesFromPref,
          base::Unretained(this), SwitchAccessCommand::kPrevious));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled,
      base::BindRepeating(&AccessibilityControllerImpl::
                              UpdateSwitchAccessAutoScanEnabledFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateSwitchAccessAutoScanSpeedFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
      base::BindRepeating(&AccessibilityControllerImpl::
                              UpdateSwitchAccessAutoScanKeyboardSpeedFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateVirtualKeyboardFromPref,
          base::Unretained(this)));

  // Load current state.
  UpdateAutoclickFromPref();
  UpdateAutoclickDelayFromPref();
  UpdateAutoclickEventTypeFromPref();
  UpdateAutoclickRevertToLeftClickFromPref();
  UpdateAutoclickStabilizePositionFromPref();
  UpdateAutoclickMovementThresholdFromPref();
  UpdateAutoclickMenuPositionFromPref();
  UpdateCaretHighlightFromPref();
  UpdateCursorHighlightFromPref();
  UpdateDictationFromPref();
  UpdateFocusHighlightFromPref();
  UpdateHighContrastFromPref();
  UpdateLargeCursorFromPref();
  UpdateMonoAudioFromPref();
  UpdateSpokenFeedbackFromPref();
  UpdateSelectToSpeakFromPref();
  UpdateStickyKeysFromPref();
  UpdateSwitchAccessFromPref();
  UpdateVirtualKeyboardFromPref();
}

void AccessibilityControllerImpl::UpdateAutoclickFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityAutoclickEnabled);

  if (autoclick_enabled_ == enabled)
    return;

  autoclick_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  Shell::Get()->autoclick_controller()->SetEnabled(
      enabled, true /* show confirmation dialog */);
}

void AccessibilityControllerImpl::UpdateAutoclickDelayFromPref() {
  DCHECK(active_user_prefs_);
  base::TimeDelta autoclick_delay = base::TimeDelta::FromMilliseconds(int64_t{
      active_user_prefs_->GetInteger(prefs::kAccessibilityAutoclickDelayMs)});

  if (autoclick_delay_ == autoclick_delay)
    return;
  autoclick_delay_ = autoclick_delay;

  Shell::Get()->autoclick_controller()->SetAutoclickDelay(autoclick_delay_);
}

void AccessibilityControllerImpl::UpdateAutoclickEventTypeFromPref() {
  Shell::Get()->autoclick_controller()->SetAutoclickEventType(
      GetAutoclickEventType());
}

void AccessibilityControllerImpl::SetAutoclickEventType(
    AutoclickEventType event_type) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetInteger(prefs::kAccessibilityAutoclickEventType,
                                 static_cast<int>(event_type));
  active_user_prefs_->CommitPendingWrite();
  Shell::Get()->autoclick_controller()->SetAutoclickEventType(event_type);
}

AutoclickEventType AccessibilityControllerImpl::GetAutoclickEventType() {
  DCHECK(active_user_prefs_);
  return static_cast<AutoclickEventType>(
      active_user_prefs_->GetInteger(prefs::kAccessibilityAutoclickEventType));
}

void AccessibilityControllerImpl::UpdateAutoclickRevertToLeftClickFromPref() {
  DCHECK(active_user_prefs_);
  bool revert_to_left_click = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityAutoclickRevertToLeftClick);

  Shell::Get()->autoclick_controller()->set_revert_to_left_click(
      revert_to_left_click);
}

void AccessibilityControllerImpl::UpdateAutoclickStabilizePositionFromPref() {
  DCHECK(active_user_prefs_);
  bool stabilize_position = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityAutoclickStabilizePosition);

  Shell::Get()->autoclick_controller()->set_stabilize_click_position(
      stabilize_position);
}

void AccessibilityControllerImpl::UpdateAutoclickMovementThresholdFromPref() {
  DCHECK(active_user_prefs_);
  int movement_threshold = active_user_prefs_->GetInteger(
      prefs::kAccessibilityAutoclickMovementThreshold);

  Shell::Get()->autoclick_controller()->SetMovementThreshold(
      movement_threshold);
}

void AccessibilityControllerImpl::UpdateAutoclickMenuPositionFromPref() {
  Shell::Get()->autoclick_controller()->SetMenuPosition(
      GetAutoclickMenuPosition());
}

void AccessibilityControllerImpl::SetAutoclickMenuPosition(
    AutoclickMenuPosition position) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetInteger(prefs::kAccessibilityAutoclickMenuPosition,
                                 static_cast<int>(position));
  active_user_prefs_->CommitPendingWrite();
  Shell::Get()->autoclick_controller()->SetMenuPosition(position);
}

AutoclickMenuPosition AccessibilityControllerImpl::GetAutoclickMenuPosition() {
  DCHECK(active_user_prefs_);
  return static_cast<AutoclickMenuPosition>(active_user_prefs_->GetInteger(
      prefs::kAccessibilityAutoclickMenuPosition));
}

void AccessibilityControllerImpl::RequestAutoclickScrollableBoundsForPoint(
    gfx::Point& point_in_screen) {
  if (client_)
    client_->RequestAutoclickScrollableBoundsForPoint(point_in_screen);
}

void AccessibilityControllerImpl::UpdateAutoclickMenuBoundsIfNeeded() {
  Shell::Get()->autoclick_controller()->UpdateAutoclickMenuBoundsIfNeeded();
}

void AccessibilityControllerImpl::OnAutoclickScrollableBoundsFound(
    gfx::Rect& bounds_in_screen) {
  Shell::Get()->autoclick_controller()->OnAutoclickScrollableBoundsFound(
      bounds_in_screen);
}

void AccessibilityControllerImpl::UpdateCaretHighlightFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityCaretHighlightEnabled);

  if (caret_highlight_enabled_ == enabled)
    return;

  caret_highlight_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityControllerImpl::UpdateCursorHighlightFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityCursorHighlightEnabled);

  if (cursor_highlight_enabled_ == enabled)
    return;

  cursor_highlight_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityControllerImpl::UpdateDictationFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityDictationEnabled);

  if (dictation_enabled_ == enabled)
    return;

  dictation_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
}

void AccessibilityControllerImpl::UpdateFocusHighlightFromPref() {
  DCHECK(active_user_prefs_);
  bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityFocusHighlightEnabled);

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (spoken_feedback_enabled_)
    enabled = false;

  if (focus_highlight_enabled_ == enabled)
    return;

  focus_highlight_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityControllerImpl::UpdateHighContrastFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityHighContrastEnabled);

  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  Shell::Get()->high_contrast_controller()->SetEnabled(enabled);
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void AccessibilityControllerImpl::UpdateLargeCursorFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityLargeCursorEnabled);
  // Reset large cursor size to the default size when large cursor is disabled.
  if (!enabled)
    active_user_prefs_->ClearPref(prefs::kAccessibilityLargeCursorDipSize);
  const int size =
      active_user_prefs_->GetInteger(prefs::kAccessibilityLargeCursorDipSize);

  if (large_cursor_enabled_ == enabled && large_cursor_size_in_dip_ == size)
    return;

  large_cursor_enabled_ = enabled;
  large_cursor_size_in_dip_ = size;

  NotifyAccessibilityStatusChanged();

  Shell::Get()->cursor_manager()->SetCursorSize(
      large_cursor_enabled_ ? ui::CursorSize::kLarge : ui::CursorSize::kNormal);
  Shell::Get()->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void AccessibilityControllerImpl::UpdateMonoAudioFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityMonoAudioEnabled);

  if (mono_audio_enabled_ == enabled)
    return;

  mono_audio_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  chromeos::CrasAudioHandler::Get()->SetOutputMonoEnabled(enabled);
}

void AccessibilityControllerImpl::UpdateSpokenFeedbackFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  // TODO(warx): ChromeVox loading/unloading requires browser process started,
  // thus it is still handled on Chrome side.

  // ChromeVox focus highlighting overrides the other focus highlighting.
  UpdateFocusHighlightFromPref();
}

void AccessibilityControllerImpl::UpdateAccessibilityHighlightingFromPrefs() {
  if (!caret_highlight_enabled_ && !cursor_highlight_enabled_ &&
      !focus_highlight_enabled_) {
    accessibility_highlight_controller_.reset();
    return;
  }

  if (!accessibility_highlight_controller_) {
    accessibility_highlight_controller_ =
        std::make_unique<AccessibilityHighlightController>();
  }

  accessibility_highlight_controller_->HighlightCaret(caret_highlight_enabled_);
  accessibility_highlight_controller_->HighlightCursor(
      cursor_highlight_enabled_);
  accessibility_highlight_controller_->HighlightFocus(focus_highlight_enabled_);
}

void AccessibilityControllerImpl::UpdateSelectToSpeakFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilitySelectToSpeakEnabled);

  if (select_to_speak_enabled_ == enabled)
    return;

  select_to_speak_enabled_ = enabled;
  select_to_speak_state_ = SelectToSpeakState::kSelectToSpeakStateInactive;

  if (enabled)
    MaybeCreateSelectToSpeakEventHandler();
  else
    select_to_speak_event_handler_.reset();

  NotifyAccessibilityStatusChanged();
}

void AccessibilityControllerImpl::MaybeCreateSelectToSpeakEventHandler() {
  // Construct the handler as needed when Select-to-Speak is enabled and the
  // delegate is set. Otherwise, destroy the handler when Select-to-Speak is
  // disabled or the delegate has been destroyed.
  if (!select_to_speak_enabled_ || !select_to_speak_event_handler_delegate_) {
    select_to_speak_event_handler_.reset();
    return;
  }

  if (select_to_speak_event_handler_)
    return;

  select_to_speak_event_handler_ = std::make_unique<SelectToSpeakEventHandler>(
      select_to_speak_event_handler_delegate_);
}

void AccessibilityControllerImpl::UpdateStickyKeysFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityStickyKeysEnabled);

  if (sticky_keys_enabled_ == enabled)
    return;

  sticky_keys_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  Shell::Get()->sticky_keys_controller()->Enable(enabled);
}

void AccessibilityControllerImpl::UpdateSwitchAccessFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilitySwitchAccessEnabled);
  if (switch_access_enabled_ == enabled)
    return;

  switch_access_enabled_ = enabled;

  if (enabled)
    MaybeCreateSwitchAccessEventHandler();
  else
    switch_access_event_handler_.reset();

  NotifyAccessibilityStatusChanged();
}

void AccessibilityControllerImpl::UpdateSwitchAccessKeyCodesFromPref(
    SwitchAccessCommand command) {
  DCHECK(active_user_prefs_);

  std::string pref_key = PrefKeyForSwitchAccessCommand(command);
  const base::ListValue* key_codes_pref = active_user_prefs_->GetList(pref_key);
  std::set<int> key_codes;
  for (const base::Value& v : *key_codes_pref) {
    int key_code = v.GetInt();
    key_codes.insert(key_code);
  }

  std::string uma_name = UmaNameForSwitchAccessCommand(command);
  if (key_codes.size() == 0) {
    SwitchAccessCommandKeyCode uma_value = UmaValueForKeyCode(0);
    base::UmaHistogramEnumeration(uma_name, uma_value);
  }
  for (int key_code : key_codes) {
    SwitchAccessCommandKeyCode uma_value = UmaValueForKeyCode(key_code);
    base::UmaHistogramEnumeration(uma_name, uma_value);
  }

  if (!switch_access_event_handler_)
    return;

  switch_access_event_handler_->SetKeyCodesForCommand(key_codes, command);
}

void AccessibilityControllerImpl::UpdateSwitchAccessAutoScanEnabledFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled);

  base::UmaHistogramBoolean("Accessibility.CrosSwitchAccess.AutoScan", enabled);
}

void AccessibilityControllerImpl::UpdateSwitchAccessAutoScanSpeedFromPref() {
  DCHECK(active_user_prefs_);
  const int speed_ms = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessAutoScanSpeedMs);

  base::UmaHistogramCustomCounts(
      "Accessibility.CrosSwitchAccess.AutoScan.SpeedMs", speed_ms, 1 /* min */,
      10000 /* max */, 100 /* buckets */);
}

void AccessibilityControllerImpl::
    UpdateSwitchAccessAutoScanKeyboardSpeedFromPref() {
  DCHECK(active_user_prefs_);
  const int speed_ms = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs);

  base::UmaHistogramCustomCounts(
      "Accessibility.CrosSwitchAccess.AutoScan.KeyboardSpeedMs", speed_ms,
      1 /* min */, 10000 /* max */, 100 /* buckets */);
}

void AccessibilityControllerImpl::MaybeCreateSwitchAccessEventHandler() {
  // Construct the handler as needed when Switch Access is enabled and the
  // delegate is set. Otherwise, destroy the handler when Switch Access is
  // disabled or the delegate has been destroyed.
  if (!switch_access_enabled_ || !switch_access_event_handler_delegate_) {
    switch_access_event_handler_.reset();
    return;
  }

  if (switch_access_event_handler_)
    return;

  switch_access_event_handler_ = std::make_unique<SwitchAccessEventHandler>(
      switch_access_event_handler_delegate_);

  if (!active_user_prefs_)
    return;

  // Update the key codes for each command once the handler is initialized.
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kSelect);
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kNext);
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kPrevious);
}

void AccessibilityControllerImpl::UpdateVirtualKeyboardFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityVirtualKeyboardEnabled);

  if (virtual_keyboard_enabled_ == enabled)
    return;

  virtual_keyboard_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  keyboard::SetAccessibilityKeyboardEnabled(enabled);
}

base::string16 AccessibilityControllerImpl::GetBatteryDescription() const {
  // Pass battery status as string to callback function.
  return PowerStatus::Get()->GetAccessibleNameString(
      /*full_description=*/true);
}

void AccessibilityControllerImpl::SetVirtualKeyboardVisible(bool is_visible) {
  if (is_visible)
    Shell::Get()->keyboard_controller()->ShowKeyboard();
  else
    Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kUser);
}

void AccessibilityControllerImpl::NotifyAccessibilityStatusChanged() {
  for (auto& observer : observers_)
    observer.OnAccessibilityStatusChanged();
}

bool AccessibilityControllerImpl::IsAccessibilityFeatureVisibleInTrayMenu(
    const std::string& path) {
  if (!active_user_prefs_)
    return true;
  if (active_user_prefs_->FindPreference(path)->IsManaged() &&
      !active_user_prefs_->GetBoolean(path)) {
    return false;
  }
  return true;
}

}  // namespace ash
