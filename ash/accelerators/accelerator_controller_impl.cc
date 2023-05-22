// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_notifications.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/devicetype.h"
#include "ash/debug.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/multi_profile_uma.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
#include "ui/aura/env.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/event_constants.h"

namespace ash {

/*
The encoding schema is as the following:

- The low 16 bits represent the key code.
- The modifiers are stored in the high 16 bits. Only the following 4 bits are
being used:
  - The 31 bit: Command key
  - The 30 bit: Alt key
  - The 29 bit: Control key
  - The 28 bit: Shift key

Examples:
  ctl+Z:        0001'0000'0000'0000'0000'0000'0101'1010
  alt+shift+A:  0010'1000'0000'0000'0000'0000'0100'0001
  */
int GetEncodedShortcut(const ui::Accelerator& accelerator) {
  // EF_SHIFT_DOWN: 28th bit.
  const int kShiftDown = 1 << 27;
  // EF_CONTROL_DOWN: 29th bit.
  const int kControlDown = 1 << 28;
  // EF_ALT_DOWN: 30th bit.
  const int kAltDown = 1 << 29;
  // EF_COMMAND_DOWN: 31th bit.
  const int kCommandDown = 1 << 30;

  int encoded_modifier = 0;
  if (accelerator.IsShiftDown()) {
    encoded_modifier |= kShiftDown;
  }
  if (accelerator.IsCtrlDown()) {
    encoded_modifier |= kControlDown;
  }
  if (accelerator.IsAltDown()) {
    encoded_modifier |= kAltDown;
  }
  if (accelerator.IsCmdDown()) {
    encoded_modifier |= kCommandDown;
  }

  // Currently KeyboardCode only has 2^8 values. It will be a long time until we
  // get to 2^16. But if KeyboardCode has 2^28+ values for some reason, the top
  // 5 bits will be overwritten.
  DCHECK((0xF800 & accelerator.key_code()) == 0);
  return encoded_modifier | accelerator.key_code();
}

namespace {

using ::base::UserMetricsAction;
using ::chromeos::WindowStateType;
using input_method::InputMethodManager;

static_assert(AcceleratorAction::kDesksActivate0 ==
                      AcceleratorAction::kDesksActivate1 - 1 &&
                  AcceleratorAction::kDesksActivate1 ==
                      AcceleratorAction::kDesksActivate2 - 1 &&
                  AcceleratorAction::kDesksActivate2 ==
                      AcceleratorAction::kDesksActivate3 - 1 &&
                  AcceleratorAction::kDesksActivate3 ==
                      AcceleratorAction::kDesksActivate4 - 1 &&
                  AcceleratorAction::kDesksActivate4 ==
                      AcceleratorAction::kDesksActivate5 - 1 &&
                  AcceleratorAction::kDesksActivate5 ==
                      AcceleratorAction::kDesksActivate6 - 1 &&
                  AcceleratorAction::kDesksActivate6 ==
                      AcceleratorAction::kDesksActivate7 - 1,
              "DESKS_ACTIVATE* actions must be consecutive");

ui::Accelerator CreateAccelerator(ui::KeyboardCode keycode,
                                  int modifiers,
                                  bool trigger_on_press) {
  ui::Accelerator accelerator(keycode, modifiers);
  accelerator.set_key_state(trigger_on_press
                                ? ui::Accelerator::KeyState::PRESSED
                                : ui::Accelerator::KeyState::RELEASED);
  return accelerator;
}

void RecordUmaHistogram(const char* histogram_name,
                        DeprecatedAcceleratorUsage sample) {
  auto* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, DEPRECATED_USAGE_COUNT, DEPRECATED_USAGE_COUNT + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void RecordActionUmaHistogram(AcceleratorAction action,
                              const ui::Accelerator& accelerator) {
  base::UmaHistogramSparse(base::StrCat({"Ash.Accelerators.Actions.",
                                         GetAcceleratorActionName(action)}),
                           GetEncodedShortcut(accelerator));
}

void RecordImeSwitchByAccelerator() {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeSwitch",
                            ImeSwitchType::kAccelerator);
}

void RecordImeSwitchByModeChangeKey() {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeSwitch",
                            ImeSwitchType::kModeChangeKey);
}

void RecordCycleBackwardMru(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_PrevWindow_Tab"));
}

void RecordCycleForwardMru(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_NextWindow_Tab"));
}

void RecordToggleAssistant(const ui::Accelerator& accelerator) {
  if (accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_SPACE) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_Space"));
  } else if (accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_A) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_A"));
  } else if (accelerator.key_code() == ui::VKEY_ASSISTANT) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Assistant"));
  }
}

void RecordToggleAppList(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_LWIN)
    base::RecordAction(UserMetricsAction("Accel_Search_LWin"));
}

void RecordSwitchToNextIme(const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Next_Ime"));
  if (accelerator.key_code() == ui::VKEY_MODECHANGE)
    RecordImeSwitchByModeChangeKey();
  else
    RecordImeSwitchByAccelerator();
}

void RecordToggleFullscreen(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ZOOM)
    base::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
}

void RecordNewTab(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_T)
    base::RecordAction(UserMetricsAction("Accel_NewTab_T"));
}

// Check if accelerator should trigger ToggleAssistant action.
bool ShouldToggleAssistant(const ui::Accelerator& accelerator) {
  // Search+A shortcut is disabled on device with an assistant key.
  // Currently only Google branded device has the key. Some external keyboard
  // may report it has the key but actually not.  This would cause keyboard
  // shortcut stops working.  So we only check the key on these branded
  // devices.
  return !(accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_A &&
           IsGoogleBrandedDevice() && ui::DeviceKeyboardHasAssistantKey());
}

void HandleSwitchToLastUsedIme(const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Previous_Ime"));
  if (accelerator.key_state() == ui::Accelerator::KeyState::PRESSED) {
    RecordImeSwitchByAccelerator();
    Shell::Get()->ime_controller()->SwitchToLastUsedIme();
  }
  // Else: consume the Ctrl+Space ET_KEY_RELEASED event but do not do anything.
}

bool CanHandleSwitchIme(const ui::Accelerator& accelerator) {
  return Shell::Get()->ime_controller()->CanSwitchImeWithAccelerator(
      accelerator);
}

void HandleSwitchIme(const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Switch_Ime"));
  RecordImeSwitchByAccelerator();
  Shell::Get()->ime_controller()->SwitchImeWithAccelerator(accelerator);
}

bool CanHandleToggleAppList(
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator,
    const std::set<ui::KeyboardCode>& currently_pressed_keys) {
  for (auto key : currently_pressed_keys) {
    // The AppList accelerator is triggered on search(VKEY_LWIN) key release.
    // Sometimes users will press and release the search key while holding other
    // keys in an attempt to trigger a different accelerator. We should not
    // toggle the AppList in that case. Check for VKEY_SHIFT because this is
    // used to show fullscreen app list.
    if (key != ui::VKEY_LWIN && key != ui::VKEY_SHIFT &&
        key != ui::VKEY_BROWSER_SEARCH && key != ui::VKEY_ALL_APPLICATIONS) {
      return false;
    }
  }

  if (accelerator.key_code() == ui::VKEY_LWIN) {
    // If something else was pressed between the Search key (LWIN)
    // being pressed and released, then ignore the release of the
    // Search key.
    if (previous_accelerator.key_state() !=
            ui::Accelerator::KeyState::PRESSED ||
        previous_accelerator.key_code() != ui::VKEY_LWIN ||
        previous_accelerator.interrupted_by_mouse_event()) {
      return false;
    }

    // When spoken feedback is enabled, we should neither toggle the list nor
    // consume the key since Search+Shift is one of the shortcuts the a11y
    // feature uses. crbug.com/132296
    if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
      return false;
  }
  return true;
}

bool CanHandleDisableCapsLock(const ui::Accelerator& previous_accelerator) {
  ui::KeyboardCode previous_key_code = previous_accelerator.key_code();
  if (previous_accelerator.key_state() == ui::Accelerator::KeyState::RELEASED ||
      (previous_key_code != ui::VKEY_LSHIFT &&
       previous_key_code != ui::VKEY_SHIFT &&
       previous_key_code != ui::VKEY_RSHIFT)) {
    // If something else was pressed between the Shift key being pressed
    // and released, then ignore the release of the Shift key.
    return false;
  }
  return Shell::Get()->ime_controller()->IsCapsLockEnabled();
}

bool CanHandleToggleCapsLock(
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator,
    const std::set<ui::KeyboardCode>& currently_pressed_keys) {
  // Iterate the set of pressed keys. If any redundant key is pressed, CapsLock
  // should not be triggered. Otherwise, CapsLock may be triggered accidentally.
  // See issue 789283 (https://crbug.com/789283)
  for (const auto& pressed_key : currently_pressed_keys) {
    if (pressed_key != ui::VKEY_LWIN && pressed_key != ui::VKEY_MENU)
      return false;
  }

  // This shortcut is set to be trigger on release. Either the current
  // accelerator is a Search release or Alt release.
  if (accelerator.key_code() == ui::VKEY_LWIN &&
      accelerator.key_state() == ui::Accelerator::KeyState::RELEASED) {
    // The previous must be either an Alt press or Search press:
    // 1. Press Alt, Press Search, Release Search, Release Alt.
    // 2. Press Search, Press Alt, Release Search, Release Alt.
    if (previous_accelerator.key_state() ==
            ui::Accelerator::KeyState::PRESSED &&
        (previous_accelerator.key_code() == ui::VKEY_LWIN ||
         previous_accelerator.key_code() == ui::VKEY_MENU)) {
      return true;
    }
  }

  // Alt release.
  if (accelerator.key_code() == ui::VKEY_MENU &&
      accelerator.key_state() == ui::Accelerator::KeyState::RELEASED) {
    // The previous must be either an Alt press or Search press:
    // 3. Press Alt, Press Search, Release Alt, Release Search.
    // 4. Press Search, Press Alt, Release Alt, Release Search.
    if (previous_accelerator.key_state() ==
            ui::Accelerator::KeyState::PRESSED &&
        (previous_accelerator.key_code() == ui::VKEY_LWIN ||
         previous_accelerator.key_code() == ui::VKEY_MENU)) {
      return true;
    }
  }

  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerImpl, public:

AcceleratorControllerImpl::TestApi::TestApi(
    AcceleratorControllerImpl* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

bool AcceleratorControllerImpl::TestApi::TriggerTabletModeVolumeAdjustTimer() {
  return controller_->tablet_volume_controller_
      .TriggerTabletModeVolumeAdjustTimerForTest();  // IN-TEST
}

void AcceleratorControllerImpl::TestApi::RegisterAccelerators(
    base::span<const AcceleratorData> accelerators) {
  // Initializing accelerators will register them.
  controller_->accelerator_configuration()->Initialize(accelerators);
  // If customization is not available, register the accelerators manually.
  if (!::features::IsShortcutCustomizationEnabled()) {
    controller_->RegisterAccelerators(accelerators);
  }
}

void AcceleratorControllerImpl::TestApi::ObserveAcceleratorUpdates() {
  DCHECK(::features::IsShortcutCustomizationEnabled());
  controller_->accelerator_configuration()->AddObserver(controller_);
}

bool AcceleratorControllerImpl::TestApi::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) {
  return controller_->IsActionForAcceleratorEnabled(accelerator);
}

const DeprecatedAcceleratorData*
AcceleratorControllerImpl::TestApi::GetDeprecatedAcceleratorData(
    AcceleratorAction action) {
  return controller_->accelerator_configuration()->GetDeprecatedAcceleratorData(
      action);
}

ExitWarningHandler*
AcceleratorControllerImpl::TestApi::GetExitWarningHandler() {
  return &controller_->exit_warning_handler_;
}

const TabletVolumeController::SideVolumeButtonLocation&
AcceleratorControllerImpl::TestApi::GetSideVolumeButtonLocation() {
  return controller_->tablet_volume_controller_
      .GetSideVolumeButtonLocationForTest();  // IN-TEST
}

void AcceleratorControllerImpl::TestApi::SetSideVolumeButtonFilePath(
    base::FilePath path) {
  controller_->tablet_volume_controller_
      .SetSideVolumeButtonFilePathForTest(  // IN-TEST
          path);
}

void AcceleratorControllerImpl::TestApi::SetSideVolumeButtonLocation(
    const std::string& region,
    const std::string& side) {
  controller_->tablet_volume_controller_
      .SetSideVolumeButtonLocationForTest(  // IN-TEST
          region, side);
}

AcceleratorControllerImpl::AcceleratorControllerImpl(
    AshAcceleratorConfiguration* config)
    : accelerator_manager_(std::make_unique<ui::AcceleratorManager>()),
      accelerator_history_(std::make_unique<AcceleratorHistoryImpl>()),
      accelerator_configuration_(config),
      output_volume_metric_delay_timer_(
          FROM_HERE,
          CrasAudioHandler::kMetricsDelayTimerInterval,
          /*receiver=*/this,
          &AcceleratorControllerImpl::RecordVolumeSource) {
  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    // Observe input method changes to determine when to use positional
    // shortcuts. Calling AddObserver will cause InputMethodChanged to be
    // called once even when the method does not change.
    InputMethodManager::Get()->AddObserver(this);
  }

  Init();

  if (::features::IsShortcutCustomizationEnabled()) {
    accelerator_configuration_->AddObserver(this);
  }

  // Let AcceleratorHistory be a PreTargetHandler on aura::Env to ensure that it
  // receives KeyEvents and MouseEvents. In some cases Shell PreTargetHandlers
  // will handle Events before AcceleratorHistory gets to see them. This
  // interferes with Accelerator processing. See https://crbug.com/1174603.
  aura::Env::GetInstance()->AddPreTargetHandler(
      accelerator_history_.get(), ui::EventTarget::Priority::kAccessibility);
}

AcceleratorControllerImpl::~AcceleratorControllerImpl() {
  // |AcceleratorControllerImpl| is owned by the shell which always is
  // deconstructed before |InputMethodManager|
  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    InputMethodManager::Get()->RemoveObserver(this);
  }
  if (::features::IsShortcutCustomizationEnabled()) {
    accelerator_configuration_->RemoveObserver(this);
  }
  aura::Env::GetInstance()->RemovePreTargetHandler(accelerator_history_.get());
}

void AcceleratorControllerImpl::InputMethodChanged(InputMethodManager* manager,
                                                   Profile* profile,
                                                   bool show_message) {
  DCHECK(::features::IsImprovedKeyboardShortcutsEnabled());
  DCHECK(manager);

  // InputMethodChanged will be called as soon as the observer is registered
  // from Init(), so these settings get propagated before any keys are
  // seen.
  const bool use_positional_lookup =
      manager->ArePositionalShortcutsUsedByCurrentInputMethod();
  accelerator_configuration_->SetUsePositionalLookup(use_positional_lookup);
  accelerator_manager_->SetUsePositionalLookup(use_positional_lookup);
}

void AcceleratorControllerImpl::OnAcceleratorsUpdated() {
  DCHECK(::features::IsShortcutCustomizationEnabled());

  // Accelerators have been updated, unregister all accelerators and re-register
  // them.
  UnregisterAll(this);
  RegisterAccelerators(accelerator_configuration_->GetAllAccelerators());
}

void AcceleratorControllerImpl::Register(
    const std::vector<ui::Accelerator>& accelerators,
    ui::AcceleratorTarget* target) {
  accelerator_manager_->Register(
      accelerators, ui::AcceleratorManager::kNormalPriority, target);
}

void AcceleratorControllerImpl::Unregister(const ui::Accelerator& accelerator,
                                           ui::AcceleratorTarget* target) {
  accelerator_manager_->Unregister(accelerator, target);
}

void AcceleratorControllerImpl::UnregisterAll(ui::AcceleratorTarget* target) {
  accelerator_manager_->UnregisterAll(target);
}

bool AcceleratorControllerImpl::Process(const ui::Accelerator& accelerator) {
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorControllerImpl::IsDeprecated(
    const ui::Accelerator& accelerator) const {
  return accelerator_configuration_->IsDeprecated(accelerator);
}

bool AcceleratorControllerImpl::PerformActionIfEnabled(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  if (CanPerformAction(action, accelerator)) {
    PerformAction(action, accelerator);
    return true;
  }
  return false;
}

bool AcceleratorControllerImpl::OnMenuAccelerator(
    const ui::Accelerator& accelerator) {
  accelerator_history_->StoreCurrentAccelerator(accelerator);

  // Menu shouldn't be closed for an invalid accelerator.
  const AcceleratorAction* action_ptr =
      accelerator_configuration_->FindAcceleratorAction(accelerator);
  return action_ptr && !base::Contains(actions_keeping_menu_open_, *action_ptr);
}

bool AcceleratorControllerImpl::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->IsRegistered(accelerator);
}

AcceleratorHistoryImpl* AcceleratorControllerImpl::GetAcceleratorHistory() {
  return accelerator_history_.get();
}

bool AcceleratorControllerImpl::DoesAcceleratorMatchAction(
    const ui::Accelerator& accelerator,
    AcceleratorAction action) {
  const AcceleratorAction* action_ptr =
      accelerator_configuration_->FindAcceleratorAction(accelerator);
  return action_ptr && *action_ptr == action;
}

bool AcceleratorControllerImpl::IsPreferred(
    const ui::Accelerator& accelerator) const {
  const AcceleratorAction* action_ptr =
      accelerator_configuration_->FindAcceleratorAction(accelerator);
  return action_ptr && base::Contains(preferred_actions_, *action_ptr);
}

bool AcceleratorControllerImpl::IsReserved(
    const ui::Accelerator& accelerator) const {
  const AcceleratorAction* action_ptr =
      accelerator_configuration_->FindAcceleratorAction(accelerator);

  return action_ptr && base::Contains(reserved_actions_, *action_ptr);
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerImpl, ui::AcceleratorTarget implementation:

bool AcceleratorControllerImpl::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const AcceleratorAction* action =
      accelerator_configuration_->FindAcceleratorAction(accelerator);

  if (!action || !CanPerformAction(*action, accelerator)) {
    return false;
  }

  // Handling the deprecated accelerators (if any) only if action can be
  // performed.
  if (MaybeDeprecatedAcceleratorPressed(*action, accelerator) ==
      AcceleratorProcessingStatus::STOP) {
    return false;
  }

  PerformAction(*action, accelerator);
  return ShouldActionConsumeKeyEvent(*action);
}

bool AcceleratorControllerImpl::CanHandleAccelerators() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerImpl, private:

void AcceleratorControllerImpl::Init() {
  for (size_t i = 0; i < kActionsAllowedAtLoginOrLockScreenLength; ++i) {
    actions_allowed_at_login_screen_.insert(
        kActionsAllowedAtLoginOrLockScreen[i]);
    actions_allowed_at_lock_screen_.insert(
        kActionsAllowedAtLoginOrLockScreen[i]);
  }
  for (size_t i = 0; i < kActionsAllowedAtLockScreenLength; ++i)
    actions_allowed_at_lock_screen_.insert(kActionsAllowedAtLockScreen[i]);
  for (size_t i = 0; i < kActionsAllowedAtPowerMenuLength; ++i)
    actions_allowed_at_power_menu_.insert(kActionsAllowedAtPowerMenu[i]);
  for (size_t i = 0; i < kActionsAllowedAtModalWindowLength; ++i)
    actions_allowed_at_modal_window_.insert(kActionsAllowedAtModalWindow[i]);
  for (size_t i = 0; i < kPreferredActionsLength; ++i)
    preferred_actions_.insert(kPreferredActions[i]);
  for (size_t i = 0; i < kReservedActionsLength; ++i)
    reserved_actions_.insert(kReservedActions[i]);
  for (size_t i = 0; i < kRepeatableActionsLength; ++i)
    repeatable_actions_.insert(kRepeatableActions[i]);
  for (size_t i = 0; i < kActionsAllowedInAppModeOrPinnedModeLength; ++i) {
    actions_allowed_in_app_mode_.insert(
        kActionsAllowedInAppModeOrPinnedMode[i]);
    actions_allowed_in_pinned_mode_.insert(
        kActionsAllowedInAppModeOrPinnedMode[i]);
  }

  for (size_t i = 0; i < kActionsAllowedInPinnedModeLength; ++i)
    actions_allowed_in_pinned_mode_.insert(kActionsAllowedInPinnedMode[i]);
  for (size_t i = 0; i < kActionsAllowedInAppModeLength; ++i)
    actions_allowed_in_app_mode_.insert(kActionsAllowedInAppMode[i]);
  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i)
    actions_needing_window_.insert(kActionsNeedingWindow[i]);
  for (size_t i = 0; i < kActionsKeepingMenuOpenLength; ++i)
    actions_keeping_menu_open_.insert(kActionsKeepingMenuOpen[i]);

  RegisterAccelerators(accelerator_configuration_->GetAllAccelerators());

  if (debug::DebugAcceleratorsEnabled()) {
    // All debug accelerators are reserved.
    for (size_t i = 0; i < kDebugAcceleratorDataLength; ++i)
      reserved_actions_.insert(kDebugAcceleratorData[i].action);
  }

  if (debug::DeveloperAcceleratorsEnabled()) {
    // Developer accelerators are also reserved.
    for (size_t i = 0; i < kDeveloperAcceleratorDataLength; ++i)
      reserved_actions_.insert(kDeveloperAcceleratorData[i].action);
  }
}

void AcceleratorControllerImpl::RegisterAccelerators(
    base::span<const AcceleratorData> accelerators) {
  std::vector<ui::Accelerator> ui_accelerators;
  ui_accelerators.reserve(accelerators.size());

  for (const auto& accelerator_data : accelerators) {
    ui::Accelerator accelerator =
        CreateAccelerator(accelerator_data.keycode, accelerator_data.modifiers,
                          accelerator_data.trigger_on_press);
    ui_accelerators.push_back(accelerator);
  }
  Register(std::move(ui_accelerators), this);
}

void AcceleratorControllerImpl::RegisterAccelerators(
    std::vector<ui::Accelerator> accelerators) {
  Register(std::move(accelerators), this);
}

bool AcceleratorControllerImpl::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) const {
  const AcceleratorAction* action_ptr =
      accelerator_configuration_->FindAcceleratorAction(accelerator);
  return action_ptr && CanPerformAction(*action_ptr, accelerator);
}

bool AcceleratorControllerImpl::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) const {
  if (accelerator.IsRepeat() && !base::Contains(repeatable_actions_, action))
    return false;

  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return restriction == RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;

  const ui::Accelerator& previous_accelerator =
      accelerator_history_->previous_accelerator();

  // True should be returned if running |action| does something. Otherwise,
  // false should be returned to give the web contents a chance at handling the
  // accelerator.
  switch (action) {
    case AcceleratorAction::kCycleBackwardMru:
    case AcceleratorAction::kCycleForwardMru:
      return accelerators::CanCycleMru();
    case AcceleratorAction::kCycleSameAppWindowsBackward:
    case AcceleratorAction::kCycleSameAppWindowsForward:
      return accelerators::CanCycleSameAppWindows();
    case AcceleratorAction::kDesksActivateDeskLeft:
    case AcceleratorAction::kDesksActivateDeskRight:
    case AcceleratorAction::kDesksMoveActiveItemLeft:
    case AcceleratorAction::kDesksMoveActiveItemRight:
    case AcceleratorAction::kDesksNewDesk:
    case AcceleratorAction::kDesksRemoveCurrentDesk:
    case AcceleratorAction::kDesksActivate0:
    case AcceleratorAction::kDesksActivate1:
    case AcceleratorAction::kDesksActivate2:
    case AcceleratorAction::kDesksActivate3:
    case AcceleratorAction::kDesksActivate4:
    case AcceleratorAction::kDesksActivate5:
    case AcceleratorAction::kDesksActivate6:
    case AcceleratorAction::kDesksActivate7:
    case AcceleratorAction::kDesksToggleAssignToAllDesks:
      return true;
    case AcceleratorAction::kDebugKeyboardBacklightToggle:
    case AcceleratorAction::kDebugMicrophoneMuteToggle:
    case AcceleratorAction::kDebugPrintLayerHierarchy:
    case AcceleratorAction::kDebugPrintViewHierarchy:
    case AcceleratorAction::kDebugPrintWindowHierarchy:
    case AcceleratorAction::kDebugShowToast:
    case AcceleratorAction::kDebugSystemUiStyleViewer:
    case AcceleratorAction::kDebugToggleDarkMode:
    case AcceleratorAction::kDebugToggleDynamicColor:
    case AcceleratorAction::kDebugToggleGlanceables:
    case AcceleratorAction::kDebugTogglePowerButtonMenu:
    case AcceleratorAction::kDebugToggleShowDebugBorders:
    case AcceleratorAction::kDebugToggleShowFpsCounter:
    case AcceleratorAction::kDebugToggleShowPaintRects:
    case AcceleratorAction::kDebugToggleTouchPad:
    case AcceleratorAction::kDebugToggleTouchScreen:
    case AcceleratorAction::kDebugToggleTabletMode:
    case AcceleratorAction::kDebugToggleWallpaperMode:
    case AcceleratorAction::kDebugTriggerCrash:
    case AcceleratorAction::kDebugToggleHudDisplay:
      return debug::DebugAcceleratorsEnabled();
    case AcceleratorAction::kDevAddRemoveDisplay:
    case AcceleratorAction::kDevToggleAppList:
    case AcceleratorAction::kDevToggleUnifiedDesktop:
      return debug::DeveloperAcceleratorsEnabled();
    case AcceleratorAction::kDisableCapsLock:
      return CanHandleDisableCapsLock(previous_accelerator);
    case AcceleratorAction::kLockScreen:
      return accelerators::CanLock();
    case AcceleratorAction::kMagnifierZoomIn:
    case AcceleratorAction::kMagnifierZoomOut:
      return accelerators::CanPerformMagnifierZoom();
    case AcceleratorAction::kMicrophoneMuteToggle:
      return true;
    case AcceleratorAction::kMoveActiveWindowBetweenDisplays:
      return accelerators::CanMoveActiveWindowBetweenDisplays();
    case AcceleratorAction::kNewIncognitoWindow:
      return accelerators::CanCreateNewIncognitoWindow();
    case AcceleratorAction::kPasteClipboardHistoryPlainText:
      return true;
    case AcceleratorAction::kPrivacyScreenToggle:
      return accelerators::CanTogglePrivacyScreen();
    case AcceleratorAction::kRotateScreen:
      return true;
    case AcceleratorAction::kScaleUiDown:
    case AcceleratorAction::kScaleUiReset:
    case AcceleratorAction::kScaleUiUp:
      return true;
    case AcceleratorAction::kShowStylusTools:
      return accelerators::CanShowStylusTools();
    case AcceleratorAction::kStartAssistant:
      return true;
    case AcceleratorAction::kSwapPrimaryDisplay:
      return accelerators::CanSwapPrimaryDisplay();
    case AcceleratorAction::kSwitchIme:
      return CanHandleSwitchIme(accelerator);
    case AcceleratorAction::kSwitchToNextIme:
      return accelerators::CanCycleInputMethod();
    case AcceleratorAction::kSwitchToLastUsedIme:
      return accelerators::CanCycleInputMethod();
    case AcceleratorAction::kSwitchToPreviousUser:
    case AcceleratorAction::kSwitchToNextUser:
      return accelerators::CanCycleUser();
    case AcceleratorAction::kToggleAppList:
      return CanHandleToggleAppList(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case AcceleratorAction::kToggleCalendar:
      return true;
    case AcceleratorAction::kToggleCapsLock:
      return CanHandleToggleCapsLock(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case AcceleratorAction::kToggleClipboardHistory:
      return true;
    case AcceleratorAction::kToggleDictation:
      return accelerators::CanToggleDictation();
    case AcceleratorAction::kToggleDockedMagnifier:
      return true;
    case AcceleratorAction::kToggleFloating:
      return accelerators::CanToggleFloatingWindow();
    case AcceleratorAction::kToggleFullscreenMagnifier:
      return true;
    case AcceleratorAction::kToggleGameDashboard:
      return accelerators::CanToggleGameDashboard();
    case AcceleratorAction::kToggleMessageCenterBubble:
      return true;
    case AcceleratorAction::kToggleMirrorMode:
      return true;
    case AcceleratorAction::kToggleOverview:
      return accelerators::CanToggleOverview();
    case AcceleratorAction::kToggleSnapGroupWindowsGroupAndUngroup:
      return accelerators::CanGroupOrUngroupWindows();
    case AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore:
      return accelerators::CanMinimizeSnapGroupWindows();
    case AcceleratorAction::kToggleMultitaskMenu:
      return accelerators::CanToggleMultitaskMenu();
    case AcceleratorAction::kTouchHudClear:
    case AcceleratorAction::kTouchHudModeChange:
      return accelerators::CanActivateTouchHud();
    case AcceleratorAction::kUnpin:
      return accelerators::CanUnpinWindow();
    case AcceleratorAction::kWindowCycleSnapLeft:
    case AcceleratorAction::kWindowCycleSnapRight:
      return accelerators::CanWindowSnap();
    case AcceleratorAction::kFocusPip:
      return accelerators::CanFindPipWidget();
    case AcceleratorAction::kFocusCameraPreview:
      return accelerators::CanFocusCameraPreview();
    case AcceleratorAction::kMinimizeTopWindowOnBack:
      return accelerators::CanMinimizeTopWindowOnBack();
    case AcceleratorAction::kTakePartialScreenshot:
    case AcceleratorAction::kTakeScreenshot:
    case AcceleratorAction::kTakeWindowScreenshot:
      return accelerators::CanScreenshot(action ==
                                         AcceleratorAction::kTakeScreenshot);
    case AcceleratorAction::kToggleProjectorMarker:
      return accelerators::CanToggleProjectorMarker();
    case AcceleratorAction::kToggleResizeLockMenu:
      return accelerators::CanToggleResizeLockMenu();
    case AcceleratorAction::kDebugTuckFloatedWindowLeft:
    case AcceleratorAction::kDebugTuckFloatedWindowRight:
      return debug::CanTuckFloatedWindow();
    case AcceleratorAction::kDebugToggleVideoConferenceCameraTrayIcon:
      return true;

    // The following are always enabled.
    case AcceleratorAction::kBrightnessDown:
    case AcceleratorAction::kBrightnessUp:
    case AcceleratorAction::kExit:
    case AcceleratorAction::kFocusNextPane:
    case AcceleratorAction::kFocusPreviousPane:
    case AcceleratorAction::kFocusShelf:
    case AcceleratorAction::kKeyboardBacklightToggle:
    case AcceleratorAction::kKeyboardBrightnessDown:
    case AcceleratorAction::kKeyboardBrightnessUp:
    case AcceleratorAction::kLaunchApp0:
    case AcceleratorAction::kLaunchApp1:
    case AcceleratorAction::kLaunchApp2:
    case AcceleratorAction::kLaunchApp3:
    case AcceleratorAction::kLaunchApp4:
    case AcceleratorAction::kLaunchApp5:
    case AcceleratorAction::kLaunchApp6:
    case AcceleratorAction::kLaunchApp7:
    case AcceleratorAction::kLaunchLastApp:
    case AcceleratorAction::kLockPressed:
    case AcceleratorAction::kLockReleased:
    case AcceleratorAction::kMediaFastForward:
    case AcceleratorAction::kMediaNextTrack:
    case AcceleratorAction::kMediaPause:
    case AcceleratorAction::kMediaPlay:
    case AcceleratorAction::kMediaPlayPause:
    case AcceleratorAction::kMediaPrevTrack:
    case AcceleratorAction::kMediaRewind:
    case AcceleratorAction::kMediaStop:
    case AcceleratorAction::kNewTab:
    case AcceleratorAction::kNewWindow:
    case AcceleratorAction::kOpenCalculator:
    case AcceleratorAction::kOpenCrosh:
    case AcceleratorAction::kOpenDiagnostics:
    case AcceleratorAction::kOpenFeedbackPage:
    case AcceleratorAction::kOpenFileManager:
    case AcceleratorAction::kOpenGetHelp:
    case AcceleratorAction::kPowerPressed:
    case AcceleratorAction::kPowerReleased:
    case AcceleratorAction::kPrintUiHierarchies:
    case AcceleratorAction::kRestoreTab:
    case AcceleratorAction::kRotateWindow:
    case AcceleratorAction::kShowEmojiPicker:
    case AcceleratorAction::kToggleImeMenuBubble:
    case AcceleratorAction::kShowShortcutViewer:
    case AcceleratorAction::kShowTaskManager:
    case AcceleratorAction::kSuspend:
    case AcceleratorAction::kToggleFullscreen:
    case AcceleratorAction::kToggleHighContrast:
    case AcceleratorAction::kToggleMaximized:
    case AcceleratorAction::kToggleSpokenFeedback:
    case AcceleratorAction::kToggleSystemTrayBubble:
    case AcceleratorAction::kToggleWifi:
    case AcceleratorAction::kVolumeDown:
    case AcceleratorAction::kVolumeMute:
    case AcceleratorAction::kVolumeUp:
    case AcceleratorAction::kWindowMinimize:
      return true;
    case AcceleratorAction::kTouchFingerprintSensor1:
    case AcceleratorAction::kTouchFingerprintSensor2:
    case AcceleratorAction::kTouchFingerprintSensor3:
      return FakeBiodClient::Get() != nullptr;
  }
}

void AcceleratorControllerImpl::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return;

  if ((action == AcceleratorAction::kVolumeDown ||
       action == AcceleratorAction::kVolumeUp) &&
      Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    if (tablet_volume_controller_.ShouldSwapSideVolumeButtons(
            accelerator.source_device_id()))
      action = action == AcceleratorAction::kVolumeDown
                   ? AcceleratorAction::kVolumeUp
                   : AcceleratorAction::kVolumeDown;

    tablet_volume_controller_.StartTabletModeVolumeAdjustTimer(
        action == AcceleratorAction::kVolumeUp);
  }

  // If your accelerator invokes more than one line of code, please either
  // implement it in your module's controller code or pull it into a HandleFoo()
  // function above.
  switch (action) {
    case AcceleratorAction::kBrightnessDown: {
      base::RecordAction(UserMetricsAction("Accel_BrightnessDown_F6"));
      accelerators::BrightnessDown();
      break;
    }
    case AcceleratorAction::kBrightnessUp: {
      base::RecordAction(UserMetricsAction("Accel_BrightnessUp_F7"));
      accelerators::BrightnessUp();
      break;
    }
    case AcceleratorAction::kCycleBackwardMru:
      RecordCycleBackwardMru(accelerator);
      accelerators::CycleBackwardMru(/*same_app_only=*/false);
      break;
    case AcceleratorAction::kCycleForwardMru:
      RecordCycleForwardMru(accelerator);
      accelerators::CycleForwardMru(/*same_app_only=*/false);
      break;
    case AcceleratorAction::kCycleSameAppWindowsBackward:
      // TODO(b/250699271): Add metrics
      accelerators::CycleBackwardMru(/*same_app_only=*/true);
      break;
    case AcceleratorAction::kCycleSameAppWindowsForward:
      // TODO(b/250699271): Add metrics
      accelerators::CycleForwardMru(/*same_app_only=*/true);
      break;
    case AcceleratorAction::kDesksActivateDeskLeft:
      // UMA metrics are recorded in the function.
      accelerators::ActivateDesk(/*activate_left=*/true);
      break;
    case AcceleratorAction::kDesksActivateDeskRight:
      // UMA metrics are recorded in the function.
      accelerators::ActivateDesk(/*activate_left=*/false);
      break;
    case AcceleratorAction::kDesksMoveActiveItemLeft:
      // UMA metrics are recorded in the function.
      accelerators::MoveActiveItem(/*going_left=*/true);
      break;
    case AcceleratorAction::kDesksMoveActiveItemRight:
      // UMA metrics are recorded in the function.
      accelerators::MoveActiveItem(/*going_left=*/false);
      break;
    case AcceleratorAction::kDesksNewDesk:
      // UMA metrics are recorded in the function.
      accelerators::NewDesk();
      break;
    case AcceleratorAction::kDesksRemoveCurrentDesk:
      // UMA metrics are recorded in the function.
      accelerators::RemoveCurrentDesk();
      break;
    case AcceleratorAction::kDesksActivate0:
    case AcceleratorAction::kDesksActivate1:
    case AcceleratorAction::kDesksActivate2:
    case AcceleratorAction::kDesksActivate3:
    case AcceleratorAction::kDesksActivate4:
    case AcceleratorAction::kDesksActivate5:
    case AcceleratorAction::kDesksActivate6:
    case AcceleratorAction::kDesksActivate7:
      accelerators::ActivateDeskAtIndex(action);
      break;
    case AcceleratorAction::kDesksToggleAssignToAllDesks:
      accelerators::ToggleAssignToAllDesk();
      break;
    case AcceleratorAction::kDebugKeyboardBacklightToggle:
    case AcceleratorAction::kDebugMicrophoneMuteToggle:
    case AcceleratorAction::kDebugPrintLayerHierarchy:
    case AcceleratorAction::kDebugPrintViewHierarchy:
    case AcceleratorAction::kDebugPrintWindowHierarchy:
    case AcceleratorAction::kDebugShowToast:
    case AcceleratorAction::kDebugToggleDarkMode:
    case AcceleratorAction::kDebugToggleDynamicColor:
    case AcceleratorAction::kDebugToggleGlanceables:
    case AcceleratorAction::kDebugTogglePowerButtonMenu:
    case AcceleratorAction::kDebugToggleVideoConferenceCameraTrayIcon:
    case AcceleratorAction::kDebugSystemUiStyleViewer:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case AcceleratorAction::kDebugToggleShowDebugBorders:
      debug::ToggleShowDebugBorders();
      break;
    case AcceleratorAction::kDebugToggleShowFpsCounter:
      debug::ToggleShowFpsCounter();
      break;
    case AcceleratorAction::kDebugToggleShowPaintRects:
      debug::ToggleShowPaintRects();
      break;
    case AcceleratorAction::kDebugToggleTouchPad:
    case AcceleratorAction::kDebugToggleTouchScreen:
    case AcceleratorAction::kDebugToggleTabletMode:
    case AcceleratorAction::kDebugToggleWallpaperMode:
    case AcceleratorAction::kDebugTriggerCrash:
    case AcceleratorAction::kDebugToggleHudDisplay:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case AcceleratorAction::kDevAddRemoveDisplay:
      Shell::Get()->display_manager()->AddRemoveDisplay();
      break;
    case AcceleratorAction::kDevToggleAppList:
      RecordToggleAppList(accelerator);
      accelerators::ToggleAppList(AppListShowSource::kSearchKey,
                                  base::TimeTicks());
      break;
    case AcceleratorAction::kDevToggleUnifiedDesktop:
      accelerators::ToggleUnifiedDesktop();
      break;
    case AcceleratorAction::kDisableCapsLock:
      base::RecordAction(base::UserMetricsAction("Accel_Disable_Caps_Lock"));
      accelerators::DisableCapsLock();
      break;
    case AcceleratorAction::kExit:
      // UMA metrics are recorded in the handler.
      exit_warning_handler_.HandleAccelerator();
      break;
    case AcceleratorAction::kFocusNextPane:
      base::RecordAction(UserMetricsAction("Accel_Focus_Next_Pane"));
      accelerators::RotatePaneFocus(FocusCycler::FORWARD);
      break;
    case AcceleratorAction::kFocusPreviousPane:
      base::RecordAction(UserMetricsAction("Accel_Focus_Previous_Pane"));
      accelerators::RotatePaneFocus(FocusCycler::BACKWARD);
      break;
    case AcceleratorAction::kFocusShelf:
      base::RecordAction(UserMetricsAction("Accel_Focus_Shelf"));
      accelerators::FocusShelf();
      break;
    case AcceleratorAction::kFocusCameraPreview:
      accelerators::FocusCameraPreview();
      break;
    case AcceleratorAction::kFocusPip:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Pip"));
      accelerators::FocusPip();
      break;
    case AcceleratorAction::kKeyboardBacklightToggle:
      if (ash::features::IsKeyboardBacklightToggleEnabled()) {
        base::RecordAction(base::UserMetricsAction("Accel_Keyboard_Backlight"));
        accelerators::ToggleKeyboardBacklight();
      }
      break;
    case AcceleratorAction::kKeyboardBrightnessDown: {
      base::RecordAction(UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));
      accelerators::KeyboardBrightnessDown();
      break;
    }
    case AcceleratorAction::kKeyboardBrightnessUp: {
      base::RecordAction(UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));
      accelerators::KeyboardBrightnessUp();
      break;
    }
    case AcceleratorAction::kLaunchApp0:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(0);
      break;
    case AcceleratorAction::kLaunchApp1:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(1);
      break;
    case AcceleratorAction::kLaunchApp2:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(2);
      break;
    case AcceleratorAction::kLaunchApp3:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(3);
      break;
    case AcceleratorAction::kLaunchApp4:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(4);
      break;
    case AcceleratorAction::kLaunchApp5:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(5);
      break;
    case AcceleratorAction::kLaunchApp6:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(6);
      break;
    case AcceleratorAction::kLaunchApp7:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(7);
      break;
    case AcceleratorAction::kLaunchLastApp:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_Last_App"));
      accelerators::LaunchLastApp();
      break;
    case AcceleratorAction::kLockPressed:
    case AcceleratorAction::kLockReleased:
      accelerators::LockPressed(action == AcceleratorAction::kLockPressed);
      break;
    case AcceleratorAction::kLockScreen:
      base::RecordAction(base::UserMetricsAction("Accel_LockScreen_L"));
      accelerators::LockScreen();
      break;
    case AcceleratorAction::kMagnifierZoomIn:
      accelerators::ActiveMagnifierZoom(1);
      break;
    case AcceleratorAction::kMagnifierZoomOut:
      accelerators::ActiveMagnifierZoom(-1);
      break;
    case AcceleratorAction::kMediaFastForward:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Fast_Forward"));
      accelerators::MediaFastForward();
      break;
    case AcceleratorAction::kMediaNextTrack:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Next_Track"));
      accelerators::MediaNextTrack();
      break;
    case AcceleratorAction::kMediaPause:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Pause"));
      accelerators::MediaPause();
      break;
    case AcceleratorAction::kMediaPlay:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Play"));
      accelerators::MediaPlay();
      break;
    case AcceleratorAction::kMediaPlayPause:
      base::RecordAction(base::UserMetricsAction("Accel_Media_PlayPause"));
      accelerators::MediaPlayPause();
      break;
    case AcceleratorAction::kMediaPrevTrack:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Prev_Track"));
      accelerators::MediaPrevTrack();
      break;
    case AcceleratorAction::kMediaRewind:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Rewind"));
      accelerators::MediaRewind();
      break;
    case AcceleratorAction::kMediaStop:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Stop"));
      accelerators::MediaStop();
      break;
    case AcceleratorAction::kMicrophoneMuteToggle:
      base::RecordAction(base::UserMetricsAction("Accel_Microphone_Mute"));
      accelerators::MicrophoneMuteToggle();
      break;
    case AcceleratorAction::kMoveActiveWindowBetweenDisplays:
      accelerators::MoveActiveWindowBetweenDisplays();
      break;
    case AcceleratorAction::kNewIncognitoWindow:
      base::RecordAction(base::UserMetricsAction("Accel_New_Incognito_Window"));
      accelerators::NewIncognitoWindow();
      break;
    case AcceleratorAction::kNewTab:
      RecordNewTab(accelerator);
      accelerators::NewTab();
      break;
    case AcceleratorAction::kNewWindow:
      base::RecordAction(base::UserMetricsAction("Accel_New_Window"));
      accelerators::NewWindow();
      break;
    case AcceleratorAction::kOpenCalculator:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Calculator"));
      accelerators::OpenCalculator();
      break;
    case AcceleratorAction::kOpenCrosh:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Crosh"));
      accelerators::OpenCrosh();
      break;
    case AcceleratorAction::kOpenDiagnostics:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Diagnostics"));
      accelerators::OpenDiagnostics();
      break;
    case AcceleratorAction::kOpenFeedbackPage:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Feedback_Page"));
      accelerators::OpenFeedbackPage();
      break;
    case AcceleratorAction::kOpenFileManager:
      base::RecordAction(base::UserMetricsAction("Accel_Open_File_Manager"));
      accelerators::OpenFileManager();
      break;
    case AcceleratorAction::kOpenGetHelp:
      accelerators::OpenHelp();
      break;
    case AcceleratorAction::kPasteClipboardHistoryPlainText:
      accelerators::ToggleClipboardHistory(/*is_plain_text_paste=*/true);
      break;
    case AcceleratorAction::kPowerPressed:
    case AcceleratorAction::kPowerReleased:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd, the Chrome OS power manager, in linux desktop,
        // so call the PowerButtonController here.
        accelerators::PowerPressed(action == AcceleratorAction::kPowerPressed);
      }
      // We don't do anything with these at present on the device,
      // (power button events are reported to us from powerm via
      // D-BUS), but we consume them to prevent them from getting
      // passed to apps -- see http://crbug.com/146609.
      break;
    case AcceleratorAction::kPrintUiHierarchies:
      debug::PrintUIHierarchies();
      break;
    case AcceleratorAction::kPrivacyScreenToggle:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Privacy_Screen"));
      accelerators::TogglePrivacyScreen();
      break;
    case AcceleratorAction::kRotateScreen:
      accelerators::RotateScreen();
      break;
    case AcceleratorAction::kRestoreTab:
      base::RecordAction(base::UserMetricsAction("Accel_Restore_Tab"));
      accelerators::RestoreTab();
      break;
    case AcceleratorAction::kRotateWindow:
      base::RecordAction(UserMetricsAction("Accel_Rotate_Active_Window"));
      accelerators::RotateActiveWindow();
      break;
    case AcceleratorAction::kScaleUiDown:
      accelerators::ZoomDisplay(false /* down */);
      break;
    case AcceleratorAction::kScaleUiReset:
      accelerators::ResetDisplayZoom();
      break;
    case AcceleratorAction::kScaleUiUp:
      accelerators::ZoomDisplay(true /* up */);
      break;
    case AcceleratorAction::kShowEmojiPicker:
      base::RecordAction(UserMetricsAction("Accel_Show_Emoji_Picker"));
      accelerators::ShowEmojiPicker();
      break;
    case AcceleratorAction::kToggleImeMenuBubble:
      base::RecordAction(UserMetricsAction("Accel_Show_Ime_Menu_Bubble"));
      accelerators::ToggleImeMenuBubble();
      break;
    case AcceleratorAction::kToggleProjectorMarker:
      accelerators::ToggleProjectorMarker();
      break;
    case AcceleratorAction::kShowShortcutViewer:
      if (features::ShouldOnlyShowNewShortcutApp()) {
        accelerators::ShowShortcutCustomizationApp();
      } else {
        accelerators::ShowKeyboardShortcutViewer();
      }
      break;
    case AcceleratorAction::kShowStylusTools:
      base::RecordAction(UserMetricsAction("Accel_Show_Stylus_Tools"));
      accelerators::ShowStylusTools();
      break;
    case AcceleratorAction::kShowTaskManager:
      base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
      accelerators::ShowTaskManager();
      break;
    case AcceleratorAction::kStartAssistant:
      // TODO(longbowei): Move this to CanToggleAssistant().
      if (ShouldToggleAssistant(accelerator)) {
        RecordToggleAssistant(accelerator);
        accelerators::ToggleAssistant();
      }
      break;
    case AcceleratorAction::kSuspend:
      base::RecordAction(UserMetricsAction("Accel_Suspend"));
      accelerators::Suspend();
      break;
    case AcceleratorAction::kSwapPrimaryDisplay:
      base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));
      accelerators::ShiftPrimaryDisplay();
      break;
    case AcceleratorAction::kSwitchIme:
      HandleSwitchIme(accelerator);
      break;
    case AcceleratorAction::kSwitchToLastUsedIme:
      HandleSwitchToLastUsedIme(accelerator);
      break;
    case AcceleratorAction::kSwitchToNextIme:
      RecordSwitchToNextIme(accelerator);
      accelerators::SwitchToNextIme();
      break;
    case AcceleratorAction::kSwitchToNextUser:
      MultiProfileUMA::RecordSwitchActiveUser(
          MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Next_User"));
      accelerators::CycleUser(CycleUserDirection::NEXT);
      break;
    case AcceleratorAction::kSwitchToPreviousUser:
      MultiProfileUMA::RecordSwitchActiveUser(
          MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Previous_User"));
      accelerators::CycleUser(CycleUserDirection::PREVIOUS);
      break;
    case AcceleratorAction::kTakePartialScreenshot:
      // UMA metrics are recorded in the function.
      accelerators::MaybeTakePartialScreenshot();
      break;
    case AcceleratorAction::kTakeScreenshot:
      base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
      accelerators::TakeScreenshot(accelerator.key_code() == ui::VKEY_SNAPSHOT);
      break;
    case AcceleratorAction::kTakeWindowScreenshot:
      // UMA metrics are recorded in the function.
      accelerators::MaybeTakeWindowScreenshot();
      break;
    case AcceleratorAction::kToggleAppList: {
      RecordToggleAppList(accelerator);
      accelerators::ToggleAppList(AppListShowSource::kSearchKey,
                                  base::TimeTicks());
      break;
    }
    case AcceleratorAction::kToggleCalendar:
      accelerators::ToggleCalendar();
      break;
    case AcceleratorAction::kToggleCapsLock:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Caps_Lock"));
      accelerators::ToggleCapsLock();
      break;
    case AcceleratorAction::kToggleClipboardHistory:
      accelerators::ToggleClipboardHistory(/*is_plain_text_paste=*/false);
      break;
    case AcceleratorAction::kToggleDictation:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Dictation"));
      accelerators::ToggleDictation();
      break;
    case AcceleratorAction::kToggleDockedMagnifier:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Docked_Magnifier"));
      accelerators::ToggleDockedMagnifier();
      break;
    case AcceleratorAction::kDebugTuckFloatedWindowLeft:
    case AcceleratorAction::kDebugTuckFloatedWindowRight:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case AcceleratorAction::kToggleFloating:
      // UMA metrics are recorded in the function.
      accelerators::ToggleFloating();
      break;
    case AcceleratorAction::kToggleFullscreen:
      RecordToggleFullscreen(accelerator);
      accelerators::ToggleFullscreen();
      break;
    case AcceleratorAction::kToggleFullscreenMagnifier:
      base::RecordAction(
          UserMetricsAction("Accel_Toggle_Fullscreen_Magnifier"));
      accelerators::ToggleFullscreenMagnifier();
      break;
    case AcceleratorAction::kToggleGameDashboard:
      accelerators::ToggleGameDashboard();
      break;
    case AcceleratorAction::kToggleHighContrast:
      base::RecordAction(UserMetricsAction("Accel_Toggle_High_Contrast"));
      accelerators::ToggleHighContrast();
      break;
    case AcceleratorAction::kToggleMaximized:
      accelerators::ToggleMaximized();
      break;
    case AcceleratorAction::kToggleMessageCenterBubble:
      base::RecordAction(
          UserMetricsAction("Accel_Toggle_Message_Center_Bubble"));
      accelerators::ToggleMessageCenterBubble();
      break;
    case AcceleratorAction::kToggleMirrorMode:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Mirror_Mode"));
      accelerators::ToggleMirrorMode();
      break;
    case AcceleratorAction::kToggleMultitaskMenu:
      accelerators::ToggleMultitaskMenu();
      return;
    case AcceleratorAction::kToggleOverview:
      base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
      accelerators::ToggleOverview();
      break;
    case AcceleratorAction::kToggleSnapGroupWindowsGroupAndUngroup:
      accelerators::GroupOrUngroupWindowsInSnapGroup();
      break;
    case AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore:
      base::RecordAction(base::UserMetricsAction(
          "Accel_Toggle_Snap_Group_Windows_Minimize_Restore"));
      accelerators::MinimizeWindowsInSnapGroup();
      break;
    case AcceleratorAction::kToggleResizeLockMenu:
      base::RecordAction(
          base::UserMetricsAction("Accel_Toggle_Resize_Lock_Menu"));
      accelerators::ToggleResizeLockMenu();
      break;
    case AcceleratorAction::kToggleSpokenFeedback:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));
      accelerators::ToggleSpokenFeedback();
      break;
    case AcceleratorAction::kToggleSystemTrayBubble:
      base::RecordAction(UserMetricsAction("Accel_Toggle_System_Tray_Bubble"));
      accelerators::ToggleSystemTrayBubble();
      break;
    case AcceleratorAction::kToggleWifi:
      accelerators::ToggleWifi();
      break;
    case AcceleratorAction::kTouchHudClear:
      accelerators::TouchHudClear();
      break;
    case AcceleratorAction::kTouchHudModeChange:
      accelerators::TouchHudModeChange();
      break;
    case AcceleratorAction::kUnpin:
      accelerators::UnpinWindow();
      break;
    case AcceleratorAction::kVolumeDown:
      base::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));
      output_volume_metric_delay_timer_.Reset();
      accelerators::VolumeDown();
      break;
    case AcceleratorAction::kVolumeMute:
      if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
        base::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));
      accelerators::VolumeMute();
      break;
    case AcceleratorAction::kVolumeUp:
      base::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));
      output_volume_metric_delay_timer_.Reset();
      accelerators::VolumeUp();
      break;
    case AcceleratorAction::kWindowCycleSnapLeft:
      base::RecordAction(UserMetricsAction("Accel_Window_Snap_Left"));
      accelerators::WindowSnap(AcceleratorAction::kWindowCycleSnapLeft);
      break;
    case AcceleratorAction::kWindowCycleSnapRight:
      base::RecordAction(UserMetricsAction("Accel_Window_Snap_Right"));
      accelerators::WindowSnap(AcceleratorAction::kWindowCycleSnapRight);
      break;
    case AcceleratorAction::kWindowMinimize:
      base::RecordAction(
          base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
      accelerators::WindowMinimize();
      break;
    case AcceleratorAction::kMinimizeTopWindowOnBack:
      base::RecordAction(
          base::UserMetricsAction("Accel_Minimize_Top_Window_On_Back"));
      accelerators::TopWindowMinimizeOnBack();
      break;
    case kTouchFingerprintSensor1:
      accelerators::TouchFingerprintSensor(1);
      break;
    case kTouchFingerprintSensor2:
      accelerators::TouchFingerprintSensor(2);
      break;
    case kTouchFingerprintSensor3:
      accelerators::TouchFingerprintSensor(3);
      break;
  }

  RecordActionUmaHistogram(action, accelerator);
  NotifyActionPerformed(action);

  // Reset any in progress composition.
  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    auto* input_method =
        Shell::Get()->window_tree_host_manager()->input_method();

    input_method->CancelComposition(input_method->GetTextInputClient());
  }
}

bool AcceleratorControllerImpl::ShouldActionConsumeKeyEvent(
    AcceleratorAction action) {
  // Adding new exceptions is *STRONGLY* discouraged.
  return true;
}

AcceleratorControllerImpl::AcceleratorProcessingRestriction
AcceleratorControllerImpl::GetAcceleratorProcessingRestriction(
    int action) const {
  if (ShouldPreventProcessingAccelerators()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->screen_pinning_controller()->IsPinned() &&
      !base::Contains(actions_allowed_in_pinned_mode_, action)) {
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      !base::Contains(actions_allowed_at_login_screen_, action)) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      !base::Contains(actions_allowed_at_lock_screen_, action)) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->power_button_controller()->IsMenuOpened() &&
      !base::Contains(actions_allowed_at_power_menu_, action)) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->session_controller()->IsRunningInAppMode() &&
      !base::Contains(actions_allowed_in_app_mode_, action)) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::IsSystemModalWindowOpen() &&
      !base::Contains(actions_allowed_at_modal_window_, action)) {
    // Note we prevent the shortcut from propagating so it will not
    // be passed to the modal window. This is important for things like
    // Alt+Tab that would cause an undesired effect in the modal window by
    // cycling through its window elements.
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  if (base::Contains(actions_needing_window_, action) &&
      Shell::Get()
          ->mru_window_tracker()
          ->BuildMruWindowList(kActiveDesk)
          .empty()) {
    Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
        AccessibilityAlert::WINDOW_NEEDED);
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  return RESTRICTION_NONE;
}

AcceleratorControllerImpl::AcceleratorProcessingStatus
AcceleratorControllerImpl::MaybeDeprecatedAcceleratorPressed(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) const {
  const DeprecatedAcceleratorData* deprecated_data =
      accelerator_configuration_->GetDeprecatedAcceleratorData(action);
  if (!deprecated_data) {
    // The action is not associated with any deprecated accelerators, and hence
    // should be performed normally.
    return AcceleratorProcessingStatus::PROCEED;
  }

  // This action is associated with new and deprecated accelerators, find which
  // one is |accelerator|.
  if (!accelerator_configuration_->IsDeprecated(accelerator)) {
    // This is a new accelerator replacing the old deprecated one.
    // Record UMA stats and proceed normally to perform it.
    RecordUmaHistogram(deprecated_data->uma_histogram_name, NEW_USED);
    return AcceleratorProcessingStatus::PROCEED;
  }

  // This accelerator has been deprecated and should be treated according
  // to its |DeprecatedAcceleratorData|.

  // Record UMA stats.
  RecordUmaHistogram(deprecated_data->uma_histogram_name, DEPRECATED_USED);

  // We always display the notification as long as this |data| entry exists.
  ShowDeprecatedAcceleratorNotification(
      deprecated_data->uma_histogram_name,
      deprecated_data->notification_message_id,
      deprecated_data->old_shortcut_id, deprecated_data->new_shortcut_id);

  if (!deprecated_data->deprecated_enabled)
    return AcceleratorProcessingStatus::STOP;

  return AcceleratorProcessingStatus::PROCEED;
}

void AcceleratorControllerImpl::SetPreventProcessingAccelerators(
    bool prevent_processing_accelerators) {
  prevent_processing_accelerators_ = prevent_processing_accelerators;
}

bool AcceleratorControllerImpl::ShouldPreventProcessingAccelerators() const {
  return prevent_processing_accelerators_;
}

void AcceleratorControllerImpl::RecordVolumeSource() {
  accelerators::RecordVolumeSource();
}

}  // namespace ash
