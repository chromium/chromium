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
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/devicetype.h"
#include "ash/debug.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/multi_profile_uma.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/aura/env.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {
namespace {

using ::base::UserMetricsAction;
using ::chromeos::WindowStateType;
using input_method::InputMethodManager;

static_assert(DESKS_ACTIVATE_0 == DESKS_ACTIVATE_1 - 1 &&
                  DESKS_ACTIVATE_1 == DESKS_ACTIVATE_2 - 1 &&
                  DESKS_ACTIVATE_2 == DESKS_ACTIVATE_3 - 1 &&
                  DESKS_ACTIVATE_3 == DESKS_ACTIVATE_4 - 1 &&
                  DESKS_ACTIVATE_4 == DESKS_ACTIVATE_5 - 1 &&
                  DESKS_ACTIVATE_5 == DESKS_ACTIVATE_6 - 1 &&
                  DESKS_ACTIVATE_6 == DESKS_ACTIVATE_7 - 1,
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
    case CYCLE_BACKWARD_MRU:
    case CYCLE_FORWARD_MRU:
      return accelerators::CanCycleMru();
    case CYCLE_SAME_APP_WINDOWS_BACKWARD:
    case CYCLE_SAME_APP_WINDOWS_FORWARD:
      return accelerators::CanCycleSameAppWindows();
    case DESKS_ACTIVATE_DESK_LEFT:
    case DESKS_ACTIVATE_DESK_RIGHT:
    case DESKS_MOVE_ACTIVE_ITEM_LEFT:
    case DESKS_MOVE_ACTIVE_ITEM_RIGHT:
    case DESKS_NEW_DESK:
    case DESKS_REMOVE_CURRENT_DESK:
    case DESKS_ACTIVATE_0:
    case DESKS_ACTIVATE_1:
    case DESKS_ACTIVATE_2:
    case DESKS_ACTIVATE_3:
    case DESKS_ACTIVATE_4:
    case DESKS_ACTIVATE_5:
    case DESKS_ACTIVATE_6:
    case DESKS_ACTIVATE_7:
    case DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS:
      return true;
    case DEBUG_KEYBOARD_BACKLIGHT_TOGGLE:
    case DEBUG_MICROPHONE_MUTE_TOGGLE:
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
    case DEBUG_SYSTEM_UI_STYLE_VIEWER:
    case DEBUG_TOGGLE_DARK_MODE:
    case DEBUG_TOGGLE_DYNAMIC_COLOR:
    case DEBUG_TOGGLE_GLANCEABLES:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case DEBUG_TOGGLE_TOUCH_PAD:
    case DEBUG_TOGGLE_TOUCH_SCREEN:
    case DEBUG_TOGGLE_TABLET_MODE:
    case DEBUG_TOGGLE_WALLPAPER_MODE:
    case DEBUG_TRIGGER_CRASH:
    case DEBUG_TOGGLE_HUD_DISPLAY:
      return debug::DebugAcceleratorsEnabled();
    case DEV_ADD_REMOVE_DISPLAY:
    case DEV_TOGGLE_APP_LIST:
    case DEV_TOGGLE_UNIFIED_DESKTOP:
      return debug::DeveloperAcceleratorsEnabled();
    case DISABLE_CAPS_LOCK:
      return CanHandleDisableCapsLock(previous_accelerator);
    case LOCK_SCREEN:
      return accelerators::CanLock();
    case MAGNIFIER_ZOOM_IN:
    case MAGNIFIER_ZOOM_OUT:
      return accelerators::CanPerformMagnifierZoom();
    case MICROPHONE_MUTE_TOGGLE:
      return true;
    case MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS:
      return accelerators::CanMoveActiveWindowBetweenDisplays();
    case NEW_INCOGNITO_WINDOW:
      return accelerators::CanCreateNewIncognitoWindow();
    case PASTE_CLIPBOARD_HISTORY_PLAIN_TEXT:
      return true;
    case PRIVACY_SCREEN_TOGGLE:
      return accelerators::CanTogglePrivacyScreen();
    case ROTATE_SCREEN:
      return true;
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
      return true;
    case SHOW_STYLUS_TOOLS:
      return accelerators::CanShowStylusTools();
    case START_AMBIENT_MODE:
      return accelerators::CanStartAmbientMode();
    case START_ASSISTANT:
      return true;
    case SWAP_PRIMARY_DISPLAY:
      return accelerators::CanSwapPrimaryDisplay();
    case SWITCH_IME:
      return CanHandleSwitchIme(accelerator);
    case SWITCH_TO_NEXT_IME:
      return accelerators::CanCycleInputMethod();
    case SWITCH_TO_LAST_USED_IME:
      return accelerators::CanCycleInputMethod();
    case SWITCH_TO_PREVIOUS_USER:
    case SWITCH_TO_NEXT_USER:
      return accelerators::CanCycleUser();
    case TOGGLE_APP_LIST:
      return CanHandleToggleAppList(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case TOGGLE_CALENDAR:
      return true;
    case TOGGLE_CAPS_LOCK:
      return CanHandleToggleCapsLock(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case TOGGLE_CLIPBOARD_HISTORY:
      return true;
    case TOGGLE_DICTATION:
      return accelerators::CanToggleDictation();
    case TOGGLE_DOCKED_MAGNIFIER:
      return true;
    case TOGGLE_FLOATING:
      return accelerators::CanToggleFloatingWindow();
    case TOGGLE_FULLSCREEN_MAGNIFIER:
      return true;
    case TOGGLE_GAME_DASHBOARD:
      return accelerators::CanToggleGameDashboard();
    case TOGGLE_MESSAGE_CENTER_BUBBLE:
      return true;
    case TOGGLE_MIRROR_MODE:
      return true;
    case TOGGLE_OVERVIEW:
      return accelerators::CanToggleOverview();
    case TOGGLE_MULTITASK_MENU:
      return accelerators::CanToggleMultitaskMenu();
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      return accelerators::CanActivateTouchHud();
    case UNPIN:
      return accelerators::CanUnpinWindow();
    case WINDOW_CYCLE_SNAP_LEFT:
    case WINDOW_CYCLE_SNAP_RIGHT:
      return accelerators::CanWindowSnap();
    case FOCUS_PIP:
      return accelerators::CanFindPipWidget();
    case FOCUS_CAMERA_PREVIEW:
      return accelerators::CanFocusCameraPreview();
    case MINIMIZE_TOP_WINDOW_ON_BACK:
      return accelerators::CanMinimizeTopWindowOnBack();
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
      return accelerators::CanScreenshot(action == TAKE_SCREENSHOT);
    case TOGGLE_PROJECTOR_MARKER:
      return accelerators::CanToggleProjectorMarker();
    case TOGGLE_RESIZE_LOCK_MENU:
      return accelerators::CanToggleResizeLockMenu();
    case DEBUG_TUCK_FLOATED_WINDOW_LEFT:
    case DEBUG_TUCK_FLOATED_WINDOW_RIGHT:
      return debug::CanTuckFloatedWindow();
    case DEBUG_TOGGLE_VIDEO_CONFERENCE_CAMERA_TRAY_ICON:
      return true;

    // The following are always enabled.
    case BRIGHTNESS_DOWN:
    case BRIGHTNESS_UP:
    case EXIT:
    case FOCUS_NEXT_PANE:
    case FOCUS_PREVIOUS_PANE:
    case FOCUS_SHELF:
    case KEYBOARD_BACKLIGHT_TOGGLE:
    case KEYBOARD_BRIGHTNESS_DOWN:
    case KEYBOARD_BRIGHTNESS_UP:
    case LAUNCH_APP_0:
    case LAUNCH_APP_1:
    case LAUNCH_APP_2:
    case LAUNCH_APP_3:
    case LAUNCH_APP_4:
    case LAUNCH_APP_5:
    case LAUNCH_APP_6:
    case LAUNCH_APP_7:
    case LAUNCH_LAST_APP:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case MEDIA_FAST_FORWARD:
    case MEDIA_NEXT_TRACK:
    case MEDIA_PAUSE:
    case MEDIA_PLAY:
    case MEDIA_PLAY_PAUSE:
    case MEDIA_PREV_TRACK:
    case MEDIA_REWIND:
    case MEDIA_STOP:
    case NEW_TAB:
    case NEW_WINDOW:
    case OPEN_CALCULATOR:
    case OPEN_CROSH:
    case OPEN_DIAGNOSTICS:
    case OPEN_FEEDBACK_PAGE:
    case OPEN_FILE_MANAGER:
    case OPEN_GET_HELP:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case PRINT_UI_HIERARCHIES:
    case RESTORE_TAB:
    case ROTATE_WINDOW:
    case SHOW_EMOJI_PICKER:
    case TOGGLE_IME_MENU_BUBBLE:
    case SHOW_SHORTCUT_VIEWER:
    case SHOW_TASK_MANAGER:
    case SUSPEND:
    case TOGGLE_FULLSCREEN:
    case TOGGLE_HIGH_CONTRAST:
    case TOGGLE_MAXIMIZED:
    case TOGGLE_SPOKEN_FEEDBACK:
    case TOGGLE_SYSTEM_TRAY_BUBBLE:
    case TOGGLE_WIFI:
    case VOLUME_DOWN:
    case VOLUME_MUTE:
    case VOLUME_UP:
    case WINDOW_MINIMIZE:
      return true;
  }
}

void AcceleratorControllerImpl::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return;

  if ((action == VOLUME_DOWN || action == VOLUME_UP) &&
      Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    if (tablet_volume_controller_.ShouldSwapSideVolumeButtons(
            accelerator.source_device_id()))
      action = action == VOLUME_DOWN ? VOLUME_UP : VOLUME_DOWN;

    tablet_volume_controller_.StartTabletModeVolumeAdjustTimer(action ==
                                                               VOLUME_UP);
  }

  // If your accelerator invokes more than one line of code, please either
  // implement it in your module's controller code or pull it into a HandleFoo()
  // function above.
  switch (action) {
    case BRIGHTNESS_DOWN: {
      base::RecordAction(UserMetricsAction("Accel_BrightnessDown_F6"));
      accelerators::BrightnessDown();
      break;
    }
    case BRIGHTNESS_UP: {
      base::RecordAction(UserMetricsAction("Accel_BrightnessUp_F7"));
      accelerators::BrightnessUp();
      break;
    }
    case CYCLE_BACKWARD_MRU:
      RecordCycleBackwardMru(accelerator);
      accelerators::CycleBackwardMru(/*same_app_only=*/false);
      break;
    case CYCLE_FORWARD_MRU:
      RecordCycleForwardMru(accelerator);
      accelerators::CycleForwardMru(/*same_app_only=*/false);
      break;
    case CYCLE_SAME_APP_WINDOWS_BACKWARD:
      // TODO(b/250699271): Add metrics
      accelerators::CycleBackwardMru(/*same_app_only=*/true);
      break;
    case CYCLE_SAME_APP_WINDOWS_FORWARD:
      // TODO(b/250699271): Add metrics
      accelerators::CycleForwardMru(/*same_app_only=*/true);
      break;
    case DESKS_ACTIVATE_DESK_LEFT:
      // UMA metrics are recorded in the function.
      accelerators::ActivateDesk(/*activate_left=*/true);
      break;
    case DESKS_ACTIVATE_DESK_RIGHT:
      // UMA metrics are recorded in the function.
      accelerators::ActivateDesk(/*activate_left=*/false);
      break;
    case DESKS_MOVE_ACTIVE_ITEM_LEFT:
      // UMA metrics are recorded in the function.
      accelerators::MoveActiveItem(/*going_left=*/true);
      break;
    case DESKS_MOVE_ACTIVE_ITEM_RIGHT:
      // UMA metrics are recorded in the function.
      accelerators::MoveActiveItem(/*going_left=*/false);
      break;
    case DESKS_NEW_DESK:
      // UMA metrics are recorded in the function.
      accelerators::NewDesk();
      break;
    case DESKS_REMOVE_CURRENT_DESK:
      // UMA metrics are recorded in the function.
      accelerators::RemoveCurrentDesk();
      break;
    case DESKS_ACTIVATE_0:
    case DESKS_ACTIVATE_1:
    case DESKS_ACTIVATE_2:
    case DESKS_ACTIVATE_3:
    case DESKS_ACTIVATE_4:
    case DESKS_ACTIVATE_5:
    case DESKS_ACTIVATE_6:
    case DESKS_ACTIVATE_7:
      accelerators::ActivateDeskAtIndex(action);
      break;
    case DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS:
      accelerators::ToggleAssignToAllDesk();
      break;
    case DEBUG_KEYBOARD_BACKLIGHT_TOGGLE:
    case DEBUG_MICROPHONE_MUTE_TOGGLE:
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
    case DEBUG_TOGGLE_DARK_MODE:
    case DEBUG_TOGGLE_DYNAMIC_COLOR:
    case DEBUG_TOGGLE_GLANCEABLES:
    case DEBUG_TOGGLE_VIDEO_CONFERENCE_CAMERA_TRAY_ICON:
    case DEBUG_SYSTEM_UI_STYLE_VIEWER:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
      debug::ToggleShowDebugBorders();
      break;
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
      debug::ToggleShowFpsCounter();
      break;
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      debug::ToggleShowPaintRects();
      break;
    case DEBUG_TOGGLE_TOUCH_PAD:
    case DEBUG_TOGGLE_TOUCH_SCREEN:
    case DEBUG_TOGGLE_TABLET_MODE:
    case DEBUG_TOGGLE_WALLPAPER_MODE:
    case DEBUG_TRIGGER_CRASH:
    case DEBUG_TOGGLE_HUD_DISPLAY:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case DEV_ADD_REMOVE_DISPLAY:
      Shell::Get()->display_manager()->AddRemoveDisplay();
      break;
    case DEV_TOGGLE_APP_LIST:
      RecordToggleAppList(accelerator);
      accelerators::ToggleAppList(AppListShowSource::kSearchKey,
                                  base::TimeTicks());
      break;
    case DEV_TOGGLE_UNIFIED_DESKTOP:
      accelerators::ToggleUnifiedDesktop();
      break;
    case DISABLE_CAPS_LOCK:
      base::RecordAction(base::UserMetricsAction("Accel_Disable_Caps_Lock"));
      accelerators::DisableCapsLock();
      break;
    case EXIT:
      // UMA metrics are recorded in the handler.
      exit_warning_handler_.HandleAccelerator();
      break;
    case FOCUS_NEXT_PANE:
      base::RecordAction(UserMetricsAction("Accel_Focus_Next_Pane"));
      accelerators::RotatePaneFocus(FocusCycler::FORWARD);
      break;
    case FOCUS_PREVIOUS_PANE:
      base::RecordAction(UserMetricsAction("Accel_Focus_Previous_Pane"));
      accelerators::RotatePaneFocus(FocusCycler::BACKWARD);
      break;
    case FOCUS_SHELF:
      base::RecordAction(UserMetricsAction("Accel_Focus_Shelf"));
      accelerators::FocusShelf();
      break;
    case FOCUS_CAMERA_PREVIEW:
      accelerators::FocusCameraPreview();
      break;
    case FOCUS_PIP:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Pip"));
      accelerators::FocusPip();
      break;
    case KEYBOARD_BACKLIGHT_TOGGLE:
      if (ash::features::IsKeyboardBacklightToggleEnabled()) {
        base::RecordAction(base::UserMetricsAction("Accel_Keyboard_Backlight"));
        accelerators::ToggleKeyboardBacklight();
      }
      break;
    case KEYBOARD_BRIGHTNESS_DOWN: {
      base::RecordAction(UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));
      accelerators::KeyboardBrightnessDown();
      break;
    }
    case KEYBOARD_BRIGHTNESS_UP: {
      base::RecordAction(UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));
      accelerators::KeyboardBrightnessUp();
      break;
    }
    case LAUNCH_APP_0:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(0);
      break;
    case LAUNCH_APP_1:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(1);
      break;
    case LAUNCH_APP_2:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(2);
      break;
    case LAUNCH_APP_3:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(3);
      break;
    case LAUNCH_APP_4:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(4);
      break;
    case LAUNCH_APP_5:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(5);
      break;
    case LAUNCH_APP_6:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(6);
      break;
    case LAUNCH_APP_7:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_App"));
      accelerators::LaunchAppN(7);
      break;
    case LAUNCH_LAST_APP:
      base::RecordAction(base::UserMetricsAction("Accel_Launch_Last_App"));
      accelerators::LaunchLastApp();
      break;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
      accelerators::LockPressed(action == LOCK_PRESSED);
      break;
    case LOCK_SCREEN:
      base::RecordAction(base::UserMetricsAction("Accel_LockScreen_L"));
      accelerators::LockScreen();
      break;
    case MAGNIFIER_ZOOM_IN:
      accelerators::ActiveMagnifierZoom(1);
      break;
    case MAGNIFIER_ZOOM_OUT:
      accelerators::ActiveMagnifierZoom(-1);
      break;
    case MEDIA_FAST_FORWARD:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Fast_Forward"));
      accelerators::MediaFastForward();
      break;
    case MEDIA_NEXT_TRACK:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Next_Track"));
      accelerators::MediaNextTrack();
      break;
    case MEDIA_PAUSE:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Pause"));
      accelerators::MediaPause();
      break;
    case MEDIA_PLAY:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Play"));
      accelerators::MediaPlay();
      break;
    case MEDIA_PLAY_PAUSE:
      base::RecordAction(base::UserMetricsAction("Accel_Media_PlayPause"));
      accelerators::MediaPlayPause();
      break;
    case MEDIA_PREV_TRACK:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Prev_Track"));
      accelerators::MediaPrevTrack();
      break;
    case MEDIA_REWIND:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Rewind"));
      accelerators::MediaRewind();
      break;
    case MEDIA_STOP:
      base::RecordAction(base::UserMetricsAction("Accel_Media_Stop"));
      accelerators::MediaStop();
      break;
    case MICROPHONE_MUTE_TOGGLE:
      base::RecordAction(base::UserMetricsAction("Accel_Microphone_Mute"));
      accelerators::MicrophoneMuteToggle();
      break;
    case MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS:
      accelerators::MoveActiveWindowBetweenDisplays();
      break;
    case NEW_INCOGNITO_WINDOW:
      base::RecordAction(base::UserMetricsAction("Accel_New_Incognito_Window"));
      accelerators::NewIncognitoWindow();
      break;
    case NEW_TAB:
      RecordNewTab(accelerator);
      accelerators::NewTab();
      break;
    case NEW_WINDOW:
      base::RecordAction(base::UserMetricsAction("Accel_New_Window"));
      accelerators::NewWindow();
      break;
    case OPEN_CALCULATOR:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Calculator"));
      accelerators::OpenCalculator();
      break;
    case OPEN_CROSH:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Crosh"));
      accelerators::OpenCrosh();
      break;
    case OPEN_DIAGNOSTICS:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Diagnostics"));
      accelerators::OpenDiagnostics();
      break;
    case OPEN_FEEDBACK_PAGE:
      base::RecordAction(base::UserMetricsAction("Accel_Open_Feedback_Page"));
      accelerators::OpenFeedbackPage();
      break;
    case OPEN_FILE_MANAGER:
      base::RecordAction(base::UserMetricsAction("Accel_Open_File_Manager"));
      accelerators::OpenFileManager();
      break;
    case OPEN_GET_HELP:
      accelerators::OpenHelp();
      break;
    case PASTE_CLIPBOARD_HISTORY_PLAIN_TEXT:
      accelerators::ToggleClipboardHistory(/*is_plain_text_paste=*/true);
      break;
    case POWER_PRESSED:
    case POWER_RELEASED:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd, the Chrome OS power manager, in linux desktop,
        // so call the PowerButtonController here.
        accelerators::PowerPressed(action == POWER_PRESSED);
      }
      // We don't do anything with these at present on the device,
      // (power button events are reported to us from powerm via
      // D-BUS), but we consume them to prevent them from getting
      // passed to apps -- see http://crbug.com/146609.
      break;
    case PRINT_UI_HIERARCHIES:
      debug::PrintUIHierarchies();
      break;
    case PRIVACY_SCREEN_TOGGLE:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Privacy_Screen"));
      accelerators::TogglePrivacyScreen();
      break;
    case ROTATE_SCREEN:
      accelerators::RotateScreen();
      break;
    case RESTORE_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_Restore_Tab"));
      accelerators::RestoreTab();
      break;
    case ROTATE_WINDOW:
      base::RecordAction(UserMetricsAction("Accel_Rotate_Active_Window"));
      accelerators::RotateActiveWindow();
      break;
    case SCALE_UI_DOWN:
      accelerators::ZoomDisplay(false /* down */);
      break;
    case SCALE_UI_RESET:
      accelerators::ResetDisplayZoom();
      break;
    case SCALE_UI_UP:
      accelerators::ZoomDisplay(true /* up */);
      break;
    case SHOW_EMOJI_PICKER:
      base::RecordAction(UserMetricsAction("Accel_Show_Emoji_Picker"));
      accelerators::ShowEmojiPicker();
      break;
    case TOGGLE_IME_MENU_BUBBLE:
      base::RecordAction(UserMetricsAction("Accel_Show_Ime_Menu_Bubble"));
      accelerators::ToggleImeMenuBubble();
      break;
    case TOGGLE_PROJECTOR_MARKER:
      accelerators::ToggleProjectorMarker();
      break;
    case SHOW_SHORTCUT_VIEWER:
      if (features::ShouldOnlyShowNewShortcutApp()) {
        accelerators::ShowShortcutCustomizationApp();
      } else {
        accelerators::ShowKeyboardShortcutViewer();
      }
      break;
    case SHOW_STYLUS_TOOLS:
      base::RecordAction(UserMetricsAction("Accel_Show_Stylus_Tools"));
      accelerators::ShowStylusTools();
      break;
    case SHOW_TASK_MANAGER:
      base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
      accelerators::ShowTaskManager();
      break;
    case START_AMBIENT_MODE:
      accelerators::ToggleAmbientMode();
      break;
    case START_ASSISTANT:
      // TODO(longbowei): Move this to CanToggleAssistant().
      if (ShouldToggleAssistant(accelerator)) {
        RecordToggleAssistant(accelerator);
        accelerators::ToggleAssistant();
      }
      break;
    case SUSPEND:
      base::RecordAction(UserMetricsAction("Accel_Suspend"));
      accelerators::Suspend();
      break;
    case SWAP_PRIMARY_DISPLAY:
      base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));
      accelerators::ShiftPrimaryDisplay();
      break;
    case SWITCH_IME:
      HandleSwitchIme(accelerator);
      break;
    case SWITCH_TO_LAST_USED_IME:
      HandleSwitchToLastUsedIme(accelerator);
      break;
    case SWITCH_TO_NEXT_IME:
      RecordSwitchToNextIme(accelerator);
      accelerators::SwitchToNextIme();
      break;
    case SWITCH_TO_NEXT_USER:
      MultiProfileUMA::RecordSwitchActiveUser(
          MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Next_User"));
      accelerators::CycleUser(CycleUserDirection::NEXT);
      break;
    case SWITCH_TO_PREVIOUS_USER:
      MultiProfileUMA::RecordSwitchActiveUser(
          MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Previous_User"));
      accelerators::CycleUser(CycleUserDirection::PREVIOUS);
      break;
    case TAKE_PARTIAL_SCREENSHOT:
      // UMA metrics are recorded in the function.
      accelerators::MaybeTakePartialScreenshot();
      break;
    case TAKE_SCREENSHOT:
      base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
      accelerators::TakeScreenshot(accelerator.key_code() == ui::VKEY_SNAPSHOT);
      break;
    case TAKE_WINDOW_SCREENSHOT:
      // UMA metrics are recorded in the function.
      accelerators::MaybeTakeWindowScreenshot();
      break;
    case TOGGLE_APP_LIST: {
      RecordToggleAppList(accelerator);
      accelerators::ToggleAppList(AppListShowSource::kSearchKey,
                                  base::TimeTicks());
      break;
    }
    case TOGGLE_CALENDAR:
      accelerators::ToggleCalendar();
      break;
    case TOGGLE_CAPS_LOCK:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Caps_Lock"));
      accelerators::ToggleCapsLock();
      break;
    case TOGGLE_CLIPBOARD_HISTORY:
      accelerators::ToggleClipboardHistory(/*is_plain_text_paste=*/false);
      break;
    case TOGGLE_DICTATION:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Dictation"));
      accelerators::ToggleDictation();
      break;
    case TOGGLE_DOCKED_MAGNIFIER:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Docked_Magnifier"));
      accelerators::ToggleDockedMagnifier();
      break;
    case DEBUG_TUCK_FLOATED_WINDOW_LEFT:
    case DEBUG_TUCK_FLOATED_WINDOW_RIGHT:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case TOGGLE_FLOATING:
      // UMA metrics are recorded in the function.
      accelerators::ToggleFloating();
      break;
    case TOGGLE_FULLSCREEN:
      RecordToggleFullscreen(accelerator);
      accelerators::ToggleFullscreen();
      break;
    case TOGGLE_FULLSCREEN_MAGNIFIER:
      base::RecordAction(
          UserMetricsAction("Accel_Toggle_Fullscreen_Magnifier"));
      accelerators::ToggleFullscreenMagnifier();
      break;
    case TOGGLE_GAME_DASHBOARD:
      accelerators::ToggleGameDashboard();
      break;
    case TOGGLE_HIGH_CONTRAST:
      base::RecordAction(UserMetricsAction("Accel_Toggle_High_Contrast"));
      accelerators::ToggleHighContrast();
      break;
    case TOGGLE_MAXIMIZED:
      accelerators::ToggleMaximized();
      break;
    case TOGGLE_MESSAGE_CENTER_BUBBLE:
      base::RecordAction(
          UserMetricsAction("Accel_Toggle_Message_Center_Bubble"));
      accelerators::ToggleMessageCenterBubble();
      break;
    case TOGGLE_MIRROR_MODE:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Mirror_Mode"));
      accelerators::ToggleMirrorMode();
      break;
    case TOGGLE_MULTITASK_MENU:
      accelerators::ToggleMultitaskMenu();
      return;
    case TOGGLE_OVERVIEW:
      base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
      accelerators::ToggleOverview();
      break;
    case TOGGLE_RESIZE_LOCK_MENU:
      base::RecordAction(
          base::UserMetricsAction("Accel_Toggle_Resize_Lock_Menu"));
      accelerators::ToggleResizeLockMenu();
      break;
    case TOGGLE_SPOKEN_FEEDBACK:
      base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));
      accelerators::ToggleSpokenFeedback();
      break;
    case TOGGLE_SYSTEM_TRAY_BUBBLE:
      base::RecordAction(UserMetricsAction("Accel_Toggle_System_Tray_Bubble"));
      accelerators::ToggleSystemTrayBubble();
      break;
    case TOGGLE_WIFI:
      accelerators::ToggleWifi();
      break;
    case TOUCH_HUD_CLEAR:
      accelerators::TouchHudClear();
      break;
    case TOUCH_HUD_MODE_CHANGE:
      accelerators::TouchHudModeChange();
      break;
    case UNPIN:
      accelerators::UnpinWindow();
      break;
    case VOLUME_DOWN:
      base::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));
      output_volume_metric_delay_timer_.Reset();
      accelerators::VolumeDown();
      break;
    case VOLUME_MUTE:
      if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
        base::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));
      accelerators::VolumeMute();
      break;
    case VOLUME_UP:
      base::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));
      output_volume_metric_delay_timer_.Reset();
      accelerators::VolumeUp();
      break;
    case WINDOW_CYCLE_SNAP_LEFT:
      base::RecordAction(UserMetricsAction("Accel_Window_Snap_Left"));
      accelerators::WindowSnap(AcceleratorAction::WINDOW_CYCLE_SNAP_LEFT);
      break;
    case WINDOW_CYCLE_SNAP_RIGHT:
      base::RecordAction(UserMetricsAction("Accel_Window_Snap_Right"));
      accelerators::WindowSnap(AcceleratorAction::WINDOW_CYCLE_SNAP_RIGHT);
      break;
    case WINDOW_MINIMIZE:
      base::RecordAction(
          base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
      accelerators::WindowMinimize();
      break;
    case MINIMIZE_TOP_WINDOW_ON_BACK:
      base::RecordAction(
          base::UserMetricsAction("Accel_Minimize_Top_Window_On_Back"));
      accelerators::TopWindowMinimizeOnBack();
      break;
  }

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
