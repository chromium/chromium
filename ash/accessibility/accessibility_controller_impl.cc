// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_highlight_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/accessibility_panel_layout_manager.h"
#include "ash/accessibility/point_scan_controller.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/events/accessibility_event_rewriter.h"
#include "ash/events/select_to_speak_event_handler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/login_status.h"
#include "ash/policy/policy_recommendation_restorer.h"
#include "ash/public/cpp/accessibility_controller_client.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/accessibility_feature_disable_dialog.h"
#include "ash/system/accessibility/floating_accessibility_controller.h"
#include "ash/system/accessibility/switch_access_menu_bubble_controller.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
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

using FeatureType = AccessibilityControllerImpl::FeatureType;

// These classes are used to store the static configuration for a11y features.
struct FeatureData {
  FeatureType type;
  const char* pref;
  const gfx::VectorIcon* icon;
  FeatureType conflicting_feature = FeatureType::kNoConflictingFeature;
};

struct FeatureDialogData {
  AccessibilityControllerImpl::FeatureType type;
  const char* pref;
  int title;
  int body;
  bool mandatory;
};

// A static array describing each feature.
const FeatureData kFeatures[] = {
    {FeatureType::kAutoclick, prefs::kAccessibilityAutoclickEnabled,
     &kSystemMenuAccessibilityAutoClickIcon},
    {FeatureType::kCaretHighlight, prefs::kAccessibilityCaretHighlightEnabled,
     nullptr},
    {FeatureType::KCursorHighlight, prefs::kAccessibilityCursorHighlightEnabled,
     nullptr},
    {FeatureType::kCursorColor, prefs::kAccessibilityCursorColorEnabled,
     nullptr},
    {FeatureType::kDictation, prefs::kAccessibilityDictationEnabled,
     &kDictationMenuIcon},
    {FeatureType::kFocusHighlight, prefs::kAccessibilityFocusHighlightEnabled,
     nullptr, /* conflicting_feature= */ FeatureType::kSpokenFeedback},
    {FeatureType::kFloatingMenu, prefs::kAccessibilityFloatingMenuEnabled,
     nullptr},
    {FeatureType::kFullscreenMagnifier,
     prefs::kAccessibilityScreenMagnifierEnabled,
     &kSystemMenuAccessibilityFullscreenMagnifierIcon},
    {FeatureType::kDockedMagnifier, prefs::kDockedMagnifierEnabled,
     &kSystemMenuAccessibilityDockedMagnifierIcon},
    {FeatureType::kHighContrast, prefs::kAccessibilityHighContrastEnabled,
     &kSystemMenuAccessibilityContrastIcon},
    {FeatureType::kLargeCursor, prefs::kAccessibilityLargeCursorEnabled,
     nullptr},
    {FeatureType::kMonoAudio, prefs::kAccessibilityMonoAudioEnabled, nullptr},
    {FeatureType::kSpokenFeedback, prefs::kAccessibilitySpokenFeedbackEnabled,
     &kSystemMenuAccessibilityChromevoxIcon},
    {FeatureType::kSelectToSpeak, prefs::kAccessibilitySelectToSpeakEnabled,
     &kSystemMenuAccessibilitySelectToSpeakIcon},
    {FeatureType::kStickyKeys, prefs::kAccessibilityStickyKeysEnabled, nullptr},
    {FeatureType::kSwitchAccess, prefs::kAccessibilitySwitchAccessEnabled,
     &kSwitchAccessIcon},
    {FeatureType::kVirtualKeyboard, prefs::kAccessibilityVirtualKeyboardEnabled,
     &kSystemMenuKeyboardIcon}};

// An array describing the confirmation dialogs for the features which have
// them.
const FeatureDialogData kFeatureDialogs[] = {
    {FeatureType::kDictation, prefs::kDictationAcceleratorDialogHasBeenAccepted,
     IDS_ASH_DICTATION_CONFIRMATION_TITLE, IDS_ASH_DICTATION_CONFIRMATION_BODY,
     true},
    {FeatureType::kFullscreenMagnifier,
     prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
     IDS_ASH_SCREEN_MAGNIFIER_TITLE, IDS_ASH_SCREEN_MAGNIFIER_BODY, false},
    {FeatureType::kDockedMagnifier,
     prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
     IDS_ASH_DOCKED_MAGNIFIER_TITLE, IDS_ASH_DOCKED_MAGNIFIER_BODY, false},
    {FeatureType::kHighContrast,
     prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
     IDS_ASH_HIGH_CONTRAST_TITLE, IDS_ASH_HIGH_CONTRAST_BODY, false}};

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
    prefs::kAccessibilityCursorColorEnabled,
    prefs::kAccessibilityCursorColor,
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
    prefs::kAccessibilityShortcutsEnabled,
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

// List of switch access accessibility prefs that are to be copied (if changed
// by the user) from the current user to the signin screen profile. That way
// if a switch access user signs out, their switch continues to function.
constexpr const char* const kSwitchAccessPrefsCopiedToSignin[]{
    prefs::kAccessibilitySwitchAccessAutoScanEnabled,
    prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
    prefs::kAccessibilitySwitchAccessAutoScanSpeedMs,
    prefs::kAccessibilitySwitchAccessEnabled,
    prefs::kAccessibilitySwitchAccessNextKeyCodes,
    prefs::kAccessibilitySwitchAccessNextSetting,
    prefs::kAccessibilitySwitchAccessPreviousKeyCodes,
    prefs::kAccessibilitySwitchAccessPreviousSetting,
    prefs::kAccessibilitySwitchAccessSelectKeyCodes,
    prefs::kAccessibilitySwitchAccessSelectSetting,
};

// Helper function that is used to verify the validity of kFeatures and
// kFeatureDialogs.
bool VerifyFeaturesData() {
  // All feature prefs must be unique.
  std::set<const char*> feature_prefs;
  for (auto feature_data : kFeatures) {
    if (feature_prefs.find(feature_data.pref) != feature_prefs.end())
      return false;
    feature_prefs.insert(feature_data.pref);
  }

  for (auto dialog_data : kFeatureDialogs) {
    if (feature_prefs.find(dialog_data.pref) != feature_prefs.end())
      return false;
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
  // Shown when Switch Access is enabled.
  kSwitchAccessEnabled,
};

// Returns notification icon based on the A11yNotificationType.
const gfx::VectorIcon& GetNotificationIcon(A11yNotificationType type) {
  switch (type) {
    case A11yNotificationType::kSpokenFeedbackBrailleEnabled:
      return kNotificationAccessibilityIcon;
    case A11yNotificationType::kBrailleDisplayConnected:
      return kNotificationAccessibilityBrailleIcon;
    case A11yNotificationType::kSwitchAccessEnabled:
      return kSwitchAccessIcon;
    default:
      return kNotificationChromevoxIcon;
  }
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
  } else if (type == A11yNotificationType::kSwitchAccessEnabled) {
    title = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SWITCH_ACCESS_ENABLED_TITLE);
    text = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SWITCH_ACCESS_ENABLED);
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
  notification->set_pinned(true);
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

AccessibilityControllerImpl::Feature::Feature(
    FeatureType type,
    const std::string& pref_name,
    const gfx::VectorIcon* icon,
    AccessibilityControllerImpl* controller)
    : type_(type), pref_name_(pref_name), icon_(icon), owner_(controller) {}

AccessibilityControllerImpl::Feature::~Feature() = default;

void AccessibilityControllerImpl::Feature::SetEnabled(bool enabled) {
  PrefService* prefs = owner_->active_user_prefs_;
  if (!prefs)
    return;
  prefs->SetBoolean(pref_name_, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityControllerImpl::Feature::IsVisibleInTray() const {
  return (conflicting_feature_ == kNoConflictingFeature ||
          !owner_->GetFeature(conflicting_feature_).enabled()) &&
         owner_->IsAccessibilityFeatureVisibleInTrayMenu(pref_name_);
}

bool AccessibilityControllerImpl::Feature::IsEnterpriseIconVisible() const {
  return owner_->IsEnterpriseIconVisibleInTrayMenu(pref_name_);
}

const gfx::VectorIcon& AccessibilityControllerImpl::Feature::icon() const {
  DCHECK(icon_);
  if (icon_)
    return *icon_;
  return kPaletteTrayIconDefaultIcon;
}

void AccessibilityControllerImpl::Feature::UpdateFromPref() {
  PrefService* prefs = owner_->active_user_prefs_;
  DCHECK(prefs);

  bool enabled = prefs->GetBoolean(pref_name_);

  if (conflicting_feature_ != FeatureType::kNoConflictingFeature &&
      owner_->GetFeature(conflicting_feature_).enabled()) {
    enabled = false;
  }

  if (enabled == enabled_)
    return;

  enabled_ = enabled;
  owner_->UpdateFeatureFromPref(type_);
}

void AccessibilityControllerImpl::Feature::SetConflictingFeature(
    AccessibilityControllerImpl::FeatureType feature) {
  DCHECK_EQ(conflicting_feature_, FeatureType::kNoConflictingFeature);
  conflicting_feature_ = feature;
}

AccessibilityControllerImpl::FeatureWithDialog::FeatureWithDialog(
    FeatureType type,
    const std::string& pref_name,
    const gfx::VectorIcon* icon,
    const Dialog& dialog,
    AccessibilityControllerImpl* controller)
    : AccessibilityControllerImpl::Feature(type, pref_name, icon, controller),
      dialog_(dialog) {}
AccessibilityControllerImpl::FeatureWithDialog::~FeatureWithDialog() = default;

void AccessibilityControllerImpl::FeatureWithDialog::SetDialogAccepted() {
  PrefService* prefs = owner_->active_user_prefs_;
  if (!prefs)
    return;
  prefs->SetBoolean(dialog_.pref_name, true);
  prefs->CommitPendingWrite();
}

bool AccessibilityControllerImpl::FeatureWithDialog::WasDialogAccepted() const {
  PrefService* prefs = owner_->active_user_prefs_;
  DCHECK(prefs);
  return prefs->GetBoolean(dialog_.pref_name);
}

void AccessibilityControllerImpl::FeatureWithDialog::SetEnabledWithDialog(
    bool enabled,
    base::OnceClosure completion_callback) {
  PrefService* prefs = owner_->active_user_prefs_;
  if (!prefs)
    return;
  // We should not show the dialog when the feature is already enabled.
  if (enabled && !this->enabled() && !WasDialogAccepted()) {
    Shell::Get()->accelerator_controller()->MaybeShowConfirmationDialog(
        dialog_.title_resource_id, dialog_.body_resource_id,
        // Callback for if the user accepts the dialog
        base::BindOnce(
            [](base::WeakPtr<AccessibilityControllerImpl> owner,
               FeatureType type, base::OnceClosure completion_callback) {
              if (!owner)
                return;

              static_cast<FeatureWithDialog&>(owner->GetFeature(type))
                  .SetDialogAccepted();
              // If they accept, try again to set value to true
              owner->GetFeature(type).SetEnabled(true);
              std::move(completion_callback).Run();
            },
            owner_->weak_ptr_factory_.GetWeakPtr(), type_,
            std::move(completion_callback)),
        base::DoNothing());

    return;
  }
  Feature::SetEnabled(enabled);
  std::move(completion_callback).Run();
}

void AccessibilityControllerImpl::FeatureWithDialog::SetEnabled(bool enabled) {
  if (dialog_.mandatory)
    SetEnabledWithDialog(enabled, base::DoNothing());
  else
    Feature::SetEnabled(enabled);
}

AccessibilityControllerImpl::AccessibilityControllerImpl()
    : autoclick_delay_(AutoclickController::GetDefaultAutoclickDelay()) {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  CreateAccessibilityFeatures();
}

AccessibilityControllerImpl::~AccessibilityControllerImpl() {
  floating_menu_controller_.reset();
}

void AccessibilityControllerImpl::CreateAccessibilityFeatures() {
  DCHECK(VerifyFeaturesData());
  // First, build all features with dialog.
  std::map<FeatureType, Dialog> dialogs;
  for (auto dialog_data : kFeatureDialogs) {
    dialogs[dialog_data.type] = {dialog_data.pref, dialog_data.title,
                                 dialog_data.body, dialog_data.mandatory};
  }
  for (auto feature_data : kFeatures) {
    DCHECK(!features_[feature_data.type]);
    auto it = dialogs.find(feature_data.type);
    if (it == dialogs.end()) {
      features_[feature_data.type] = std::make_unique<Feature>(
          feature_data.type, feature_data.pref, feature_data.icon, this);
    } else {
      features_[feature_data.type] = std::make_unique<FeatureWithDialog>(
          feature_data.type, feature_data.pref, feature_data.icon, it->second,
          this);
    }
  }
}

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
      prefs::kAccessibilityCursorColorEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityCursorColor, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityDictationEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityFloatingMenuEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityFloatingMenuPosition,
      static_cast<int>(kDefaultFloatingMenuPosition),
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
      prefs::kAccessibilityShortcutsEnabled, true,
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
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, false,
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

void AccessibilityControllerImpl::AddObserver(AccessibilityObserver* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityControllerImpl::RemoveObserver(
    AccessibilityObserver* observer) {
  observers_.RemoveObserver(observer);
}

AccessibilityControllerImpl::Feature& AccessibilityControllerImpl::GetFeature(
    FeatureType type) const {
  DCHECK(features_[type].get());
  return *features_[type].get();
}

AccessibilityControllerImpl::Feature& AccessibilityControllerImpl::autoclick()
    const {
  return GetFeature(FeatureType::kAutoclick);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::caret_highlight() const {
  return GetFeature(FeatureType::kCaretHighlight);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::cursor_highlight() const {
  return GetFeature(FeatureType::KCursorHighlight);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::cursor_color() const {
  return GetFeature(FeatureType::kCursorColor);
}

AccessibilityControllerImpl::FeatureWithDialog&
AccessibilityControllerImpl::dictation() const {
  return static_cast<FeatureWithDialog&>(GetFeature(FeatureType::kDictation));
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::focus_highlight() const {
  return GetFeature(FeatureType::kFocusHighlight);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::floating_menu() const {
  return GetFeature(FeatureType::kFloatingMenu);
}

AccessibilityControllerImpl::FeatureWithDialog&
AccessibilityControllerImpl::fullscreen_magnifier() const {
  return static_cast<FeatureWithDialog&>(
      GetFeature(FeatureType::kFullscreenMagnifier));
}

AccessibilityControllerImpl::FeatureWithDialog&
AccessibilityControllerImpl::docked_magnifier() const {
  return static_cast<FeatureWithDialog&>(
      GetFeature(FeatureType::kDockedMagnifier));
}

AccessibilityControllerImpl::FeatureWithDialog&
AccessibilityControllerImpl::high_contrast() const {
  return static_cast<FeatureWithDialog&>(
      GetFeature(FeatureType::kHighContrast));
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::large_cursor() const {
  return GetFeature(FeatureType::kLargeCursor);
}

AccessibilityControllerImpl::Feature& AccessibilityControllerImpl::mono_audio()
    const {
  return GetFeature(FeatureType::kMonoAudio);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::spoken_feedback() const {
  return GetFeature(FeatureType::kSpokenFeedback);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::select_to_speak() const {
  return GetFeature(FeatureType::kSelectToSpeak);
}

AccessibilityControllerImpl::Feature& AccessibilityControllerImpl::sticky_keys()
    const {
  return GetFeature(FeatureType::kStickyKeys);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::switch_access() const {
  return GetFeature(FeatureType::kSwitchAccess);
}

AccessibilityControllerImpl::Feature&
AccessibilityControllerImpl::virtual_keyboard() const {
  return GetFeature(FeatureType::kVirtualKeyboard);
}

bool AccessibilityControllerImpl::IsAutoclickSettingVisibleInTray() {
  return autoclick().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForAutoclick() {
  return autoclick().IsEnterpriseIconVisible();
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
          IsSwitchAccessSettingVisibleInTray());
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

bool AccessibilityControllerImpl::IsCaretHighlightSettingVisibleInTray() {
  return caret_highlight().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForCaretHighlight() {
  return caret_highlight().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsCursorHighlightSettingVisibleInTray() {
  return cursor_highlight().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForCursorHighlight() {
  return cursor_highlight().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsDictationSettingVisibleInTray() {
  return dictation().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForDictation() {
  return dictation().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsFocusHighlightSettingVisibleInTray() {
  return focus_highlight().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForFocusHighlight() {
  return focus_highlight().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsFullScreenMagnifierSettingVisibleInTray() {
  return fullscreen_magnifier().IsVisibleInTray();
}

bool AccessibilityControllerImpl::
    IsEnterpriseIconVisibleForFullScreenMagnifier() {
  return fullscreen_magnifier().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsDockedMagnifierSettingVisibleInTray() {
  return docked_magnifier().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForDockedMagnifier() {
  return docked_magnifier().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsHighContrastSettingVisibleInTray() {
  return high_contrast().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForHighContrast() {
  return high_contrast().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsLargeCursorSettingVisibleInTray() {
  return large_cursor().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForLargeCursor() {
  return large_cursor().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsMonoAudioSettingVisibleInTray() {
  return mono_audio().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForMonoAudio() {
  return mono_audio().IsEnterpriseIconVisible();
}

void AccessibilityControllerImpl::SetSpokenFeedbackEnabled(
    bool enabled,
    AccessibilityNotificationVisibility notify) {
  spoken_feedback().SetEnabled(enabled);

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
  return spoken_feedback().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForSpokenFeedback() {
  return spoken_feedback().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsSelectToSpeakSettingVisibleInTray() {
  return select_to_speak().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForSelectToSpeak() {
  return select_to_speak().IsEnterpriseIconVisible();
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

bool AccessibilityControllerImpl::IsSwitchAccessRunning() const {
  return switch_access().enabled() || switch_access_disable_dialog_showing_;
}

bool AccessibilityControllerImpl::IsSwitchAccessSettingVisibleInTray() {
  // Switch Access cannot be enabled on the sign-in page because there is no way
  // to configure switches while the device is locked.
  if (!switch_access().enabled() &&
      Shell::Get()->session_controller()->login_status() ==
          ash::LoginStatus::NOT_LOGGED_IN) {
    return false;
  }
  return switch_access().IsVisibleInTray();
  return IsEnterpriseIconVisibleInTrayMenu(
      prefs::kAccessibilitySwitchAccessEnabled);
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForSwitchAccess() {
  return switch_access().IsEnterpriseIconVisible();
}

void AccessibilityControllerImpl::SetAccessibilityEventRewriter(
    AccessibilityEventRewriter* accessibility_event_rewriter) {
  accessibility_event_rewriter_ = accessibility_event_rewriter;
  if (accessibility_event_rewriter_)
    UpdateKeyCodesAfterSwitchAccessEnabled();
}

void AccessibilityControllerImpl::HideSwitchAccessBackButton() {
  if (IsSwitchAccessRunning())
    switch_access_bubble_controller_->HideBackButton();
}

void AccessibilityControllerImpl::HideSwitchAccessMenu() {
  if (IsSwitchAccessRunning())
    switch_access_bubble_controller_->HideMenuBubble();
}

void AccessibilityControllerImpl::ShowSwitchAccessBackButton(
    const gfx::Rect& anchor) {
  switch_access_bubble_controller_->ShowBackButton(anchor);
}

void AccessibilityControllerImpl::ShowSwitchAccessMenu(
    const gfx::Rect& anchor,
    std::vector<std::string> actions_to_show) {
  switch_access_bubble_controller_->ShowMenu(anchor, actions_to_show);
}

void AccessibilityControllerImpl::
    DisablePolicyRecommendationRestorerForTesting() {
  Shell::Get()->policy_recommendation_restorer()->DisableForTesting();
}

void AccessibilityControllerImpl::StartPointScanning() {
  if (!point_scan_controller_)
    point_scan_controller_.reset(new PointScanController());

  point_scan_controller_->Start();
}

bool AccessibilityControllerImpl::IsStickyKeysSettingVisibleInTray() {
  return sticky_keys().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForStickyKeys() {
  return sticky_keys().IsEnterpriseIconVisible();
}

bool AccessibilityControllerImpl::IsVirtualKeyboardSettingVisibleInTray() {
  return virtual_keyboard().IsVisibleInTray();
}

bool AccessibilityControllerImpl::IsEnterpriseIconVisibleForVirtualKeyboard() {
  return virtual_keyboard().IsEnterpriseIconVisible();
}

void AccessibilityControllerImpl::ShowFloatingMenuIfEnabled() {
  if (floating_menu().enabled() && !floating_menu_controller_) {
    floating_menu_controller_ =
        std::make_unique<FloatingAccessibilityController>(this);
    floating_menu_controller_->Show(GetFloatingMenuPosition());
  } else {
    always_show_floating_menu_when_enabled_ = true;
  }
}

FloatingAccessibilityController*
AccessibilityControllerImpl::GetFloatingMenuController() {
  return floating_menu_controller_.get();
}

void AccessibilityControllerImpl::SetTabletModeShelfNavigationButtonsEnabled(
    bool enabled) {
  if (!active_user_prefs_)
    return;

  active_user_prefs_->SetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, enabled);
  active_user_prefs_->CommitPendingWrite();
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
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  if (client_)
    client_->HandleAccessibilityGesture(gesture, location);
}

void AccessibilityControllerImpl::ToggleDictation() {
  // Do nothing if dictation is not enabled.
  if (!dictation().enabled())
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

  dictation().SetEnabled(true);
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
  if (connected && spoken_feedback().enabled())
    type = A11yNotificationType::kBrailleDisplayConnected;
  else if (connected && !spoken_feedback().enabled())
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

AccessibilityEventRewriter*
AccessibilityControllerImpl::GetAccessibilityEventRewriterForTest() {
  return accessibility_event_rewriter_;
}

void AccessibilityControllerImpl::
    DisableSwitchAccessDisableConfirmationDialogTesting() {
  no_switch_access_disable_confirmation_dialog_for_testing_ = true;
}

void AccessibilityControllerImpl::OnTabletModeStarted() {
  if (spoken_feedback().enabled())
    ShowAccessibilityNotification(A11yNotificationType::kSpokenFeedbackEnabled);
}

void AccessibilityControllerImpl::OnTabletModeEnded() {
  if (spoken_feedback().enabled())
    ShowAccessibilityNotification(A11yNotificationType::kSpokenFeedbackEnabled);
}

void AccessibilityControllerImpl::ObservePrefs(PrefService* prefs) {
  DCHECK(prefs);

  active_user_prefs_ = prefs;

  // Watch for pref updates from webui settings and policy.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  // It is safe to use base::Unreatined since we own pref_change_registrar.
  for (int feature_id = 0; feature_id < FeatureType::kFeatureCount;
       feature_id++) {
    Feature* feature = features_[feature_id].get();
    DCHECK(feature);
    pref_change_registrar_->Add(
        feature->pref_name(),
        base::BindRepeating(
            &AccessibilityControllerImpl::Feature::UpdateFromPref,
            base::Unretained(feature)));
    feature->UpdateFromPref();
  }

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
      prefs::kAccessibilityFloatingMenuPosition,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateFloatingMenuPositionFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorDipSize,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateLargeCursorFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityShortcutsEnabled,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateShortcutsEnabledFromPref,
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
      prefs::kAccessibilitySwitchAccessNextSetting,
      base::BindRepeating(
          &AccessibilityControllerImpl::SyncSwitchAccessPrefsToSignInProfile,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessPreviousSetting,
      base::BindRepeating(
          &AccessibilityControllerImpl::SyncSwitchAccessPrefsToSignInProfile,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySwitchAccessSelectSetting,
      base::BindRepeating(
          &AccessibilityControllerImpl::SyncSwitchAccessPrefsToSignInProfile,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled,
      base::BindRepeating(&AccessibilityControllerImpl::
                              UpdateTabletModeShelfNavigationButtonsFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityCursorColor,
      base::BindRepeating(
          &AccessibilityControllerImpl::UpdateCursorColorFromPrefs,
          base::Unretained(this)));

  // Load current state.
  for (int feature_id = 0; feature_id < FeatureType::kFeatureCount;
       feature_id++) {
    features_[feature_id]->UpdateFromPref();
  }

  UpdateAutoclickDelayFromPref();
  UpdateAutoclickEventTypeFromPref();
  UpdateAutoclickRevertToLeftClickFromPref();
  UpdateAutoclickStabilizePositionFromPref();
  UpdateAutoclickMovementThresholdFromPref();
  UpdateAutoclickMenuPositionFromPref();
  UpdateFloatingMenuPositionFromPref();
  UpdateLargeCursorFromPref();
  UpdateCursorColorFromPrefs();
  UpdateShortcutsEnabledFromPref();
  UpdateTabletModeShelfNavigationButtonsFromPref();
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
    FloatingMenuPosition position) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetInteger(prefs::kAccessibilityAutoclickMenuPosition,
                                 static_cast<int>(position));
  active_user_prefs_->CommitPendingWrite();
  Shell::Get()->autoclick_controller()->SetMenuPosition(position);
}

FloatingMenuPosition AccessibilityControllerImpl::GetAutoclickMenuPosition() {
  DCHECK(active_user_prefs_);
  return static_cast<FloatingMenuPosition>(active_user_prefs_->GetInteger(
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

void AccessibilityControllerImpl::HandleAutoclickScrollableBoundsFound(
    gfx::Rect& bounds_in_screen) {
  Shell::Get()->autoclick_controller()->HandleAutoclickScrollableBoundsFound(
      bounds_in_screen);
}

void AccessibilityControllerImpl::SetFloatingMenuPosition(
    FloatingMenuPosition position) {
  if (!active_user_prefs_)
    return;
  active_user_prefs_->SetInteger(prefs::kAccessibilityFloatingMenuPosition,
                                 static_cast<int>(position));
  active_user_prefs_->CommitPendingWrite();
}

void AccessibilityControllerImpl::UpdateFloatingMenuPositionFromPref() {
  if (floating_menu_controller_)
    floating_menu_controller_->SetMenuPosition(GetFloatingMenuPosition());
}

FloatingMenuPosition AccessibilityControllerImpl::GetFloatingMenuPosition() {
  DCHECK(active_user_prefs_);
  return static_cast<FloatingMenuPosition>(active_user_prefs_->GetInteger(
      prefs::kAccessibilityFloatingMenuPosition));
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

  if (large_cursor_size_in_dip_ == size)
    return;

  large_cursor_size_in_dip_ = size;

  NotifyAccessibilityStatusChanged();

  Shell* shell = Shell::Get();
  shell->cursor_manager()->SetCursorSize(large_cursor().enabled()
                                             ? ui::CursorSize::kLarge
                                             : ui::CursorSize::kNormal);
  shell->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
  shell->UpdateCursorCompositingEnabled();
}

void AccessibilityControllerImpl::UpdateCursorColorFromPrefs() {
  DCHECK(active_user_prefs_);

  // Not yet released: cursor color is behind a flag.
  if (!features::IsAccessibilityCursorColorEnabled())
    return;

  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityCursorColorEnabled);
  Shell* shell = Shell::Get();
  shell->SetCursorColor(
      enabled ? active_user_prefs_->GetInteger(prefs::kAccessibilityCursorColor)
              : kDefaultCursorColor);
  NotifyAccessibilityStatusChanged();
  shell->UpdateCursorCompositingEnabled();
}

void AccessibilityControllerImpl::UpdateAccessibilityHighlightingFromPrefs() {
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

void AccessibilityControllerImpl::MaybeCreateSelectToSpeakEventHandler() {
  // Construct the handler as needed when Select-to-Speak is enabled and the
  // delegate is set. Otherwise, destroy the handler when Select-to-Speak is
  // disabled or the delegate has been destroyed.
  if (!select_to_speak().enabled() ||
      !select_to_speak_event_handler_delegate_) {
    select_to_speak_event_handler_.reset();
    return;
  }

  if (select_to_speak_event_handler_)
    return;

  select_to_speak_event_handler_ = std::make_unique<SelectToSpeakEventHandler>(
      select_to_speak_event_handler_delegate_);
}

void AccessibilityControllerImpl::UpdateSwitchAccessKeyCodesFromPref(
    SwitchAccessCommand command) {
  if (!active_user_prefs_)
    return;

  SyncSwitchAccessPrefsToSignInProfile();

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

  if (accessibility_event_rewriter_)
    accessibility_event_rewriter_->SetKeyCodesForSwitchAccessCommand(key_codes,
                                                                     command);
}

void AccessibilityControllerImpl::UpdateSwitchAccessAutoScanEnabledFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled);

  base::UmaHistogramBoolean("Accessibility.CrosSwitchAccess.AutoScan", enabled);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityControllerImpl::UpdateSwitchAccessAutoScanSpeedFromPref() {
  DCHECK(active_user_prefs_);
  const int speed_ms = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessAutoScanSpeedMs);

  base::UmaHistogramCustomCounts(
      "Accessibility.CrosSwitchAccess.AutoScan.SpeedMs", speed_ms, 1 /* min */,
      10000 /* max */, 100 /* buckets */);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityControllerImpl::
    UpdateSwitchAccessAutoScanKeyboardSpeedFromPref() {
  DCHECK(active_user_prefs_);
  const int speed_ms = active_user_prefs_->GetInteger(
      prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs);

  base::UmaHistogramCustomCounts(
      "Accessibility.CrosSwitchAccess.AutoScan.KeyboardSpeedMs", speed_ms,
      1 /* min */, 10000 /* max */, 100 /* buckets */);
  SyncSwitchAccessPrefsToSignInProfile();
}

void AccessibilityControllerImpl::SwitchAccessDisableDialogClosed(
    bool disable_dialog_accepted) {
  switch_access_disable_dialog_showing_ = false;
  // Always deactivate switch access. Turning switch access off ensures it is
  // re-activated correctly.
  // The pref was already disabled, but we left switch access on so the user
  // could interact with the dialog.
  DeactivateSwitchAccess();
  if (disable_dialog_accepted) {
    NotifyAccessibilityStatusChanged();
    SyncSwitchAccessPrefsToSignInProfile();
  } else {
    // Reset the preference (which was already set to false). Doing so turns
    // switch access back on.
    skip_switch_access_notification_ = true;
    switch_access().SetEnabled(true);
  }
}

void AccessibilityControllerImpl::UpdateKeyCodesAfterSwitchAccessEnabled() {
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kSelect);
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kNext);
  UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand::kPrevious);
}

void AccessibilityControllerImpl::ActivateSwitchAccess() {
  switch_access_bubble_controller_ =
      std::make_unique<SwitchAccessMenuBubbleController>();
  UpdateKeyCodesAfterSwitchAccessEnabled();
  if (::switches::IsSwitchAccessPointScanningEnabled())
    StartPointScanning();
  if (skip_switch_access_notification_) {
    skip_switch_access_notification_ = false;
    return;
  }

  ShowAccessibilityNotification(A11yNotificationType::kSwitchAccessEnabled);
}

void AccessibilityControllerImpl::DeactivateSwitchAccess() {
  if (client_)
    client_->OnSwitchAccessDisabled();
  switch_access_bubble_controller_.reset();
}

void AccessibilityControllerImpl::SyncSwitchAccessPrefsToSignInProfile() {
  if (!active_user_prefs_ || IsSigninPrefService(active_user_prefs_))
    return;

  PrefService* signin_prefs =
      Shell::Get()->session_controller()->GetSigninScreenPrefService();
  DCHECK(signin_prefs);

  for (const auto* pref_path : kSwitchAccessPrefsCopiedToSignin) {
    const PrefService::Preference* pref =
        active_user_prefs_->FindPreference(pref_path);

    // Ignore if the pref has not been set by the user.
    if (!pref || !pref->IsUserControlled())
      continue;

    // Copy the pref value to the signin profile.
    const base::Value* value = pref->GetValue();
    signin_prefs->Set(pref_path, *value);
  }
}

void AccessibilityControllerImpl::UpdateShortcutsEnabledFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled =
      active_user_prefs_->GetBoolean(prefs::kAccessibilityShortcutsEnabled);

  if (shortcuts_enabled_ == enabled)
    return;

  shortcuts_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
}

void AccessibilityControllerImpl::
    UpdateTabletModeShelfNavigationButtonsFromPref() {
  DCHECK(active_user_prefs_);
  const bool enabled = active_user_prefs_->GetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled);

  if (tablet_mode_shelf_navigation_buttons_enabled_ == enabled)
    return;

  tablet_mode_shelf_navigation_buttons_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
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

void AccessibilityControllerImpl::PerformAcceleratorAction(
    AcceleratorAction accelerator_action) {
  AcceleratorController::Get()->PerformActionIfEnabled(accelerator_action,
                                                       /* accelerator = */ {});
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

void AccessibilityControllerImpl::UpdateFeatureFromPref(FeatureType feature) {
  bool enabled = features_[feature]->enabled();

  switch (feature) {
    case FeatureType::kAutoclick:
      Shell::Get()->autoclick_controller()->SetEnabled(
          enabled, true /* show confirmation dialog */);
      break;
    case FeatureType::kCaretHighlight:
      UpdateAccessibilityHighlightingFromPrefs();
      break;
    case FeatureType::KCursorHighlight:
      UpdateAccessibilityHighlightingFromPrefs();
      break;
    case FeatureType::kDictation:
      break;
    case FeatureType::kFloatingMenu:
      if (enabled && always_show_floating_menu_when_enabled_)
        ShowFloatingMenuIfEnabled();
      else
        floating_menu_controller_.reset();
      break;
    case FeatureType::kFocusHighlight:
      UpdateAccessibilityHighlightingFromPrefs();
      break;
    case FeatureType::kFullscreenMagnifier:
      break;
    case FeatureType::kDockedMagnifier:
      break;
    case FeatureType::kHighContrast:
      Shell::Get()->high_contrast_controller()->SetEnabled(enabled);
      Shell::Get()->UpdateCursorCompositingEnabled();
      break;
    case FeatureType::kLargeCursor:
      if (!enabled)
        active_user_prefs_->ClearPref(prefs::kAccessibilityLargeCursorDipSize);

      Shell::Get()->cursor_manager()->SetCursorSize(
          large_cursor().enabled() ? ui::CursorSize::kLarge
                                   : ui::CursorSize::kNormal);
      Shell::Get()->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
      Shell::Get()->UpdateCursorCompositingEnabled();
      break;
    case FeatureType::kMonoAudio:
      chromeos::CrasAudioHandler::Get()->SetOutputMonoEnabled(enabled);
      break;
    case FeatureType::kSpokenFeedback:
      message_center::MessageCenter::Get()->SetSpokenFeedbackEnabled(enabled);
      // TODO(warx): ChromeVox loading/unloading requires browser process
      // started, thus it is still handled on Chrome side.

      // ChromeVox focus highlighting overrides the other focus highlighting.
      focus_highlight().UpdateFromPref();
      break;
    case FeatureType::kSelectToSpeak:
      select_to_speak_state_ = SelectToSpeakState::kSelectToSpeakStateInactive;
      if (enabled)
        MaybeCreateSelectToSpeakEventHandler();
      else
        select_to_speak_event_handler_.reset();
      break;
    case FeatureType::kStickyKeys:
      Shell::Get()->sticky_keys_controller()->Enable(enabled);
      break;
    case FeatureType::kSwitchAccess:
      if (!enabled) {
        ShowAccessibilityNotification(A11yNotificationType::kNone);
        if (no_switch_access_disable_confirmation_dialog_for_testing_) {
          SwitchAccessDisableDialogClosed(true);
        } else {
          // Show a dialog before disabling Switch Access.
          new AccessibilityFeatureDisableDialog(
              IDS_ASH_SWITCH_ACCESS_DISABLE_CONFIRMATION_TITLE,
              IDS_ASH_SWITCH_ACCESS_DISABLE_CONFIRMATION_BODY,
              base::BindOnce(
                  &AccessibilityControllerImpl::SwitchAccessDisableDialogClosed,
                  weak_ptr_factory_.GetWeakPtr(), true),
              base::BindOnce(
                  &AccessibilityControllerImpl::SwitchAccessDisableDialogClosed,
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
      UpdateCursorColorFromPrefs();
      break;
    case FeatureType::kFeatureCount:
    case FeatureType::kNoConflictingFeature:
      NOTREACHED();
  }
  NotifyAccessibilityStatusChanged();
}

}  // namespace ash
