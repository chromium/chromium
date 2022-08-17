// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/devicetype.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/debug.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_move_window_util.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/focus_cycler.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/window_rotation.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_accessibility_controller.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/wm/desks/chromeos_desks_histogram_enums.h"
#include "chromeos/ui/wm/features.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

const char kTabletCountOfVolumeAdjustType[] = "Tablet.CountOfVolumeAdjustType";

const char kAccessibilityHighContrastShortcut[] =
    "Accessibility.Shortcuts.CrosHighContrast";
const char kAccessibilitySpokenFeedbackShortcut[] =
    "Accessibility.Shortcuts.CrosSpokenFeedback";
const char kAccessibilityScreenMagnifierShortcut[] =
    "Accessibility.Shortcuts.CrosScreenMagnifier";
const char kAccessibilityDockedMagnifierShortcut[] =
    "Accessibility.Shortcuts.CrosDockedMagnifier";

const char kAccelWindowSnap[] = "Ash.Accelerators.WindowSnap";

namespace {

using ::base::UserMetricsAction;
using ::chromeos::WindowStateType;
using input_method::InputMethodManager;

// Toast id and duration for Assistant shortcuts.
constexpr char kAssistantErrorToastId[] = "assistant_error";

constexpr char kVirtualDesksToastId[] = "virtual_desks_toast";

// Path of the json file that contains side volume button location info.
constexpr char kSideVolumeButtonLocationFilePath[] =
    "/usr/share/chromeos-assets/side_volume_button/location.json";

// The interval between two volume control actions within one volume adjust.
constexpr base::TimeDelta kVolumeAdjustTimeout = base::Seconds(2);

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
// Records the result of triggering the rotation accelerator.
enum class RotationAcceleratorAction {
  kCancelledDialog = 0,
  kAcceptedDialog = 1,
  kAlreadyAcceptedDialog = 2,
  kMaxValue = kAlreadyAcceptedDialog,
};

static_assert(DESKS_ACTIVATE_0 == DESKS_ACTIVATE_1 - 1 &&
                  DESKS_ACTIVATE_1 == DESKS_ACTIVATE_2 - 1 &&
                  DESKS_ACTIVATE_2 == DESKS_ACTIVATE_3 - 1 &&
                  DESKS_ACTIVATE_3 == DESKS_ACTIVATE_4 - 1 &&
                  DESKS_ACTIVATE_4 == DESKS_ACTIVATE_5 - 1 &&
                  DESKS_ACTIVATE_5 == DESKS_ACTIVATE_6 - 1 &&
                  DESKS_ACTIVATE_6 == DESKS_ACTIVATE_7 - 1,
              "DESKS_ACTIVATE* actions must be consecutive");

void RecordRotationAcceleratorAction(const RotationAcceleratorAction& action) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Accelerators.Rotation.Usage", action);
}

void RecordWindowSnapAcceleratorAction(WindowSnapAcceleratorAction action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelWindowSnap, action);
}

void RecordTabletVolumeAdjustTypeHistogram(TabletModeVolumeAdjustType type) {
  UMA_HISTOGRAM_ENUMERATION(kTabletCountOfVolumeAdjustType, type);
}

void ShowToast(std::string id,
               ToastCatalogName catalog_name,
               const std::u16string& text) {
  ToastData toast(id, catalog_name, text, ToastData::kDefaultToastDuration,
                  /*visible_on_lock_screen=*/true);
  Shell::Get()->toast_manager()->Show(toast);
}

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

void HandleActivateDesk(const ui::Accelerator& accelerator,
                        bool activate_left) {
  auto* desks_controller = DesksController::Get();
  const bool success = desks_controller->ActivateAdjacentDesk(
      activate_left, DesksSwitchSource::kDeskSwitchShortcut);
  if (!success)
    return;

  if (activate_left) {
    base::RecordAction(base::UserMetricsAction("Accel_Desks_ActivateLeft"));
  } else {
    base::RecordAction(base::UserMetricsAction("Accel_Desks_ActivateRight"));
  }
}

void HandleMoveActiveItem(const ui::Accelerator& accelerator, bool going_left) {
  auto* desks_controller = DesksController::Get();
  if (desks_controller->AreDesksBeingModified())
    return;

  aura::Window* window_to_move = nullptr;
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (in_overview) {
    window_to_move =
        overview_controller->overview_session()->GetHighlightedWindow();
  } else {
    window_to_move = window_util::GetActiveWindow();
  }

  if (!window_to_move || !desks_util::BelongsToActiveDesk(window_to_move))
    return;

  Desk* target_desk = nullptr;
  if (going_left) {
    target_desk = desks_controller->GetPreviousDesk();
    base::RecordAction(base::UserMetricsAction("Accel_Desks_MoveWindowLeft"));
  } else {
    target_desk = desks_controller->GetNextDesk();
    base::RecordAction(base::UserMetricsAction("Accel_Desks_MoveWindowRight"));
  }

  if (!target_desk)
    return;

  if (!in_overview) {
    desks_animations::PerformWindowMoveToDeskAnimation(window_to_move,
                                                       going_left);
  }

  if (!desks_controller->MoveWindowFromActiveDeskTo(
          window_to_move, target_desk, window_to_move->GetRootWindow(),
          DesksMoveWindowFromActiveDeskSource::kShortcut)) {
    return;
  }

  if (in_overview) {
    // We should not exit overview as a result of this shortcut.
    DCHECK(overview_controller->InOverviewSession());
    overview_controller->overview_session()->PositionWindows(/*animate=*/true);
  }
}

void HandleNewDesk() {
  auto* desks_controller = DesksController::Get();
  if (!desks_controller->CanCreateDesks()) {
    ShowToast(kVirtualDesksToastId, ToastCatalogName::kVirtualDesksLimitMax,
              l10n_util::GetStringUTF16(IDS_ASH_DESKS_MAX_NUM_REACHED));
    return;
  }

  if (desks_controller->AreDesksBeingModified())
    return;

  // Add a new desk and switch to it.
  const size_t new_desk_index = desks_controller->desks().size();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  const Desk* desk = desks_controller->desks()[new_desk_index].get();
  desks_controller->ActivateDesk(desk, DesksSwitchSource::kNewDeskShortcut);
  base::RecordAction(base::UserMetricsAction("Accel_Desks_NewDesk"));
}

void HandleRemoveCurrentDesk() {
  if (window_util::IsAnyWindowDragged())
    return;

  auto* desks_controller = DesksController::Get();
  if (!desks_controller->CanRemoveDesks()) {
    ShowToast(kVirtualDesksToastId, ToastCatalogName::kVirtualDesksLimitMin,
              l10n_util::GetStringUTF16(IDS_ASH_DESKS_MIN_NUM_REACHED));
    return;
  }

  if (desks_controller->AreDesksBeingModified())
    return;

  // TODO(afakhry): Finalize the desk removal animation outside of overview with
  // UX. https://crbug.com/977434.
  desks_controller->RemoveDesk(desks_controller->active_desk(),
                               DesksCreationRemovalSource::kKeyboard,
                               DeskCloseType::kCombineDesks);
  base::RecordAction(base::UserMetricsAction("Accel_Desks_RemoveDesk"));
}

void HandleActivateDeskAtIndex(AcceleratorAction action) {
  DCHECK_LE(action, DESKS_ACTIVATE_7);
  const size_t target_index = action - DESKS_ACTIVATE_0;
  auto* desks_controller = DesksController::Get();
  // Only 1 desk animation can occur at a time so ignore this action if there's
  // an ongoing desk animation.
  if (desks_controller->AreDesksBeingModified())
    return;

  const auto& desks = desks_controller->desks();
  if (target_index < desks.size()) {
    desks_controller->ActivateDesk(
        desks[target_index].get(),
        DesksSwitchSource::kIndexedDeskSwitchShortcut);
  } else {
    for (auto* root : Shell::GetAllRootWindows())
      desks_animations::PerformHitTheWallAnimation(root, /*going_left=*/false);
  }
}

void HandleToggleAssignToAllDesks() {
  auto* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;

  // Only children of the desk container should have their assigned to all
  // desks state toggled to avoid interfering with special windows like
  // always-on-top windows, floated windows, etc.
  if (desks_util::IsActiveDeskContainer(active_window->parent())) {
    const bool is_already_visible_on_all_desks =
        desks_util::IsWindowVisibleOnAllWorkspaces(active_window);
    if (!is_already_visible_on_all_desks) {
      UMA_HISTOGRAM_ENUMERATION(
          chromeos::kDesksAssignToAllDesksSourceHistogramName,
          chromeos::DesksAssignToAllDesksSource::kKeyboardShortcut);
    }

    active_window->SetProperty(
        aura::client::kWindowWorkspaceKey,
        is_already_visible_on_all_desks
            ? aura::client::kWindowWorkspaceUnassignedWorkspace
            : aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  }
}

void HandleRotatePaneFocus(FocusCycler::Direction direction) {
  switch (direction) {
    // TODO(stevet): Not sure if this is the same as IDC_FOCUS_NEXT_PANE.
    case FocusCycler::FORWARD: {
      base::RecordAction(UserMetricsAction("Accel_Focus_Next_Pane"));
      break;
    }
    case FocusCycler::BACKWARD: {
      base::RecordAction(UserMetricsAction("Accel_Focus_Previous_Pane"));
      break;
    }
  }
  Shell::Get()->focus_cycler()->RotateFocus(direction);
}

void HandleFocusShelf() {
  base::RecordAction(UserMetricsAction("Accel_Focus_Shelf"));

  if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    // If floating accessibility menu is shown, focus on it instead of the
    // shelf.
    FloatingAccessibilityController* floating_menu =
        Shell::Get()->accessibility_controller()->GetFloatingMenuController();
    if (floating_menu) {
      floating_menu->FocusOnMenu();
    }
    return;
  }

  // TODO(jamescook): Should this be GetRootWindowForNewWindows()?
  // Focus the home button.
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  shelf->shelf_focus_cycler()->FocusNavigation(false /* lastElement */);
}

// TODO(zentaro): This is duplicated in accelerator_commands.cc. Remove
// once the CanFocusPipWidget() function is moved.
views::Widget* FindPipWidget() {
  return Shell::Get()->focus_cycler()->FindWidget(
      base::BindRepeating([](views::Widget* widget) {
        return WindowState::Get(widget->GetNativeWindow())->IsPip();
      }));
}

bool CanHandleFocusCameraPreview() {
  auto* controller = CaptureModeController::Get();
  // Only use the shortcut to focus the camera preview while video recording is
  // in progress. As focus traversal of the camera preview in the capture
  // session will be handled by CaptureModeSessionFocusCycler instead.
  if (controller->IsActive() || !controller->is_recording_in_progress())
    return false;

  auto* camera_controller = controller->camera_controller();
  DCHECK(camera_controller);
  auto* preview_widget = camera_controller->camera_preview_widget();
  return preview_widget && preview_widget->IsVisible();
}

void HandleFocusCameraPreview() {
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  DCHECK(camera_controller);
  camera_controller->PseudoFocusCameraPreview();
}

void HandleToggleMirrorMode() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Mirror_Mode"));
  bool mirror = !Shell::Get()->display_manager()->IsInMirrorMode();
  Shell::Get()->display_configuration_controller()->SetMirrorMode(
      mirror, true /* throttle */);
}

bool CanHandleNewIncognitoWindow() {
  // Guest mode does not use incognito windows. The browser may have other
  // restrictions on incognito mode (e.g. enterprise policy) but those are rare.
  // For non-guest mode, consume the key and defer the decision to the browser.
  absl::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return user_type && *user_type != user_manager::USER_TYPE_GUEST;
}

void HandleNewTab(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_T)
    base::RecordAction(UserMetricsAction("Accel_NewTab_T"));
  NewWindowDelegate::GetPrimary()->NewTab();
}

bool CanCycleInputMethod() {
  return Shell::Get()->ime_controller()->CanSwitchIme();
}

bool CanHandleCycleMru(const ui::Accelerator& accelerator) {
  // Don't do anything when Alt+Tab is hit while a virtual keyboard is showing.
  // Touchscreen users have better window switching options. It would be
  // preferable if we could tell whether this event actually came from a virtual
  // keyboard, but there's no easy way to do so, thus we block Alt+Tab when the
  // virtual keyboard is showing, even if it came from a real keyboard. See
  // http://crbug.com/638269
  return !keyboard::KeyboardUIController::Get()->IsKeyboardVisible();
}

void HandleSwitchToNextIme(const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Next_Ime"));
  if (accelerator.key_code() == ui::VKEY_MODECHANGE)
    RecordImeSwitchByModeChangeKey();
  else
    RecordImeSwitchByAccelerator();
  Shell::Get()->ime_controller()->SwitchToNextIme();
}

void HandleSwitchToLastUsedIme(const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Previous_Ime"));
  if (accelerator.key_state() == ui::Accelerator::KeyState::PRESSED) {
    RecordImeSwitchByAccelerator();
    Shell::Get()->ime_controller()->SwitchToLastUsedIme();
  }
  // Else: consume the Ctrl+Space ET_KEY_RELEASED event but do not do anything.
}

display::Display::Rotation GetNextRotationInClamshell(
    display::Display::Rotation current) {
  switch (current) {
    case display::Display::ROTATE_0:
      return display::Display::ROTATE_90;
    case display::Display::ROTATE_90:
      return display::Display::ROTATE_180;
    case display::Display::ROTATE_180:
      return display::Display::ROTATE_270;
    case display::Display::ROTATE_270:
      return display::Display::ROTATE_0;
  }
  NOTREACHED() << "Unknown rotation:" << current;
  return display::Display::ROTATE_0;
}

display::Display::Rotation GetNextRotationInTabletMode(
    int64_t display_id,
    display::Display::Rotation current) {
  Shell* shell = Shell::Get();
  DCHECK(shell->tablet_mode_controller()->InTabletMode());

  if (!display::HasInternalDisplay() ||
      display_id != display::Display::InternalDisplayId()) {
    return GetNextRotationInClamshell(current);
  }

  const chromeos::OrientationType app_requested_lock =
      shell->screen_orientation_controller()
          ->GetCurrentAppRequestedOrientationLock();

  bool add_180_degrees = false;
  switch (app_requested_lock) {
    case chromeos::OrientationType::kCurrent:
    case chromeos::OrientationType::kLandscapePrimary:
    case chromeos::OrientationType::kLandscapeSecondary:
    case chromeos::OrientationType::kPortraitPrimary:
    case chromeos::OrientationType::kPortraitSecondary:
    case chromeos::OrientationType::kNatural:
      // Do not change the current orientation.
      return current;

    case chromeos::OrientationType::kLandscape:
    case chromeos::OrientationType::kPortrait:
      // App allows both primary and secondary orientations in either landscape
      // or portrait, therefore switch to the next one by adding 180 degrees.
      add_180_degrees = true;
      break;

    default:
      break;
  }

  switch (current) {
    case display::Display::ROTATE_0:
      return add_180_degrees ? display::Display::ROTATE_180
                             : display::Display::ROTATE_90;
    case display::Display::ROTATE_90:
      return add_180_degrees ? display::Display::ROTATE_270
                             : display::Display::ROTATE_180;
    case display::Display::ROTATE_180:
      return add_180_degrees ? display::Display::ROTATE_0
                             : display::Display::ROTATE_270;
    case display::Display::ROTATE_270:
      return add_180_degrees ? display::Display::ROTATE_90
                             : display::Display::ROTATE_0;
  }
  NOTREACHED() << "Unknown rotation:" << current;
  return display::Display::ROTATE_0;
}

bool ShouldLockRotation(int64_t display_id) {
  return display::HasInternalDisplay() &&
         display_id == display::Display::InternalDisplayId() &&
         Shell::Get()->screen_orientation_controller()->IsAutoRotationAllowed();
}

int64_t GetDisplayIdForRotation() {
  const gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  return display::Screen::GetScreen()->GetDisplayNearestPoint(point).id();
}

void RotateScreen() {
  auto* shell = Shell::Get();
  const bool in_tablet_mode =
      Shell::Get()->tablet_mode_controller()->InTabletMode();
  const int64_t display_id = GetDisplayIdForRotation();
  const display::ManagedDisplayInfo& display_info =
      shell->display_manager()->GetDisplayInfo(display_id);
  const auto active_rotation = display_info.GetActiveRotation();
  const auto next_rotation =
      in_tablet_mode ? GetNextRotationInTabletMode(display_id, active_rotation)
                     : GetNextRotationInClamshell(active_rotation);
  if (active_rotation == next_rotation)
    return;

  // When the auto-rotation is allowed in the device, display rotation requests
  // of the internal display are treated as requests to lock the user rotation.
  if (ShouldLockRotation(display_id)) {
    shell->screen_orientation_controller()->SetLockToRotation(next_rotation);
    return;
  }

  shell->display_configuration_controller()->SetDisplayRotation(
      display_id, next_rotation, display::Display::RotationSource::USER);
}

void OnRotationDialogAccepted() {
  RecordRotationAcceleratorAction(RotationAcceleratorAction::kAcceptedDialog);
  RotateScreen();
  Shell::Get()
      ->accessibility_controller()
      ->SetDisplayRotationAcceleratorDialogBeenAccepted();
}

void OnRotationDialogCancelled() {
  RecordRotationAcceleratorAction(RotationAcceleratorAction::kCancelledDialog);
}

// Rotates the screen.
void HandleRotateScreen() {
  if (Shell::Get()->display_manager()->IsInUnifiedMode())
    return;

  base::RecordAction(UserMetricsAction("Accel_Rotate_Screen"));
  const bool dialog_ever_accepted =
      Shell::Get()
          ->accessibility_controller()
          ->HasDisplayRotationAcceleratorDialogBeenAccepted();

  if (!dialog_ever_accepted) {
    Shell::Get()->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_ROTATE_SCREEN_TITLE, IDS_ASH_ROTATE_SCREEN_BODY,
        base::BindOnce(&OnRotationDialogAccepted),
        base::BindOnce(&OnRotationDialogCancelled));
  } else {
    RecordRotationAcceleratorAction(
        RotationAcceleratorAction::kAlreadyAcceptedDialog);
    RotateScreen();
  }
}

// Rotate the active window.
void HandleRotateActiveWindow() {
  base::RecordAction(UserMetricsAction("Accel_Rotate_Active_Window"));
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  // The rotation animation bases its target transform on the current
  // rotation and position. Since there could be an animation in progress
  // right now, queue this animation so when it starts it picks up a neutral
  // rotation and position. Use replace so we only enqueue one at a time.
  active_window->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  active_window->layer()->GetAnimator()->StartAnimation(
      new ui::LayerAnimationSequence(
          std::make_unique<WindowRotation>(360, active_window->layer())));
}

void HandleShowKeyboardShortcutViewer() {
  NewWindowDelegate::GetInstance()->ShowKeyboardShortcutViewer();
}

bool CanHandleScreenshot(AcceleratorAction action) {
  // |TAKE_SCREENSHOT| is allowed when user session is blocked.
  return action == TAKE_SCREENSHOT ||
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

bool CanHandleToggleResizeLockMenu() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return false;
  auto* frame_view = ash::NonClientFrameViewAsh::Get(active_window);
  return frame_view && frame_view->GetToggleResizeLockMenuCallback();
}

bool CanHandleToggleFloatingWindow() {
  if (!chromeos::wm::features::IsFloatWindowEnabled())
    return false;

  return window_util::GetActiveWindow() != nullptr;
}

// Enters capture mode image type with |source|.
void EnterImageCaptureMode(CaptureModeSource source,
                           CaptureModeEntryType entry_type) {
  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->SetSource(source);
  capture_mode_controller->SetType(CaptureModeType::kImage);
  capture_mode_controller->Start(entry_type);
}

void MaybeHandleTakeWindowScreenshot() {
  // If a capture mode session is already running, this shortcut will be treated
  // as a no-op.
  if (CaptureModeController::Get()->IsActive())
    return;
  base::RecordAction(UserMetricsAction("Accel_Take_Window_Screenshot"));
  EnterImageCaptureMode(CaptureModeSource::kWindow,
                        CaptureModeEntryType::kAccelTakeWindowScreenshot);
}

void MaybeHandleTakePartialScreenshot() {
  // If a capture mode session is already running, this shortcut will be treated
  // as a no-op.
  if (CaptureModeController::Get()->IsActive())
    return;
  base::RecordAction(UserMetricsAction("Accel_Take_Partial_Screenshot"));
  EnterImageCaptureMode(CaptureModeSource::kRegion,
                        CaptureModeEntryType::kAccelTakePartialScreenshot);
}

void HandleTakeScreenshot(ui::KeyboardCode key_code) {
  base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
  // If it is the snip key, toggle capture mode unless the session is blocked,
  // in which case, it behaves like a fullscreen screenshot.
  auto* capture_mode_controller = CaptureModeController::Get();
  if (key_code == ui::VKEY_SNAPSHOT &&
      !Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    if (capture_mode_controller->IsActive())
      capture_mode_controller->Stop();
    else
      capture_mode_controller->Start(CaptureModeEntryType::kSnipKey);
    return;
  }

  capture_mode_controller->CaptureScreenshotsOfAllDisplays();
}

void HandleToggleSystemTrayBubbleInternal(bool focus_message_center) {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  UnifiedSystemTray* tray = RootWindowController::ForWindow(target_root)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  if (tray->IsBubbleShown()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble();
    tray->ActivateBubble();

    if (focus_message_center)
      tray->FocusMessageCenter(false, true);
  }
}

void HandleToggleSystemTrayBubble() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_System_Tray_Bubble"));
  HandleToggleSystemTrayBubbleInternal(false /*focus_message_center*/);
}

void HandleToggleMessageCenterBubble() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Message_Center_Bubble"));
  HandleToggleSystemTrayBubbleInternal(true /*focus_message_center*/);
}

void HandleShowTaskManager() {
  base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
  NewWindowDelegate::GetInstance()->ShowTaskManager();
}

void HandleSwapPrimaryDisplay() {
  base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));
  accelerators::ShiftPrimaryDisplay();
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

void HandleToggleAppList(const ui::Accelerator& accelerator,
                         AppListShowSource show_source) {
  if (accelerator.key_code() == ui::VKEY_LWIN)
    base::RecordAction(UserMetricsAction("Accel_Search_LWin"));

  aura::Window* const root_window = Shell::GetRootWindowForNewWindows();
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window).id(),
      show_source, accelerator.time_stamp());
}

void HandleToggleFloating() {
  DCHECK(chromeos::wm::features::IsFloatWindowEnabled());
  aura::Window* window = window_util::GetActiveWindow();
  DCHECK(window);
  // TODO(sammiequon|shidi): Add some UI like a bounce if a window cannot be
  // floated.
  Shell::Get()->float_controller()->ToggleFloat(window);
  base::RecordAction(UserMetricsAction("Accel_Toggle_Floating"));
}

void HandleToggleFullscreen(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ZOOM)
    base::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  // Disable fullscreen while overview animation is running due to
  // http://crbug.com/1094739
  if (!overview_controller->IsInStartAnimation())
    accelerators::ToggleFullscreen();
}

void HandleToggleOverview() {
  base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview(OverviewEndAction::kAccelerator);
  else
    overview_controller->StartOverview(OverviewStartAction::kAccelerator);
}

void HandleToggleUnifiedDesktop() {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(
      !Shell::Get()->display_manager()->unified_desktop_enabled());
}

bool CanHandleWindowSnap() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return false;
  WindowState* window_state = WindowState::Get(active_window);
  return window_state && window_state->IsUserPositionable();
}

void HandleWindowSnap(AcceleratorAction action) {
  Shell* shell = Shell::Get();
  const bool in_tablet = shell->tablet_mode_controller()->InTabletMode();
  const bool in_overview = shell->overview_controller()->InOverviewSession();
  if (action == WINDOW_CYCLE_SNAP_LEFT) {
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Left"));
    if (in_tablet) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleLeftSnapInTablet);
    } else if (in_overview) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleLeftSnapInClamshellOverview);
    } else {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleLeftSnapInClamshellNoOverview);
    }
  } else {
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Right"));
    if (in_tablet) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleRightSnapInTablet);
    } else if (in_overview) {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleRightSnapInClamshellOverview);
    } else {
      RecordWindowSnapAcceleratorAction(
          WindowSnapAcceleratorAction::kCycleRightSnapInClamshellNoOverview);
    }
  }

  const WindowSnapWMEvent event(action == WINDOW_CYCLE_SNAP_LEFT
                                    ? WM_EVENT_CYCLE_SNAP_PRIMARY
                                    : WM_EVENT_CYCLE_SNAP_SECONDARY);
  aura::Window* active_window = window_util::GetActiveWindow();
  DCHECK(active_window);

  auto* window_state = WindowState::Get(active_window);
  window_state->set_snap_action_source(
      WindowSnapActionSource::kKeyboardShortcutToSnap);
  window_state->OnWMEvent(&event);
}

void HandleWindowMinimize() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
  accelerators::ToggleMinimized();
}

void HandleTopWindowMinimizeOnBack() {
  base::RecordAction(
      base::UserMetricsAction("Accel_Minimize_Top_Window_On_Back"));
  WindowState::Get(window_util::GetTopWindow())->Minimize();
}

void HandleShowEmojiPicker() {
  base::RecordAction(UserMetricsAction("Accel_Show_Emoji_Picker"));
  ui::ShowEmojiPanel();
}

void HandleToggleImeMenuBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Ime_Menu_Bubble"));

  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetStatusAreaWidget();
  if (status_area_widget) {
    ImeMenuTray* ime_menu_tray = status_area_widget->ime_menu_tray();
    if (!ime_menu_tray || !ime_menu_tray->GetVisible()) {
      // Do nothing when Ime tray is not being shown.
      return;
    }
    if (ime_menu_tray->GetBubbleView()) {
      ime_menu_tray->CloseBubble();
    } else {
      ime_menu_tray->ShowBubble();
    }
  }
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

bool CanHandleLock() {
  return Shell::Get()->session_controller()->CanLockScreen();
}

PaletteTray* GetPaletteTray() {
  return Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
      ->GetStatusAreaWidget()
      ->palette_tray();
}

void HandleShowStylusTools() {
  base::RecordAction(UserMetricsAction("Accel_Show_Stylus_Tools"));
  GetPaletteTray()->ShowBubble();
}

bool CanHandleShowStylusTools() {
  return GetPaletteTray()->ShouldShowPalette();
}

bool CanHandleStartAmbientMode() {
  return chromeos::features::IsAmbientModeEnabled();
}

void HandleToggleAmbientMode(const ui::Accelerator& accelerator) {
  Shell::Get()->ambient_controller()->ToggleInSessionUi();
}

void HandleToggleAssistant(const ui::Accelerator& accelerator) {
  if (accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_SPACE) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_Space"));
  } else if (accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_A) {
    // Search+A shortcut is disabled on device with an assistant key.
    // Currently only Google branded device has the key. Some external keyboard
    // may report it has the key but actually not.  This would cause keyboard
    // shortcut stops working.  So we only check the key on these branded
    // devices.
    if (chromeos::IsGoogleBrandedDevice() &&
        ui::DeviceKeyboardHasAssistantKey()) {
      return;
    }

    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_A"));
  } else if (accelerator.key_code() == ui::VKEY_ASSISTANT) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Assistant"));
  }

  using assistant::AssistantAllowedState;
  switch (AssistantState::Get()->allowed_state().value_or(
      AssistantAllowedState::ALLOWED)) {
    case AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER:
      // Show a toast if the active user is not primary.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_SECONDARY_USER_TOAST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_LOCALE:
      // Show a toast if the Assistant is disabled due to unsupported
      // locales.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_LOCALE_UNSUPPORTED_TOAST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_POLICY:
      // Show a toast if the Assistant is disabled due to enterprise policy.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_BY_POLICY_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_DEMO_MODE:
      // Show a toast if the Assistant is disabled due to being in Demo
      // Mode.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_DEMO_MODE_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION:
      // Show a toast if the Assistant is disabled due to being in public
      // session.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_PUBLIC_SESSION_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_INCOGNITO:
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_GUEST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE:
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_BY_ACCOUNT_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE:
      // No need to show toast in KIOSK mode.
      return;
    case AssistantAllowedState::ALLOWED:
      // Nothing need to do if allowed.
      break;
  }

  AssistantUiController::Get()->ToggleUi(
      /*entry_point=*/assistant::AssistantEntryPoint::kHotkey,
      /*exit_point=*/assistant::AssistantExitPoint::kHotkey);
}

void HandleSuspend() {
  base::RecordAction(UserMetricsAction("Accel_Suspend"));
  chromeos::PowerManagerClient::Get()->RequestSuspend();
}

bool CanHandleCycleUser() {
  return Shell::Get()->session_controller()->NumberOfLoggedInUsers() > 1;
}

void HandleCycleUser(CycleUserDirection direction) {
  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
  switch (direction) {
    case CycleUserDirection::NEXT:
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Next_User"));
      break;
    case CycleUserDirection::PREVIOUS:
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Previous_User"));
      break;
  }
  Shell::Get()->session_controller()->CycleActiveUser(direction);
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

void HandleToggleCapsLock() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Caps_Lock"));
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  ime_controller->SetCapsLockEnabled(!ime_controller->IsCapsLockEnabled());
}

void HandleToggleClipboardHistory() {
  DCHECK(Shell::Get()->clipboard_history_controller());
  Shell::Get()->clipboard_history_controller()->ToggleMenuShownByAccelerator();
}

bool CanHandleToggleDictation() {
  return Shell::Get()->accessibility_controller()->dictation().enabled();
}

void HandleToggleDictation() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Dictation"));
  Shell::Get()->accessibility_controller()->ToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
}

bool CanHandleToggleOverview() {
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  // Do not toggle overview if there is a window being dragged.
  for (auto* window : windows) {
    if (WindowState::Get(window)->is_dragged())
      return false;
  }
  return true;
}

// Return false if the accessibility shortcuts have been disabled, or if
// the accessibility feature itself associated with |accessibility_pref_name|
// is being enforced by the administrator.
bool IsAccessibilityShortcutEnabled(
    const std::string& accessibility_pref_name) {
  Shell* shell = Shell::Get();
  return shell->accessibility_controller()->accessibility_shortcuts_enabled() &&
         !shell->session_controller()
              ->GetActivePrefService()
              ->IsManagedPreference(accessibility_pref_name);
}

void SetDockedMagnifierEnabled(bool enabled) {
  Shell* shell = Shell::Get();
  // Check that the attempt to change the value of the accessibility feature
  // will be done only when the accessibility shortcuts are enabled, and
  // the feature isn't being enforced by the administrator.
  DCHECK(IsAccessibilityShortcutEnabled(prefs::kDockedMagnifierEnabled));

  shell->docked_magnifier_controller()->SetEnabled(enabled);

  RemoveDockedMagnifierNotification();
  if (shell->docked_magnifier_controller()->GetEnabled()) {
    ShowDockedMagnifierNotification();
  }
}

void HandleToggleDockedMagnifier() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Docked_Magnifier"));

  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kDockedMagnifierEnabled);

  base::UmaHistogramBoolean(kAccessibilityDockedMagnifierShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveDockedMagnifierNotification();
  if (!is_shortcut_enabled) {
    ShowDockedMagnifierDisabledByAdminNotification(
        shell->docked_magnifier_controller()->GetEnabled());
    return;
  }

  DockedMagnifierController* docked_magnifier_controller =
      shell->docked_magnifier_controller();
  AccessibilityControllerImpl* accessibility_controller =
      shell->accessibility_controller();

  const bool current_enabled = docked_magnifier_controller->GetEnabled();
  const bool dialog_ever_accepted =
      accessibility_controller->docked_magnifier().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    shell->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_DOCKED_MAGNIFIER_TITLE, IDS_ASH_DOCKED_MAGNIFIER_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->docked_magnifier()
              .SetDialogAccepted();
          SetDockedMagnifierEnabled(true);
        }),
        base::DoNothing());
  } else {
    SetDockedMagnifierEnabled(!current_enabled);
  }
}

void SetFullscreenMagnifierEnabled(bool enabled) {
  // TODO (afakhry): Move the below into a single call (crbug/817157).
  // Necessary to make magnification controller in ash observe changes to the
  // prefs iteself.
  Shell* shell = Shell::Get();
  // Check that the attempt to change the value of the accessibility feature
  // will be done only when the accessibility shortcuts are enabled, and
  // the feature isn't being enforced by the administrator.
  DCHECK(IsAccessibilityShortcutEnabled(
      prefs::kAccessibilityScreenMagnifierEnabled));

  shell->accessibility_controller()->fullscreen_magnifier().SetEnabled(enabled);

  RemoveFullscreenMagnifierNotification();
  if (shell->fullscreen_magnifier_controller()->IsEnabled()) {
    ShowFullscreenMagnifierNotification();
  }
}

void SetHighContrastEnabled(bool enabled) {
  Shell* shell = Shell::Get();
  // Check that the attempt to change the value of the accessibility feature
  // will be done only when the accessibility shortcuts are enabled, and
  // the feature isn't being enforced by the administrator.
  DCHECK(
      IsAccessibilityShortcutEnabled(prefs::kAccessibilityHighContrastEnabled));

  shell->accessibility_controller()->high_contrast().SetEnabled(enabled);

  RemoveHighContrastNotification();
  if (shell->accessibility_controller()->high_contrast().enabled()) {
    ShowHighContrastNotification();
  }
}

void HandleToggleHighContrast() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_High_Contrast"));

  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kAccessibilityHighContrastEnabled);

  base::UmaHistogramBoolean(kAccessibilityHighContrastShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveHighContrastNotification();
  if (!is_shortcut_enabled) {
    ShowHighContrastDisabledByAdminNotification(
        shell->accessibility_controller()->high_contrast().enabled());
    return;
  }

  AccessibilityControllerImpl* controller = shell->accessibility_controller();
  const bool current_enabled = controller->high_contrast().enabled();
  const bool dialog_ever_accepted =
      controller->high_contrast().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    shell->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_HIGH_CONTRAST_TITLE, IDS_ASH_HIGH_CONTRAST_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->high_contrast()
              .SetDialogAccepted();
          SetHighContrastEnabled(true);
        }),
        base::DoNothing());
  } else {
    SetHighContrastEnabled(!current_enabled);
  }
}

void HandleToggleFullscreenMagnifier() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Fullscreen_Magnifier"));

  const bool is_shortcut_enabled = IsAccessibilityShortcutEnabled(
      prefs::kAccessibilityScreenMagnifierEnabled);

  base::UmaHistogramBoolean(kAccessibilityScreenMagnifierShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveFullscreenMagnifierNotification();
  if (!is_shortcut_enabled) {
    ShowFullscreenMagnifierDisabledByAdminNotification(
        shell->fullscreen_magnifier_controller()->IsEnabled());
    return;
  }

  FullscreenMagnifierController* magnification_controller =
      shell->fullscreen_magnifier_controller();
  AccessibilityControllerImpl* accessibility_controller =
      shell->accessibility_controller();

  const bool current_enabled = magnification_controller->IsEnabled();
  const bool dialog_ever_accepted =
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    shell->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_SCREEN_MAGNIFIER_TITLE, IDS_ASH_SCREEN_MAGNIFIER_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->fullscreen_magnifier()
              .SetDialogAccepted();
          SetFullscreenMagnifierEnabled(true);
        }),
        base::DoNothing());
  } else {
    SetFullscreenMagnifierEnabled(!current_enabled);
  }
}

void HandleToggleSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));

  const bool is_shortcut_enabled = IsAccessibilityShortcutEnabled(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  base::UmaHistogramBoolean(kAccessibilitySpokenFeedbackShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();
  const bool old_value =
      shell->accessibility_controller()->spoken_feedback().enabled();

  RemoveSpokenFeedbackNotification();
  if (!is_shortcut_enabled) {
    ShowSpokenFeedbackDisabledByAdminNotification(old_value);
    return;
  }

  shell->accessibility_controller()->SetSpokenFeedbackEnabled(
      !old_value, A11Y_NOTIFICATION_SHOW);
}

bool CanHandleTogglePrivacyScreen() {
  return Shell::Get()->privacy_screen_controller()->IsSupported();
}

void HandleTogglePrivacyScreen() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Privacy_Screen"));

  PrivacyScreenController* controller =
      Shell::Get()->privacy_screen_controller();
  controller->SetEnabled(
      !controller->GetEnabled(),
      PrivacyScreenController::kToggleUISurfaceKeyboardShortcut);
}

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

void HandleVolumeDown() {
  base::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));

  auto* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputVolumePercent(0);
  } else {
    audio_handler->AdjustOutputVolumeByPercent(-kStepPercentage);
    if (audio_handler->IsOutputVolumeBelowDefaultMuteLevel())
      audio_handler->SetOutputMute(true);
    else
      AcceleratorController::PlayVolumeAdjustmentSound();
  }
}

void HandleVolumeMute(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
    base::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));

  CrasAudioHandler::Get()->SetOutputMute(true);
}

void HandleVolumeUp() {
  base::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));

  auto* audio_handler = CrasAudioHandler::Get();
  bool play_sound = false;
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputMute(false);
    audio_handler->AdjustOutputVolumeToAudibleLevel();
    play_sound = true;
  } else {
    play_sound = audio_handler->GetOutputVolumePercent() != 100;
    audio_handler->AdjustOutputVolumeByPercent(kStepPercentage);
  }

  if (play_sound)
    AcceleratorController::PlayVolumeAdjustmentSound();
}

bool CanHandleActiveMagnifierZoom() {
  return Shell::Get()->fullscreen_magnifier_controller()->IsEnabled() ||
         Shell::Get()->docked_magnifier_controller()->GetEnabled();
}

// Change the scale of the active magnifier.
void HandleActiveMagnifierZoom(int delta_index) {
  if (Shell::Get()->fullscreen_magnifier_controller()->IsEnabled()) {
    Shell::Get()->fullscreen_magnifier_controller()->StepToNextScaleValue(
        delta_index);
    return;
  }

  if (Shell::Get()->docked_magnifier_controller()->GetEnabled()) {
    Shell::Get()->docked_magnifier_controller()->StepToNextScaleValue(
        delta_index);
  }
}

bool CanHandleTouchHud() {
  return RootWindowController::ForTargetRootWindow()->touch_hud_debug();
}

bool CanUnpinWindow() {
  // WindowStateType::kTrustedPinned does not allow the user to press a key to
  // exit pinned mode.
  WindowState* window_state = WindowState::ForActiveWindow();
  return window_state &&
         window_state->GetStateType() == WindowStateType::kPinned;
}

void HandleTouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void HandleTouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
}

bool CanHandleToggleProjectorMarker() {
  auto* projector_controller = ProjectorController::Get();
  if (projector_controller) {
    return projector_controller->GetAnnotatorAvailability();
  }
  return false;
}

void HandleToggleProjectorMarker() {
  auto* projector_controller = ProjectorController::Get();
  if (projector_controller) {
    projector_controller->ToggleAnnotationTray();
  }
}

}  // namespace

constexpr const char* AcceleratorControllerImpl::kVolumeButtonRegion;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonSide;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonRegionKeyboard;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonRegionScreen;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonSideLeft;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonSideRight;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonSideTop;
constexpr const char* AcceleratorControllerImpl::kVolumeButtonSideBottom;

////////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerImpl, public:

AcceleratorControllerImpl::TestApi::TestApi(
    AcceleratorControllerImpl* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

bool AcceleratorControllerImpl::TestApi::TriggerTabletModeVolumeAdjustTimer() {
  if (!controller_->tablet_mode_volume_adjust_timer_.IsRunning())
    return false;

  controller_->tablet_mode_volume_adjust_timer_.FireNow();
  return true;
}

void AcceleratorControllerImpl::TestApi::RegisterAccelerators(
    const AcceleratorData accelerators[],
    size_t accelerators_length) {
  controller_->RegisterAccelerators(accelerators, accelerators_length);
}

bool AcceleratorControllerImpl::TestApi::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) {
  return controller_->IsActionForAcceleratorEnabled(accelerator);
}

const DeprecatedAcceleratorData*
AcceleratorControllerImpl::TestApi::GetDeprecatedAcceleratorData(
    AcceleratorAction action) {
  auto it = controller_->actions_with_deprecations_.find(action);
  if (it == controller_->actions_with_deprecations_.end())
    return nullptr;

  return it->second;
}

AccessibilityConfirmationDialog*
AcceleratorControllerImpl::TestApi::GetConfirmationDialog() {
  return controller_->confirmation_dialog_.get();
}

ExitWarningHandler*
AcceleratorControllerImpl::TestApi::GetExitWarningHandler() {
  return &controller_->exit_warning_handler_;
}

void AcceleratorControllerImpl::TestApi::SetSideVolumeButtonFilePath(
    base::FilePath path) {
  controller_->side_volume_button_location_file_path_ = path;
  controller_->ParseSideVolumeButtonLocationInfo();
}

void AcceleratorControllerImpl::TestApi::SetSideVolumeButtonLocation(
    const std::string& region,
    const std::string& side) {
  controller_->side_volume_button_location_.region = region;
  controller_->side_volume_button_location_.side = side;
}

AcceleratorControllerImpl::AcceleratorControllerImpl()
    : accelerator_manager_(std::make_unique<ui::AcceleratorManager>()),
      accelerator_history_(std::make_unique<AcceleratorHistoryImpl>()),
      side_volume_button_location_file_path_(
          base::FilePath(kSideVolumeButtonLocationFilePath)) {
  Init();

  ParseSideVolumeButtonLocationInfo();

  // Let AcceleratorHistory be a PreTargetHandler on aura::Env to ensure that it
  // receives KeyEvents and MouseEvents. In some cases Shell PreTargetHandlers
  // will handle Events before AcceleratorHistory gets to see them. This
  // interferes with Accelerator processing. See https://crbug.com/1174603.
  aura::Env::GetInstance()->AddPreTargetHandler(
      accelerator_history_.get(), ui::EventTarget::Priority::kAccessibility);
}

AcceleratorControllerImpl::~AcceleratorControllerImpl() {
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
  accelerators_.set_use_positional_lookup(use_positional_lookup);
  accelerator_manager_->SetUsePositionalLookup(use_positional_lookup);
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
  return base::Contains(deprecated_accelerators_, accelerator);
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
  AcceleratorAction* action_ptr = accelerators_.Find(accelerator);
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
  AcceleratorAction* action_ptr = accelerators_.Find(accelerator);
  return action_ptr && *action_ptr == action;
}

bool AcceleratorControllerImpl::IsPreferred(
    const ui::Accelerator& accelerator) const {
  const AcceleratorAction* action_ptr = accelerators_.Find(accelerator);
  return action_ptr && base::Contains(preferred_actions_, *action_ptr);
}

bool AcceleratorControllerImpl::IsReserved(
    const ui::Accelerator& accelerator) const {
  const AcceleratorAction* action_ptr = accelerators_.Find(accelerator);
  return action_ptr && base::Contains(reserved_actions_, *action_ptr);
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerImpl, ui::AcceleratorTarget implementation:

bool AcceleratorControllerImpl::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  AcceleratorAction action = accelerators_.Get(accelerator);
  if (!CanPerformAction(action, accelerator))
    return false;

  // Handling the deprecated accelerators (if any) only if action can be
  // performed.
  if (MaybeDeprecatedAcceleratorPressed(action, accelerator) ==
      AcceleratorProcessingStatus::STOP) {
    return false;
  }

  PerformAction(action, accelerator);
  return ShouldActionConsumeKeyEvent(action);
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

  RegisterAccelerators(kAcceleratorData, kAcceleratorDataLength);

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    RegisterAccelerators(kEnableWithPositionalAcceleratorsData,
                         kEnableWithPositionalAcceleratorsDataLength);
    if (ash::features::IsImprovedDesksKeyboardShortcutsEnabled()) {
      RegisterAccelerators(
          kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData,
          kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength);
    }
  } else if (::features::IsNewShortcutMappingEnabled()) {
    RegisterAccelerators(kEnableWithNewMappingAcceleratorData,
                         kEnableWithNewMappingAcceleratorDataLength);
  } else {
    RegisterAccelerators(kDisableWithNewMappingAcceleratorData,
                         kDisableWithNewMappingAcceleratorDataLength);
  }

  RegisterDeprecatedAccelerators();

  if (debug::DebugAcceleratorsEnabled()) {
    RegisterAccelerators(kDebugAcceleratorData, kDebugAcceleratorDataLength);
    // All debug accelerators are reserved.
    for (size_t i = 0; i < kDebugAcceleratorDataLength; ++i)
      reserved_actions_.insert(kDebugAcceleratorData[i].action);
  }

  if (debug::DeveloperAcceleratorsEnabled()) {
    RegisterAccelerators(kDeveloperAcceleratorData,
                         kDeveloperAcceleratorDataLength);
    // Developer accelerators are also reserved.
    for (size_t i = 0; i < kDeveloperAcceleratorDataLength; ++i)
      reserved_actions_.insert(kDeveloperAcceleratorData[i].action);
  }
}

void AcceleratorControllerImpl::RegisterAccelerators(
    const AcceleratorData accelerators[],
    size_t accelerators_length) {
  std::vector<ui::Accelerator> ui_accelerators;
  for (size_t i = 0; i < accelerators_length; ++i) {
    ui::Accelerator accelerator =
        CreateAccelerator(accelerators[i].keycode, accelerators[i].modifiers,
                          accelerators[i].trigger_on_press);
    ui_accelerators.push_back(accelerator);
    accelerators_.InsertNew(
        std::make_pair(accelerator, accelerators[i].action));
  }
  Register(ui_accelerators, this);
}

void AcceleratorControllerImpl::RegisterDeprecatedAccelerators() {
  for (size_t i = 0; i < kDeprecatedAcceleratorsDataLength; ++i) {
    const DeprecatedAcceleratorData* data = &kDeprecatedAcceleratorsData[i];
    actions_with_deprecations_[data->action] = data;
  }

  std::vector<ui::Accelerator> ui_accelerators;
  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    const AcceleratorData& accelerator_data = kDeprecatedAccelerators[i];
    const ui::Accelerator deprecated_accelerator =
        CreateAccelerator(accelerator_data.keycode, accelerator_data.modifiers,
                          accelerator_data.trigger_on_press);

    ui_accelerators.push_back(deprecated_accelerator);
    accelerators_.InsertNew(
        std::make_pair(deprecated_accelerator, accelerator_data.action));
    deprecated_accelerators_.insert(deprecated_accelerator);
  }
  Register(ui_accelerators, this);
}

bool AcceleratorControllerImpl::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) const {
  const AcceleratorAction* action_ptr = accelerators_.Find(accelerator);
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
      return CanHandleCycleMru(accelerator);
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
    case DEBUG_DUMP_CALENDAR_MODEL:
    case DEBUG_KEYBOARD_BACKLIGHT_TOGGLE:
    case DEBUG_MICROPHONE_MUTE_TOGGLE:
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
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
      return CanHandleLock();
    case MAGNIFIER_ZOOM_IN:
    case MAGNIFIER_ZOOM_OUT:
      return CanHandleActiveMagnifierZoom();
    case MICROPHONE_MUTE_TOGGLE:
      return true;
    case MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS:
      return display_move_window_util::
          CanHandleMoveActiveWindowBetweenDisplays();
    case NEW_INCOGNITO_WINDOW:
      return CanHandleNewIncognitoWindow();
    case PRIVACY_SCREEN_TOGGLE:
      return CanHandleTogglePrivacyScreen();
    case ROTATE_SCREEN:
      return true;
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
      return true;
    case SHOW_STYLUS_TOOLS:
      return CanHandleShowStylusTools();
    case START_AMBIENT_MODE:
      return CanHandleStartAmbientMode();
    case START_ASSISTANT:
      return true;
    case SWAP_PRIMARY_DISPLAY:
      return display::Screen::GetScreen()->GetNumDisplays() > 1;
    case SWITCH_IME:
      return CanHandleSwitchIme(accelerator);
    case SWITCH_TO_NEXT_IME:
      return CanCycleInputMethod();
    case SWITCH_TO_LAST_USED_IME:
      return CanCycleInputMethod();
    case SWITCH_TO_PREVIOUS_USER:
    case SWITCH_TO_NEXT_USER:
      return CanHandleCycleUser();
    case TOGGLE_APP_LIST:
    case TOGGLE_APP_LIST_FULLSCREEN:
      return CanHandleToggleAppList(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case TOGGLE_CALENDAR:
      return features::IsCalendarViewEnabled();
    case TOGGLE_CAPS_LOCK:
      return CanHandleToggleCapsLock(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case TOGGLE_CLIPBOARD_HISTORY:
      return true;
    case TOGGLE_DICTATION:
      return CanHandleToggleDictation();
    case TOGGLE_DOCKED_MAGNIFIER:
      return true;
    case TOGGLE_FULLSCREEN_MAGNIFIER:
      return true;
    case TOGGLE_MESSAGE_CENTER_BUBBLE:
      return true;
    case TOGGLE_MIRROR_MODE:
      return true;
    case TOGGLE_OVERVIEW:
      return CanHandleToggleOverview();
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      return CanHandleTouchHud();
    case UNPIN:
      return CanUnpinWindow();
    case WINDOW_CYCLE_SNAP_LEFT:
    case WINDOW_CYCLE_SNAP_RIGHT:
      return CanHandleWindowSnap();
    case FOCUS_PIP:
      return !!FindPipWidget();
    case FOCUS_CAMERA_PREVIEW:
      return CanHandleFocusCameraPreview();
    case MINIMIZE_TOP_WINDOW_ON_BACK:
      return window_util::ShouldMinimizeTopWindowOnBack();
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
      return CanHandleScreenshot(action);
    case TOGGLE_PROJECTOR_MARKER:
      return CanHandleToggleProjectorMarker();
    case TOGGLE_RESIZE_LOCK_MENU:
      return CanHandleToggleResizeLockMenu();
    case TOGGLE_FLOATING:
      return CanHandleToggleFloatingWindow();

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
    if (ShouldSwapSideVolumeButtons(accelerator.source_device_id()))
      action = action == VOLUME_DOWN ? VOLUME_UP : VOLUME_DOWN;

    StartTabletModeVolumeAdjustTimer(action);
  }

  // If your accelerator invokes more than one line of code, please either
  // implement it in your module's controller code or pull it into a HandleFoo()
  // function above.
  switch (action) {
    case BRIGHTNESS_DOWN: {
      BrightnessControlDelegate* delegate =
          Shell::Get()->brightness_control_delegate();
      if (delegate)
        delegate->HandleBrightnessDown(accelerator);
      break;
    }
    case BRIGHTNESS_UP: {
      BrightnessControlDelegate* delegate =
          Shell::Get()->brightness_control_delegate();
      if (delegate)
        delegate->HandleBrightnessUp(accelerator);
      break;
    }
    case CYCLE_BACKWARD_MRU:
      RecordCycleBackwardMru(accelerator);
      accelerators::CycleBackwardMru();
      break;
    case CYCLE_FORWARD_MRU:
      RecordCycleForwardMru(accelerator);
      accelerators::CycleForwardMru();
      break;
    case DESKS_ACTIVATE_DESK_LEFT:
      HandleActivateDesk(accelerator, /*activate_left=*/true);
      break;
    case DESKS_ACTIVATE_DESK_RIGHT:
      HandleActivateDesk(accelerator, /*activate_left=*/false);
      break;
    case DESKS_MOVE_ACTIVE_ITEM_LEFT:
      HandleMoveActiveItem(accelerator, /*going_left=*/true);
      break;
    case DESKS_MOVE_ACTIVE_ITEM_RIGHT:
      HandleMoveActiveItem(accelerator, /*going_left=*/false);
      break;
    case DESKS_NEW_DESK:
      HandleNewDesk();
      break;
    case DESKS_REMOVE_CURRENT_DESK:
      HandleRemoveCurrentDesk();
      break;
    case DESKS_ACTIVATE_0:
    case DESKS_ACTIVATE_1:
    case DESKS_ACTIVATE_2:
    case DESKS_ACTIVATE_3:
    case DESKS_ACTIVATE_4:
    case DESKS_ACTIVATE_5:
    case DESKS_ACTIVATE_6:
    case DESKS_ACTIVATE_7:
      HandleActivateDeskAtIndex(action);
      break;
    case DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS:
      HandleToggleAssignToAllDesks();
      break;
    case DEBUG_DUMP_CALENDAR_MODEL:
    case DEBUG_KEYBOARD_BACKLIGHT_TOGGLE:
    case DEBUG_MICROPHONE_MUTE_TOGGLE:
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
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
      HandleToggleAppList(accelerator, kSearchKey);
      break;
    case DEV_TOGGLE_UNIFIED_DESKTOP:
      HandleToggleUnifiedDesktop();
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
      HandleRotatePaneFocus(FocusCycler::FORWARD);
      break;
    case FOCUS_PREVIOUS_PANE:
      HandleRotatePaneFocus(FocusCycler::BACKWARD);
      break;
    case FOCUS_SHELF:
      HandleFocusShelf();
      break;
    case FOCUS_CAMERA_PREVIEW:
      HandleFocusCameraPreview();
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
      KeyboardBrightnessControlDelegate* delegate =
          Shell::Get()->keyboard_brightness_control_delegate();
      if (delegate)
        delegate->HandleKeyboardBrightnessDown(accelerator);
      break;
    }
    case KEYBOARD_BRIGHTNESS_UP: {
      KeyboardBrightnessControlDelegate* delegate =
          Shell::Get()->keyboard_brightness_control_delegate();
      if (delegate)
        delegate->HandleKeyboardBrightnessUp(accelerator);
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
      Shell::Get()->power_button_controller()->OnLockButtonEvent(
          action == LOCK_PRESSED, base::TimeTicks());
      break;
    case LOCK_SCREEN:
      base::RecordAction(base::UserMetricsAction("Accel_LockScreen_L"));
      accelerators::LockScreen();
      break;
    case MAGNIFIER_ZOOM_IN:
      HandleActiveMagnifierZoom(1);
      break;
    case MAGNIFIER_ZOOM_OUT:
      HandleActiveMagnifierZoom(-1);
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
      display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
      break;
    case NEW_INCOGNITO_WINDOW:
      base::RecordAction(base::UserMetricsAction("Accel_New_Incognito_Window"));
      accelerators::NewIncognitoWindow();
      break;
    case NEW_TAB:
      HandleNewTab(accelerator);
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
    case POWER_PRESSED:
    case POWER_RELEASED:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd, the Chrome OS power manager, in linux desktop,
        // so call the PowerButtonController here.
        Shell::Get()->power_button_controller()->OnPowerButtonEvent(
            action == POWER_PRESSED, base::TimeTicks());
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
      HandleTogglePrivacyScreen();
      break;
    case ROTATE_SCREEN:
      HandleRotateScreen();
      break;
    case RESTORE_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_Restore_Tab"));
      accelerators::RestoreTab();
      break;
    case ROTATE_WINDOW:
      HandleRotateActiveWindow();
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
      HandleShowEmojiPicker();
      break;
    case TOGGLE_IME_MENU_BUBBLE:
      HandleToggleImeMenuBubble();
      break;
    case TOGGLE_PROJECTOR_MARKER:
      HandleToggleProjectorMarker();
      break;
    case SHOW_SHORTCUT_VIEWER:
      HandleShowKeyboardShortcutViewer();
      break;
    case SHOW_STYLUS_TOOLS:
      HandleShowStylusTools();
      break;
    case SHOW_TASK_MANAGER:
      HandleShowTaskManager();
      break;
    case START_AMBIENT_MODE:
      HandleToggleAmbientMode(accelerator);
      break;
    case START_ASSISTANT:
      HandleToggleAssistant(accelerator);
      break;
    case SUSPEND:
      HandleSuspend();
      break;
    case SWAP_PRIMARY_DISPLAY:
      HandleSwapPrimaryDisplay();
      break;
    case SWITCH_IME:
      HandleSwitchIme(accelerator);
      break;
    case SWITCH_TO_LAST_USED_IME:
      HandleSwitchToLastUsedIme(accelerator);
      break;
    case SWITCH_TO_NEXT_IME:
      HandleSwitchToNextIme(accelerator);
      break;
    case SWITCH_TO_NEXT_USER:
      HandleCycleUser(CycleUserDirection::NEXT);
      break;
    case SWITCH_TO_PREVIOUS_USER:
      HandleCycleUser(CycleUserDirection::PREVIOUS);
      break;
    case TAKE_PARTIAL_SCREENSHOT:
      MaybeHandleTakePartialScreenshot();
      break;
    case TAKE_SCREENSHOT:
      HandleTakeScreenshot(accelerator.key_code());
      break;
    case TAKE_WINDOW_SCREENSHOT:
      MaybeHandleTakeWindowScreenshot();
      break;
    case TOGGLE_APP_LIST:
      HandleToggleAppList(accelerator, kSearchKey);
      break;
    case TOGGLE_APP_LIST_FULLSCREEN:
      HandleToggleAppList(accelerator, kSearchKeyFullscreen);
      break;
    case TOGGLE_CALENDAR:
      accelerators::ToggleCalendar();
      break;
    case TOGGLE_CAPS_LOCK:
      HandleToggleCapsLock();
      break;
    case TOGGLE_CLIPBOARD_HISTORY:
      HandleToggleClipboardHistory();
      break;
    case TOGGLE_DICTATION:
      HandleToggleDictation();
      break;
    case TOGGLE_DOCKED_MAGNIFIER:
      HandleToggleDockedMagnifier();
      break;
    case TOGGLE_FLOATING:
      HandleToggleFloating();
      break;
    case TOGGLE_FULLSCREEN:
      HandleToggleFullscreen(accelerator);
      break;
    case TOGGLE_FULLSCREEN_MAGNIFIER:
      HandleToggleFullscreenMagnifier();
      break;
    case TOGGLE_HIGH_CONTRAST:
      HandleToggleHighContrast();
      break;
    case TOGGLE_MAXIMIZED:
      accelerators::ToggleMaximized();
      break;
    case TOGGLE_MESSAGE_CENTER_BUBBLE:
      HandleToggleMessageCenterBubble();
      break;
    case TOGGLE_MIRROR_MODE:
      HandleToggleMirrorMode();
      break;
    case TOGGLE_OVERVIEW:
      HandleToggleOverview();
      break;
    case TOGGLE_RESIZE_LOCK_MENU:
      base::RecordAction(
          base::UserMetricsAction("Accel_Toggle_Resize_Lock_Menu"));
      accelerators::ToggleResizeLockMenu();
      break;
    case TOGGLE_SPOKEN_FEEDBACK:
      HandleToggleSpokenFeedback();
      break;
    case TOGGLE_SYSTEM_TRAY_BUBBLE:
      HandleToggleSystemTrayBubble();
      break;
    case TOGGLE_WIFI:
      Shell::Get()->system_tray_notifier()->NotifyRequestToggleWifi();
      break;
    case TOUCH_HUD_CLEAR:
      HandleTouchHudClear();
      break;
    case TOUCH_HUD_MODE_CHANGE:
      HandleTouchHudModeChange();
      break;
    case UNPIN:
      accelerators::UnpinWindow();
      break;
    case VOLUME_DOWN:
      HandleVolumeDown();
      break;
    case VOLUME_MUTE:
      HandleVolumeMute(accelerator);
      break;
    case VOLUME_UP:
      HandleVolumeUp();
      break;
    case WINDOW_CYCLE_SNAP_LEFT:
    case WINDOW_CYCLE_SNAP_RIGHT:
      HandleWindowSnap(action);
      break;
    case WINDOW_MINIMIZE:
      HandleWindowMinimize();
      break;
    case MINIMIZE_TOP_WINDOW_ON_BACK:
      HandleTopWindowMinimizeOnBack();
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
  auto itr = actions_with_deprecations_.find(action);
  if (itr == actions_with_deprecations_.end()) {
    // The action is not associated with any deprecated accelerators, and hence
    // should be performed normally.
    return AcceleratorProcessingStatus::PROCEED;
  }

  // This action is associated with new and deprecated accelerators, find which
  // one is |accelerator|.
  const DeprecatedAcceleratorData* data = itr->second;
  if (!base::Contains(deprecated_accelerators_, accelerator)) {
    // This is a new accelerator replacing the old deprecated one.
    // Record UMA stats and proceed normally to perform it.
    RecordUmaHistogram(data->uma_histogram_name, NEW_USED);
    return AcceleratorProcessingStatus::PROCEED;
  }

  // This accelerator has been deprecated and should be treated according
  // to its |DeprecatedAcceleratorData|.

  // Record UMA stats.
  RecordUmaHistogram(data->uma_histogram_name, DEPRECATED_USED);

  // We always display the notification as long as this |data| entry exists.
  ShowDeprecatedAcceleratorNotification(
      data->uma_histogram_name, data->notification_message_id,
      data->old_shortcut_id, data->new_shortcut_id);

  if (!data->deprecated_enabled)
    return AcceleratorProcessingStatus::STOP;

  return AcceleratorProcessingStatus::PROCEED;
}

void AcceleratorControllerImpl::MaybeShowConfirmationDialog(
    int window_title_text_id,
    int dialog_text_id,
    base::OnceClosure on_accept_callback,
    base::OnceClosure on_cancel_callback) {
  // An active dialog exists already.
  if (confirmation_dialog_)
    return;

  auto* dialog = new AccessibilityConfirmationDialog(
      l10n_util::GetStringUTF16(window_title_text_id),
      l10n_util::GetStringUTF16(dialog_text_id), std::move(on_accept_callback),
      std::move(on_cancel_callback), /* on close */ base::DoNothing());
  confirmation_dialog_ = dialog->GetWeakPtr();
}

bool AcceleratorControllerImpl::IsInternalKeyboardOrUncategorizedDevice(
    int source_device_id) const {
  if (source_device_id == ui::ED_UNKNOWN_DEVICE)
    return false;

  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL &&
        keyboard.id == source_device_id) {
      return true;
    }
  }

  for (const ui::InputDevice& uncategorized_device :
       ui::DeviceDataManager::GetInstance()->GetUncategorizedDevices()) {
    if (uncategorized_device.id == source_device_id &&
        uncategorized_device.type ==
            ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }
  return false;
}

bool AcceleratorControllerImpl::IsValidSideVolumeButtonLocation() const {
  const std::string region = side_volume_button_location_.region;
  const std::string side = side_volume_button_location_.side;
  if (region != kVolumeButtonRegionKeyboard &&
      region != kVolumeButtonRegionScreen) {
    return false;
  }
  if (side != kVolumeButtonSideLeft && side != kVolumeButtonSideRight &&
      side != kVolumeButtonSideTop && side != kVolumeButtonSideBottom) {
    return false;
  }
  return true;
}

bool AcceleratorControllerImpl::ShouldSwapSideVolumeButtons(
    int source_device_id) const {
  if (!IsInternalKeyboardOrUncategorizedDevice(source_device_id))
    return false;

  if (!IsValidSideVolumeButtonLocation())
    return false;

  chromeos::OrientationType screen_orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  const std::string side = side_volume_button_location_.side;
  const bool is_landscape_secondary_or_portrait_primary =
      screen_orientation == chromeos::OrientationType::kLandscapeSecondary ||
      screen_orientation == chromeos::OrientationType::kPortraitPrimary;

  if (side_volume_button_location_.region == kVolumeButtonRegionKeyboard) {
    if (side == kVolumeButtonSideLeft || side == kVolumeButtonSideRight)
      return chromeos::IsPrimaryOrientation(screen_orientation);
    return is_landscape_secondary_or_portrait_primary;
  }

  DCHECK_EQ(kVolumeButtonRegionScreen, side_volume_button_location_.region);
  if (side == kVolumeButtonSideLeft || side == kVolumeButtonSideRight)
    return !chromeos::IsPrimaryOrientation(screen_orientation);
  return is_landscape_secondary_or_portrait_primary;
}

void AcceleratorControllerImpl::ParseSideVolumeButtonLocationInfo() {
  std::string location_info;
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kAshSideVolumeButtonPosition)) {
    location_info =
        cl->GetSwitchValueASCII(switches::kAshSideVolumeButtonPosition);
  } else if (!base::PathExists(side_volume_button_location_file_path_) ||
             !base::ReadFileToString(side_volume_button_location_file_path_,
                                     &location_info) ||
             location_info.empty()) {
    return;
  }

  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(location_info);
  if (!parsed_json || !parsed_json->is_dict()) {
    LOG(ERROR) << "JSONReader failed reading side volume button location info: "
               << location_info;
    return;
  }

  const base::Value::Dict& info_in_dict = parsed_json->GetDict();
  const std::string* region = info_in_dict.FindString(kVolumeButtonRegion);
  if (region)
    side_volume_button_location_.region = *region;

  const std::string* side = info_in_dict.FindString(kVolumeButtonSide);
  if (side)
    side_volume_button_location_.side = *side;
}

void AcceleratorControllerImpl::UpdateTabletModeVolumeAdjustHistogram() {
  const int volume_percent = CrasAudioHandler::Get()->GetOutputVolumePercent();
  if ((volume_adjust_starts_with_up_ &&
       volume_percent >= initial_volume_percent_) ||
      (!volume_adjust_starts_with_up_ &&
       volume_percent <= initial_volume_percent_)) {
    RecordTabletVolumeAdjustTypeHistogram(
        TabletModeVolumeAdjustType::kNormalAdjustWithSwapEnabled);
  } else {
    RecordTabletVolumeAdjustTypeHistogram(
        TabletModeVolumeAdjustType::kAccidentalAdjustWithSwapEnabled);
  }
}

void AcceleratorControllerImpl::StartTabletModeVolumeAdjustTimer(
    AcceleratorAction action) {
  if (!tablet_mode_volume_adjust_timer_.IsRunning()) {
    volume_adjust_starts_with_up_ = action == VOLUME_UP;
    initial_volume_percent_ = CrasAudioHandler::Get()->GetOutputVolumePercent();
  }
  tablet_mode_volume_adjust_timer_.Start(
      FROM_HERE, kVolumeAdjustTimeout, this,
      &AcceleratorControllerImpl::UpdateTabletModeVolumeAdjustHistogram);
}

}  // namespace ash
