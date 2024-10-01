// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include <optional>

#include "ash/accelerators/accelerator_lookup.h"
#include "ash/accelerators/accelerator_notifications.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_move_window_util.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/focus_cycler.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/media/media_controller_impl.h"
#include "ash/picker/picker_controller.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/annotator/annotator_controller_base.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/window_rotation.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_accessibility_controller.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/tile_group/window_tiling_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/wm/desks/chromeos_desks_histogram_enums.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

// Keep the functions in this file in alphabetical order.
namespace ash {

const char kAccelWindowSnap[] = "Ash.Accelerators.WindowSnap";
const char kAccelRotation[] = "Ash.Accelerators.Rotation.Usage";
const char kAccelActivateDeskByIndex[] = "Ash.Accelerators.ActivateDeskByIndex";
const char kAccelTogglePicker[] = "Ash.Accelerators.TogglePicker.Action";

namespace accelerators {

namespace {

using ::base::UserMetricsAction;
using ::chromeos::WindowStateType;

// Percent by which the volume should be changed when a volume key is pressed.
constexpr double kStepPercentage = 4.0;
constexpr char kVirtualDesksToastId[] = "virtual_desks_toast";
// Toast id for Assistant shortcuts.
constexpr char kAssistantErrorToastId[] = "assistant_error";
// Toast ID for the notification center tray "No notifications" toast.
constexpr char kNotificationCenterTrayNoNotificationsToastId[] =
    "notification_center_tray_toast_ids.no_notifications";

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
// Records the result of triggering the rotation accelerator.
enum class RotationAcceleratorAction {
  kCancelledDialog = 0,
  kAcceptedDialog = 1,
  kAlreadyAcceptedDialog = 2,
  kMaxValue = kAlreadyAcceptedDialog,
};

// Record which desk is activated.
enum class ActivateDeskAcceleratorAction {
  kDesk1 = 0,
  kDesk2 = 1,
  kDesk3 = 2,
  kDesk4 = 3,
  kDesk5 = 4,
  kDesk6 = 5,
  kDesk7 = 6,
  kDesk8 = 7,
  kMaxValue = kDesk8,
};

// Record what action is triggered by pressing toggle picker.
// The enum value is 1:1 mapped to what's defined in enums.xml.
enum class TogglePickerAction {
  kToggleCapsLock = 0,
  kTogglePicker = 1,
  kMaxValue = kTogglePicker,
};

void RecordRotationAcceleratorAction(const RotationAcceleratorAction& action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelRotation, action);
}

void RecordActivateDeskByIndexAcceleratorAction(
    const ActivateDeskAcceleratorAction& action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelActivateDeskByIndex, action);
}

void RecordWindowSnapAcceleratorAction(
    const WindowSnapAcceleratorAction& action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelWindowSnap, action);
}

void RecordTogglePickerAcceleratorAction(const TogglePickerAction& action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelTogglePicker, action);
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
}

display::Display::Rotation GetNextRotationInTabletMode(
    int64_t display_id,
    display::Display::Rotation current) {
  Shell* shell = Shell::Get();
  DCHECK(display::Screen::GetScreen()->InTabletMode());

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
}

views::Widget* FindPipWidget() {
  return Shell::Get()->focus_cycler()->FindWidget(
      base::BindRepeating([](views::Widget* widget) {
        return WindowState::Get(widget->GetNativeWindow())->IsPip();
      }));
}

PaletteTray* GetPaletteTray() {
  return Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
      ->GetStatusAreaWidget()
      ->palette_tray();
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

void RotateScreenImpl() {
  auto* shell = Shell::Get();
  const int64_t display_id = GetDisplayIdForRotation();
  const display::ManagedDisplayInfo& display_info =
      shell->display_manager()->GetDisplayInfo(display_id);
  const auto active_rotation = display_info.GetActiveRotation();
  const auto next_rotation =
      display::Screen::GetScreen()->InTabletMode()
          ? GetNextRotationInTabletMode(display_id, active_rotation)
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
  RotateScreenImpl();
  Shell::Get()
      ->accessibility_controller()
      ->SetDisplayRotationAcceleratorDialogBeenAccepted();
}

void OnRotationDialogCancelled() {
  RecordRotationAcceleratorAction(RotationAcceleratorAction::kCancelledDialog);
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

void SetFullscreenMagnifierEnabled(bool enabled) {
  // TODO (afakhry): Move the below into a single call (crbug/817157).
  // Necessary to make magnification controller in ash observe changes to the
  // prefs itself.
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

void ShowToast(const std::string& id,
               ToastCatalogName catalog_name,
               const std::u16string& text) {
  ToastData toast(id, catalog_name, text, ToastData::kDefaultToastDuration,
                  /*visible_on_lock_screen=*/true);
  Shell::Get()->toast_manager()->Show(std::move(toast));
}

// Enters capture mode image type with |source|.
void EnterImageCaptureMode(CaptureModeSource source,
                           CaptureModeEntryType entry_type) {
  auto* capture_mode_controller = CaptureModeController::Get();
  if (!capture_mode_controller->SupportsBehaviorChange(entry_type)) {
    // If capture mode session is already active with the default behavior,
    // disallow changing the source. This is needed even if `Start()` is a
    // no-op, since the session is already active. See http://b/252343022.
    return;
  }
  capture_mode_controller->SetSource(source);
  capture_mode_controller->SetType(CaptureModeType::kImage);
  capture_mode_controller->Start(entry_type);
}

// Get the window's frame size button, or nullptr if there isn't one.
chromeos::FrameSizeButton* GetFrameSizeButton(aura::Window* window) {
  if (!window) {
    return nullptr;
  }
  auto* frame_view = NonClientFrameViewAsh::Get(window);
  if (!frame_view) {
    return nullptr;
  }
  return static_cast<chromeos::FrameSizeButton*>(
      frame_view->GetHeaderView()->caption_button_container()->size_button());
}

// Gets the target window for accelerator action. This can be the top visible
// window not in overview, or active window if the accelerator is pressed during
// a window drag. Returns nullptr if neither exist.
aura::Window* GetTargetWindow() {
  aura::Window* window = window_util::GetTopWindow();
  if (!window) {
    return window_util::GetActiveWindow();
  }
  if (auto* overview_controller = Shell::Get()->overview_controller();
      overview_controller->InOverviewSession() &&
      overview_controller->overview_session()->IsWindowInOverview(window)) {
    return nullptr;
  }
  return window->IsVisible() ? window : nullptr;
}

// Returns the eligible independent windows for Snap Group creation.
std::optional<std::pair<aura::Window*, aura::Window*>>
GetIndependentWindowPairForSnapGroupCreation() {
  std::optional<std::pair<aura::Window*, aura::Window*>> window_pair;
  if (IsInOverviewSession()) {
    return window_pair;
  }

  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  if (!snap_group_controller) {
    return window_pair;
  }

  aura::Window* root_window = window_util::GetRootWindowAt(
      display::Screen::GetScreen()->GetCursorScreenPoint());
  aura::Window::Windows windows = GetActiveDeskAppWindowsInZOrder(root_window);
  aura::Window::Windows window_pair_list;
  for (size_t i = 0; i < windows.size() && window_pair_list.size() < 2; i++) {
    aura::Window* window = windows[i];
    WindowState* window_state = WindowState::Get(window);
    if (!window->IsVisible() || window_state->IsMinimized() ||
        desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
      continue;
    }

    // Upon finding a Snap Group, further iteration is unnecessary. Any windows
    // appearing later in the vector will be hidden behind the group.
    if (snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      break;
    }

    window_pair_list.push_back(window);
  }

  if (window_pair_list.size() == 2) {
    window_pair =
        std::make_pair(window_pair_list.at(0), window_pair_list.at(1));
  }

  return window_pair;
}

void ToggleTray(TrayBackgroundView* tray) {
  if (!tray || !tray->GetVisible()) {
    // Do nothing when the tray is not being shown.
    return;
  }
  if (tray->GetBubbleView()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble();
  }
}

}  // namespace

bool CanActivateTouchHud() {
  return RootWindowController::ForTargetRootWindow()->touch_hud_debug();
}

bool CanCreateNewIncognitoWindow() {
  // Guest mode does not use incognito windows. The browser may have other
  // restrictions on incognito mode (e.g. enterprise policy) but those are rare.
  // For non-guest mode, consume the key and defer the decision to the browser.
  std::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return user_type && *user_type != user_manager::UserType::kGuest;
}

bool CanCycleInputMethod() {
  return Shell::Get()->ime_controller()->CanSwitchIme();
}

bool CanCycleMru() {
  // Don't do anything when Alt+Tab is hit while a virtual keyboard is showing.
  // Touchscreen users have better window switching options. It would be
  // preferable if we could tell whether this event actually came from a virtual
  // keyboard, but there's no easy way to do so, thus we block Alt+Tab when the
  // virtual keyboard is showing, even if it came from a real keyboard. See
  // http://crbug.com/638269
  return !keyboard::KeyboardUIController::Get()->IsKeyboardVisible();
}

bool CanCycleSameAppWindows() {
  return features::IsSameAppWindowCycleEnabled() && CanCycleMru();
}

bool CanCycleUser() {
  return Shell::Get()->session_controller()->NumberOfLoggedInUsers() > 1;
}

bool CanFindPipWidget() {
  return !!FindPipWidget();
}

bool CanFocusCameraPreview() {
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

bool CanLock() {
  return Shell::Get()->session_controller()->CanLockScreen();
}

bool CanCreateSnapGroup() {
  return SnapGroupController::Get();
}

void CreateSnapGroup() {
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  CHECK(snap_group_controller);

  auto maybe_create_snap_group =
      [&](aura::Window* window1, aura::Window* window2,
          std::optional<base::TimeTicks> carry_over_creation_time) {
        if (auto* snap_group = snap_group_controller->AddSnapGroup(
                window1, window2, /*replace=*/false,
                carry_over_creation_time)) {
          // TODO(b/346624805): See if we can move this to `AddSnapGroup()`.
          // Currently needed since multiple places can refresh snap bounds.
          snap_group->RefreshSnapGroup();
        }
        if (snap_group_controller->AreWindowsInSnapGroup(window1, window2)) {
          Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
              AccessibilityAlert::SNAP_GROUP_CREATION);
        }
      };

  // Phase 1: Find the topmost two independent windows snapped on the opposite
  // sides.
  const std::optional<std::pair<aura::Window*, aura::Window*>>
      independent_windows = GetIndependentWindowPairForSnapGroupCreation();
  if (independent_windows.has_value()) {
    aura::Window* window1 = independent_windows->first;
    aura::Window* window2 = independent_windows->second;
    const WindowState* window1_state = WindowState::Get(window1);
    WindowStateType window1_state_type = window1_state->GetStateType();
    const auto window1_snap_ratio = window1_state->snap_ratio();

    const WindowState* window2_state = WindowState::Get(window2);
    WindowStateType window2_state_type = window2_state->GetStateType();
    const auto window2_snap_ratio = window2_state->snap_ratio();

    const bool snap_ratio_sum_equal_to_one =
        window1_snap_ratio.has_value() && window2_snap_ratio.has_value() &&
        base::IsApproximatelyEqual(*window1_snap_ratio + *window2_snap_ratio,
                                   1.f, std::numeric_limits<float>::epsilon());

    if (snap_ratio_sum_equal_to_one) {
      if (window1_state_type == WindowStateType::kPrimarySnapped &&
          window2_state_type == WindowStateType::kSecondarySnapped) {
        maybe_create_snap_group(window1, window2, std::nullopt);
        return;
      }

      if (window1_state_type == WindowStateType::kSecondarySnapped &&
          window2_state_type == WindowStateType::kPrimarySnapped) {
        maybe_create_snap_group(window2, window1, std::nullopt);
        return;
      }
    }
  }

  // Phase 2: Find topmost visible snapped window and a Snap Group underneath to
  // perform snap to replace.
  const std::optional<std::pair<aura::Window*, aura::Window*>>
      snap_to_replace_window_pair =
          snap_group_controller
              ->GetWindowPairForSnapToReplaceWithKeyboardShortcut();
  if (!snap_to_replace_window_pair.has_value()) {
    return;
  }

  SnapGroup* to_be_replaced_snap_group = nullptr;
  for (aura::Window* window : {snap_to_replace_window_pair->first,
                               snap_to_replace_window_pair->second}) {
    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      to_be_replaced_snap_group = snap_group;
      break;
    }
  }

  const base::TimeTicks carry_over_creation_time =
      to_be_replaced_snap_group->carry_over_creation_time();
  snap_group_controller->RemoveSnapGroup(to_be_replaced_snap_group,
                                         SnapGroupExitPoint::kSnapToReplace);

  maybe_create_snap_group(snap_to_replace_window_pair->first,
                          snap_to_replace_window_pair->second,
                          carry_over_creation_time);
}

bool CanMinimizeTopWindowOnBack() {
  return window_util::ShouldMinimizeTopWindowOnBack();
}

bool CanMoveActiveWindowBetweenDisplays() {
  return display_move_window_util::CanHandleMoveActiveWindowBetweenDisplays();
}

bool CanPerformMagnifierZoom() {
  return Shell::Get()->fullscreen_magnifier_controller()->IsEnabled() ||
         Shell::Get()->docked_magnifier_controller()->GetEnabled();
}

bool CanResizePipWindow() {
  return Shell::Get()->pip_controller()->CanResizePip();
}

bool CanScreenshot(bool take_screenshot) {
  // |AcceleratorAction::kTakeScreenshot| is allowed when user session is
  // blocked.
  return take_screenshot ||
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

bool CanShowStylusTools() {
  return GetPaletteTray()->ShouldShowPalette();
}

bool CanStopScreenRecording() {
  return CaptureModeController::Get()->is_recording_in_progress();
}

bool CanSwapPrimaryDisplay() {
  return display::Screen::GetScreen()->GetNumDisplays() > 1;
}

bool CanTilingWindowResize() {
  if (!features::IsTilingWindowResizeEnabled()) {
    return false;
  }
  auto* controller = Shell::Get()->window_tiling_controller();
  return controller && controller->CanTilingResize(GetTargetWindow());
}

bool CanEnableOrToggleDictation() {
  return true;
}

bool CanToggleFloatingWindow() {
  return GetTargetWindow() != nullptr;
}

bool CanToggleGameDashboard() {
  if (!features::IsGameDashboardEnabled()) {
    return false;
  }
  aura::Window* window = GetTargetWindow();
  return window && GameDashboardController::ReadyForAccelerator(window);
}

bool CanToggleMultitaskMenu() {
  aura::Window* window = GetTargetWindow();
  if (!window) {
    return false;
  }
  if (display::Screen::GetScreen()->InTabletMode()) {
    // In tablet mode, the window just has to be able to maximize.
    return WindowState::Get(window)->CanMaximize();
  }
  // If the active window has a visible size button, the menu can be opened.
  if (auto* size_button = GetFrameSizeButton(window);
      size_button && size_button->GetVisible()) {
    return true;
  }
  // Else if the transient parent is showing the multitask menu, the menu can be
  // closed.
  auto* transient_parent = wm::GetTransientParent(window);
  auto* size_button = GetFrameSizeButton(transient_parent);
  return size_button && size_button->IsMultitaskMenuShown();
}

bool CanToggleOverview() {
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  // Do not toggle overview if there is a window being dragged.
  for (aura::Window* window : windows) {
    if (WindowState::Get(window)->is_dragged())
      return false;
  }
  return true;
}

bool CanTogglePicker() {
  CHECK(Shell::HasInstance());
  return features::IsPickerUpdateEnabled() &&
         Shell::Get()->picker_controller()->IsFeatureEnabled();
}

bool CanTogglePrivacyScreen() {
  CHECK(Shell::HasInstance());
  return Shell::Get()->privacy_screen_controller()->IsSupported();
}

bool CanToggleProjectorMarker() {
  auto* annotator_controller = AnnotatorControllerBase::Get();
  return annotator_controller &&
         annotator_controller->GetAnnotatorAvailability();
}

bool CanToggleResizeLockMenu() {
  aura::Window* window = GetTargetWindow();
  if (!window) {
    return false;
  }
  auto* frame_view = NonClientFrameViewAsh::Get(window);
  return frame_view && frame_view->GetToggleResizeLockMenuCallback();
}

bool CanUnpinWindow() {
  // WindowStateType::kTrustedPinned does not allow the user to press a key to
  // exit pinned mode.
  WindowState* window_state = WindowState::ForActiveWindow();
  return window_state &&
         window_state->GetStateType() == WindowStateType::kPinned;
}

bool CanWindowSnap() {
  aura::Window* window = GetTargetWindow();
  if (!window) {
    return false;
  }
  WindowState* window_state = WindowState::Get(window);
  return window_state && window_state->IsUserPositionable();
}

void AccessibilityAction() {
  Shell::Get()->accessibility_controller()->PerformAccessibilityAction();
}

void ActivateDesk(bool activate_left) {
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

void ActivateDeskAtIndex(AcceleratorAction action) {
  DCHECK_GE(action, AcceleratorAction::kDesksActivate0);
  DCHECK_LE(action, AcceleratorAction::kDesksActivate7);
  const size_t target_index = action - AcceleratorAction::kDesksActivate0;
  auto* desks_controller = DesksController::Get();
  // Only 1 desk animation can occur at a time so ignore this action if there's
  // an ongoing desk animation.
  if (desks_controller->AreDesksBeingModified())
    return;

  const auto& desks = desks_controller->desks();
  if (target_index < desks.size()) {
    // Record which desk users switch to.
    RecordActivateDeskByIndexAcceleratorAction(
        static_cast<ActivateDeskAcceleratorAction>(target_index));
    desks_controller->ActivateDesk(
        desks[target_index].get(),
        DesksSwitchSource::kIndexedDeskSwitchShortcut);
  } else {
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      desks_animations::PerformHitTheWallAnimation(root, /*going_left=*/false);
    }
  }
}

void ActiveMagnifierZoom(int delta_index) {
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

void BrightnessDown() {
  BrightnessControlDelegate* delegate =
      Shell::Get()->brightness_control_delegate();
  if (delegate)
    delegate->HandleBrightnessDown();
}

void BrightnessUp() {
  BrightnessControlDelegate* delegate =
      Shell::Get()->brightness_control_delegate();
  if (delegate)
    delegate->HandleBrightnessUp();
}

void CycleBackwardMru(bool same_app_only) {
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward, same_app_only);
}

void CycleForwardMru(bool same_app_only) {
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward, same_app_only);
}

void CycleUser(CycleUserDirection direction) {
  Shell::Get()->session_controller()->CycleActiveUser(direction);
}

void DisableCapsLock() {
  Shell::Get()->ime_controller()->SetCapsLockEnabled(false);
}

void FocusCameraPreview() {
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  DCHECK(camera_controller);
  camera_controller->PseudoFocusCameraPreview();
}

void FocusPip() {
  auto* widget = FindPipWidget();
  if (widget)
    Shell::Get()->focus_cycler()->FocusWidget(widget);
}

void FocusShelf() {
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

void KeyboardBrightnessDown() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  if (delegate)
    delegate->HandleKeyboardBrightnessDown();
}

void KeyboardBrightnessUp() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  if (delegate)
    delegate->HandleKeyboardBrightnessUp();
}

void LaunchAppN(int n) {
  Shelf::LaunchShelfItem(n);
}

void LaunchLastApp() {
  Shelf::LaunchShelfItem(-1);
}

void LockPressed(bool pressed) {
  Shell::Get()->power_button_controller()->OnLockButtonEvent(pressed,
                                                             base::TimeTicks());
}

void LockScreen() {
  Shell::Get()->session_controller()->LockScreen();
}

void MaybeTakePartialScreenshot() {
  base::RecordAction(base::UserMetricsAction("Accel_Take_Partial_Screenshot"));
  EnterImageCaptureMode(CaptureModeSource::kRegion,
                        CaptureModeEntryType::kAccelTakePartialScreenshot);
}

void MaybeTakeWindowScreenshot() {
  // If a capture mode session is already running, this shortcut will be treated
  // as a no-op.
  if (CaptureModeController::Get()->IsActive())
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Take_Window_Screenshot"));
  EnterImageCaptureMode(CaptureModeSource::kWindow,
                        CaptureModeEntryType::kAccelTakeWindowScreenshot);
}

void MediaFastForward() {
  Shell::Get()->media_controller()->HandleMediaSeekForward();
}

void MediaNextTrack() {
  Shell::Get()->media_controller()->HandleMediaNextTrack();
}

void MediaPause() {
  Shell::Get()->media_controller()->HandleMediaPause();
}

void MediaPlay() {
  Shell::Get()->media_controller()->HandleMediaPlay();
}

void MediaPlayPause() {
  Shell::Get()->media_controller()->HandleMediaPlayPause();
}

void MediaPrevTrack() {
  Shell::Get()->media_controller()->HandleMediaPrevTrack();
}
void MediaRewind() {
  Shell::Get()->media_controller()->HandleMediaSeekBackward();
}

void MediaStop() {
  Shell::Get()->media_controller()->HandleMediaStop();
}

void MicrophoneMuteToggle() {
  auto* const audio_handler = CrasAudioHandler::Get();
  const bool mute = !audio_handler->IsInputMuted();

  if (mute)
    base::RecordAction(base::UserMetricsAction("Keyboard_Microphone_Muted"));
  else
    base::RecordAction(base::UserMetricsAction("Keyboard_Microphone_Unmuted"));

  audio_handler->SetInputMute(
      mute, CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
}

void MoveActiveItem(bool going_left) {
  auto* desks_controller = DesksController::Get();
  if (desks_controller->AreDesksBeingModified())
    return;

  aura::Window* window_to_move = nullptr;
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (in_overview) {
    window_to_move =
        overview_controller->overview_session()->GetFocusedWindow();
  } else {
    window_to_move = GetTargetWindow();
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

void MoveActiveWindowBetweenDisplays() {
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
}

void NewDesk() {
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

void NewIncognitoWindow() {
  NewWindowDelegate::GetPrimary()->NewWindow(
      /*is_incognito=*/true,
      /*should_trigger_session_restore=*/false);
}

void NewTab() {
  NewWindowDelegate::GetPrimary()->NewTab();
}

void NewWindow() {
  NewWindowDelegate::GetPrimary()->NewWindow(
      /*is_incognito=*/false,
      /*should_trigger_session_restore=*/false);
}

void OpenCalculator() {
  NewWindowDelegate::GetInstance()->OpenCalculator();
}

void OpenCrosh() {
  NewWindowDelegate::GetInstance()->OpenCrosh();
}

void OpenDiagnostics() {
  NewWindowDelegate::GetInstance()->OpenDiagnostics();
}

void OpenFeedbackPage() {
  NewWindowDelegate::GetInstance()->OpenFeedbackPage();
}

void OpenFileManager() {
  NewWindowDelegate::GetInstance()->OpenFileManager();
}

void OpenHelp() {
  NewWindowDelegate::GetInstance()->OpenGetHelp();
}

void PerformTilingWindowResize(AcceleratorAction action) {
  if (!features::IsTilingWindowResizeEnabled()) {
    return;
  }
  auto* controller = Shell::Get()->window_tiling_controller();
  switch (action) {
    case kTilingWindowResizeLeft:
      controller->OnTilingResizeLeft(GetTargetWindow());
      return;
    case kTilingWindowResizeRight:
      controller->OnTilingResizeRight(GetTargetWindow());
      return;
    case kTilingWindowResizeUp:
      controller->OnTilingResizeUp(GetTargetWindow());
      return;
    case kTilingWindowResizeDown:
      controller->OnTilingResizeDown(GetTargetWindow());
      return;
    default:
      return;
  }
}

void PowerPressed(bool pressed) {
  Shell::Get()->power_button_controller()->OnPowerButtonEvent(
      pressed, base::TimeTicks());
}

void RecordVolumeSource() {
  base::UmaHistogramEnumeration(
      CrasAudioHandler::kOutputVolumeChangedSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kAccelerator);
}

void RemoveCurrentDesk() {
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

void ResetDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  display_manager->ResetDisplayZoom(display.id());
}

void ResizePipWindow() {
  Shell::Get()->pip_controller()->HandleKeyboardShortcut();
}

void RestoreTab() {
  NewWindowDelegate::GetPrimary()->RestoreTab();
}

void RotateActiveWindow() {
  aura::Window* window = GetTargetWindow();
  if (!window) {
    return;
  }
  // The rotation animation bases its target transform on the current
  // rotation and position. Since there could be an animation in progress
  // right now, queue this animation so when it starts it picks up a neutral
  // rotation and position. Use replace so we only enqueue one at a time.
  window->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  window->layer()->GetAnimator()->StartAnimation(new ui::LayerAnimationSequence(
      std::make_unique<WindowRotation>(360, window->layer())));
}

void RotatePaneFocus(FocusCycler::Direction direction) {
  Shell::Get()->focus_cycler()->RotateFocus(direction);
}

void RotateScreen() {
  if (Shell::Get()->display_manager()->IsInUnifiedMode())
    return;

  base::RecordAction(UserMetricsAction("Accel_Rotate_Screen"));
  const bool dialog_ever_accepted =
      Shell::Get()
          ->accessibility_controller()
          ->HasDisplayRotationAcceleratorDialogBeenAccepted();

  if (!dialog_ever_accepted) {
    Shell::Get()->accessibility_controller()->ShowConfirmationDialog(
        l10n_util::GetStringUTF16(IDS_ASH_ROTATE_SCREEN_TITLE),
        l10n_util::GetStringUTF16(IDS_ASH_ROTATE_SCREEN_BODY),
        l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON),
        l10n_util::GetStringUTF16(IDS_APP_CANCEL),
        base::BindOnce(&OnRotationDialogAccepted),
        base::BindOnce(&OnRotationDialogCancelled),
        /*on_close_callback=*/base::DoNothing());
  } else {
    RecordRotationAcceleratorAction(
        RotationAcceleratorAction::kAlreadyAcceptedDialog);
    RotateScreenImpl();
  }
}

void ShiftPrimaryDisplay() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  CHECK_GE(display_manager->GetNumDisplays(), 2U);

  const int64_t primary_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();

  const display::Displays& active_display_list =
      display_manager->active_display_list();

  auto primary_display_iter = base::ranges::find(
      active_display_list, primary_display_id, &display::Display::id);

  DCHECK(primary_display_iter != active_display_list.end());

  ++primary_display_iter;

  // If we've reach the end of |active_display_list|, wrap back around to the
  // front.
  if (primary_display_iter == active_display_list.end())
    primary_display_iter = active_display_list.begin();

  Shell::Get()->display_configuration_controller()->SetPrimaryDisplayId(
      primary_display_iter->id(), true /* throttle */);
}

void ShowEmojiPicker(const base::TimeTicks accelerator_timestamp) {
  ui::ShowEmojiPanel();
}

void ShowShortcutCustomizationApp() {
  NewWindowDelegate::GetInstance()->ShowShortcutCustomizationApp();
}

void ShowTaskManager() {
  NewWindowDelegate::GetInstance()->ShowTaskManager();
}

void StopScreenRecording() {
  CaptureModeController* controller = CaptureModeController::Get();
  CHECK(controller->is_recording_in_progress());
  controller->EndVideoRecording(EndRecordingReason::kKeyboardShortcut);
}

void Suspend() {
  chromeos::PowerManagerClient::Get()->RequestSuspend(
      /*wakeup_count=*/std::nullopt, /*duration_secs=*/0,
      power_manager::REQUEST_SUSPEND_DEFAULT);
}

void SwitchToNextIme() {
  Shell::Get()->ime_controller()->SwitchToNextIme();
}

void SwitchToLastUsedIme(bool key_pressed) {
  if (key_pressed) {
    Shell::Get()->ime_controller()->SwitchToLastUsedIme();
  }
  // Else: consume the Ctrl+Space EventType::kKeyReleased event but do not do
  // anything.
}

void ToggleAppList(AppListShowSource show_source,
                   base::TimeTicks event_time_stamp) {
  aura::Window* const root_window = Shell::GetRootWindowForNewWindows();
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window).id(),
      show_source, event_time_stamp);
}

void TakeScreenshot(bool from_snapshot_key) {
  // If it is the snip key, toggle capture mode unless the session is blocked,
  // in which case, it behaves like a fullscreen screenshot.
  auto* capture_mode_controller = CaptureModeController::Get();
  if (from_snapshot_key &&
      !Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    if (capture_mode_controller->IsActive())
      capture_mode_controller->Stop();
    else
      capture_mode_controller->Start(CaptureModeEntryType::kSnipKey);
    return;
  }
  capture_mode_controller->CaptureScreenshotsOfAllDisplays();
}

void ToggleAssignToAllDesk() {
  auto* window = GetTargetWindow();
  if (!window) {
    return;
  }

  // TODO(b/267363112): Allow a floated window to be assigned to all desks.
  // Only children of the desk container should have their assigned to all
  // desks state toggled to avoid interfering with special windows like
  // always-on-top windows, floated windows, etc.
  if (desks_util::IsActiveDeskContainer(window->parent())) {
    const bool is_already_visible_on_all_desks =
        desks_util::IsWindowVisibleOnAllWorkspaces(window);
    if (!is_already_visible_on_all_desks) {
      UMA_HISTOGRAM_ENUMERATION(
          chromeos::kDesksAssignToAllDesksSourceHistogramName,
          chromeos::DesksAssignToAllDesksSource::kKeyboardShortcut);
    }

    window->SetProperty(
        aura::client::kWindowWorkspaceKey,
        is_already_visible_on_all_desks
            ? aura::client::kWindowWorkspaceUnassignedWorkspace
            : aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  }
}

void ToggleAssistant() {
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
      // Show a toast if the Assistant is disabled due to being in Incognito
      // mode.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_GUEST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE:
      // Show a toast if the Assistant is disabled due to the account type.
      ShowToast(kAssistantErrorToastId, ToastCatalogName::kAssistantError,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_BY_ACCOUNT_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE:
      // No need to show toast in KIOSK mode.
      return;
    case AssistantAllowedState::DISALLOWED_BY_NO_BINARY:
      // No need to show toast.
      return;
    case AssistantAllowedState::ALLOWED:
      // Nothing need to do if allowed.
      break;
  }
  AssistantUiController::Get()->ToggleUi(
      /*entry_point=*/assistant::AssistantEntryPoint::kHotkey,
      /*exit_point=*/assistant::AssistantExitPoint::kHotkey);
}

void ToggleCalendar() {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  StatusAreaWidget* status_area_widget =
      RootWindowController::ForWindow(target_root)->GetStatusAreaWidget();

  DateTray* date_tray = status_area_widget->date_tray();
  GlanceablesController* const glanceables_controller =
      Shell::Get()->glanceables_controller();
  if (glanceables_controller &&
      glanceables_controller->AreGlanceablesAvailable()) {
    if (date_tray->is_active()) {
      date_tray->HideGlanceableBubble();
    } else {
      date_tray->ShowGlanceableBubble(/*from_keyboard=*/true);
    }
    return;
  }

  UnifiedSystemTray* tray = status_area_widget->unified_system_tray();
  // If currently showing the calendar view, close it.
  if (tray->IsShowingCalendarView()) {
    tray->CloseBubble();
    return;
  }

  // If currently not showing the calendar view, show the bubble if needed then
  // show the calendar view.
  if (!tray->IsBubbleShown()) {
    // Set `DateTray` to be active prior to showing the bubble, this prevents
    // flashing of the status area. See crbug.com/1332603.
    status_area_widget->date_tray()->SetIsActive(true);
    tray->ShowBubble();
  }

  tray->bubble()->ShowCalendarView(
      calendar_metrics::CalendarViewShowSource::kAccelerator,
      calendar_metrics::CalendarEventSource::kKeyboard);
}

void ToggleCapsLock() {
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  ime_controller->SetCapsLockEnabled(!ime_controller->IsCapsLockEnabled());
}

void ToggleClipboardHistory(bool is_plain_text_paste) {
  DCHECK(Shell::Get()->clipboard_history_controller());
  Shell::Get()->clipboard_history_controller()->ToggleMenuShownByAccelerator(
      is_plain_text_paste);
}

void TogglePicker(base::TimeTicks accelerator_timestamp) {
  const bool outside_user_session =
      !Shell::Get()->session_controller()->IsActiveUserSessionStarted();
  const bool is_oobe = Shell::Get()->session_controller()->GetSessionState() ==
                       session_manager::SessionState::OOBE;
  const bool is_modal_window = Shell::IsSystemModalWindowOpen();
  if (outside_user_session || is_oobe || is_modal_window) {
    ToggleCapsLock();
    RecordTogglePickerAcceleratorAction(TogglePickerAction::kToggleCapsLock);
    return;
  }

  CHECK(Shell::Get()->picker_controller());
  if (auto* picker_controller = Shell::Get()->picker_controller()) {
    picker_controller->ToggleWidget(accelerator_timestamp);
    RecordTogglePickerAcceleratorAction(TogglePickerAction::kTogglePicker);
  }
}

void EnableSelectToSpeak() {
  Shell::Get()->accessibility_controller()->EnableSelectToSpeakWithDialog();
}

void EnableOrToggleDictation() {
  Shell::Get()->accessibility_controller()->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
}

void ToggleDockedMagnifier() {
  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kDockedMagnifierEnabled);

  Shell* shell = Shell::Get();

  RemoveDockedMagnifierNotification();
  if (!is_shortcut_enabled) {
    ShowDockedMagnifierDisabledByAdminNotification(
        shell->docked_magnifier_controller()->GetEnabled());
    return;
  }

  DockedMagnifierController* docked_magnifier_controller =
      shell->docked_magnifier_controller();
  AccessibilityController* accessibility_controller =
      shell->accessibility_controller();

  const bool current_enabled = docked_magnifier_controller->GetEnabled();
  const bool dialog_ever_accepted =
      accessibility_controller->docked_magnifier().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    accessibility_controller->ShowConfirmationDialog(
        l10n_util::GetStringUTF16(IDS_ASH_DOCKED_MAGNIFIER_TITLE),
        l10n_util::GetStringUTF16(IDS_ASH_DOCKED_MAGNIFIER_BODY),
        l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON),
        l10n_util::GetStringUTF16(IDS_APP_CANCEL), base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->docked_magnifier()
              .SetDialogAccepted();
          SetDockedMagnifierEnabled(true);
        }),
        /*on_cancel_callback=*/base::DoNothing(),
        /*on_close_callback=*/base::DoNothing());
  } else {
    SetDockedMagnifierEnabled(!current_enabled);
  }
}

void ToggleFloating() {
  aura::Window* window = GetTargetWindow();
  DCHECK(window);
  // `CanFloatWindow` check is placed here rather than
  // `CanToggleFloatingWindow` as otherwise the bounce would not behave
  // properly.
  if (!chromeos::wm::CanFloatWindow(window)) {
    wm::AnimateWindow(window, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    return;
  }
  Shell::Get()->float_controller()->ToggleFloat(window);
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Floating"));
}

void ToggleFullscreen() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  // Disable fullscreen while overview animation is running due to
  // http://crbug.com/1094739
  if (overview_controller->IsInStartAnimation())
    return;
  aura::Window* window = GetTargetWindow();
  if (!window) {
    return;
  }
  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window)->OnWMEvent(&event);
}

void ToggleFullscreenMagnifier() {
  const bool is_shortcut_enabled = IsAccessibilityShortcutEnabled(
      prefs::kAccessibilityScreenMagnifierEnabled);

  Shell* shell = Shell::Get();

  RemoveFullscreenMagnifierNotification();
  if (!is_shortcut_enabled) {
    ShowFullscreenMagnifierDisabledByAdminNotification(
        shell->fullscreen_magnifier_controller()->IsEnabled());
    return;
  }

  FullscreenMagnifierController* magnification_controller =
      shell->fullscreen_magnifier_controller();
  AccessibilityController* accessibility_controller =
      shell->accessibility_controller();

  const bool current_enabled = magnification_controller->IsEnabled();
  const bool dialog_ever_accepted =
      accessibility_controller->fullscreen_magnifier().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    // Enable fullscreen magnifier before showing the dialog, so that users
    // can see the dialog more clearly.
    bool magnify_dialog =
        ::features::IsAccessibilityMagnifyAcceleratorDialogEnabled();
    int title = IDS_ASH_SCREEN_MAGNIFIER_TITLE;
    std::u16string body =
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_MAGNIFIER_BODY);
    int cancel = IDS_APP_CANCEL;
    int confirm = IDS_ASH_CONTINUE_BUTTON;
    if (magnify_dialog) {
      Shell::Get()->fullscreen_magnifier_controller()->SetEnabled(true);
      title = IDS_ASH_SCREEN_MAGNIFIER_DIALOG_TITLE;
      cancel = IDS_ASH_SCREEN_MAGNIFIER_DIALOG_TURN_OFF_BUTTON;
      confirm = IDS_ASH_SCREEN_MAGNIFIER_DIALOG_KEEP_ON_BUTTON;

      std::vector<AcceleratorLookup::AcceleratorDetails> zoom_in_details =
          Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
              AcceleratorAction::kMagnifierZoomIn);
      std::vector<AcceleratorLookup::AcceleratorDetails> zoom_out_details =
          Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
              AcceleratorAction::kMagnifierZoomOut);
      if (zoom_in_details.empty() || zoom_out_details.empty()) {
        body = l10n_util::GetStringUTF16(IDS_ASH_SCREEN_MAGNIFIER_DIALOG_BODY);
      } else {
        std::u16string zoom_in_text =
            AcceleratorLookup::GetAcceleratorDetailsText(zoom_in_details[0]);
        std::u16string zoom_out_text =
            AcceleratorLookup::GetAcceleratorDetailsText(zoom_out_details[0]);
        body = l10n_util::GetStringFUTF16(
            IDS_ASH_SCREEN_MAGNIFIER_DIALOG_BODY_DYNAMIC, zoom_in_text,
            zoom_out_text);
      }
    }
    accessibility_controller->ShowConfirmationDialog(
        l10n_util::GetStringUTF16(title), body,
        l10n_util::GetStringUTF16(confirm), l10n_util::GetStringUTF16(cancel),
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->fullscreen_magnifier()
              .SetDialogAccepted();
          SetFullscreenMagnifierEnabled(true);
        }),
        /*on_cancel_callback=*/base::BindOnce([]() {
          Shell::Get()->fullscreen_magnifier_controller()->SetEnabled(false);
        }),
        /*on_close_callback=*/base::BindOnce([]() {
          Shell::Get()->fullscreen_magnifier_controller()->SetEnabled(false);
        }));
    // Center the magnifier on the new dialog. This is done manually because the
    // dialog focus may change before the AccessibilityCommon extension has
    // loaded and begun listening for focus events.
    magnification_controller->HandleMoveMagnifierToRect(
        accessibility_controller->GetConfirmationDialogBoundsInScreen());
  } else {
    SetFullscreenMagnifierEnabled(!current_enabled);
  }
}

void ToggleGameDashboard() {
  DCHECK(features::IsGameDashboardEnabled());
  aura::Window* window = GetTargetWindow();
  DCHECK(window);
  if (auto* context =
          GameDashboardController::Get()->GetGameDashboardContext(window)) {
    context->ToggleMainMenuByAccelerator();
  }
}

void ToggleHighContrast() {
  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kAccessibilityHighContrastEnabled);

  Shell* shell = Shell::Get();

  RemoveHighContrastNotification();
  if (!is_shortcut_enabled) {
    ShowHighContrastDisabledByAdminNotification(
        shell->accessibility_controller()->high_contrast().enabled());
    return;
  }

  AccessibilityController* controller = shell->accessibility_controller();
  const bool current_enabled = controller->high_contrast().enabled();
  const bool dialog_ever_accepted =
      controller->high_contrast().WasDialogAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    controller->ShowConfirmationDialog(
        l10n_util::GetStringUTF16(IDS_ASH_HIGH_CONTRAST_TITLE),
        l10n_util::GetStringUTF16(IDS_ASH_HIGH_CONTRAST_BODY),
        l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON),
        l10n_util::GetStringUTF16(IDS_APP_CANCEL), base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->high_contrast()
              .SetDialogAccepted();
          SetHighContrastEnabled(true);
        }),
        /*on_cancel_callback=*/base::DoNothing(),
        /*on_close_callback=*/base::DoNothing());
  } else {
    SetHighContrastEnabled(!current_enabled);
  }
}

void ToggleSpokenFeedback() {
  const bool is_shortcut_enabled = IsAccessibilityShortcutEnabled(
      prefs::kAccessibilitySpokenFeedbackEnabled);

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

void ToggleImeMenuBubble() {
  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetStatusAreaWidget();
  if (status_area_widget) {
    ToggleTray(status_area_widget->ime_menu_tray());
  }
}

void ToggleKeyboardBacklight() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  delegate->HandleToggleKeyboardBacklight();
}

void ToggleMaximized() {
  aura::Window* window = GetTargetWindow();
  if (!window) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(window)->OnWMEvent(&event);
}

bool ToggleMinimized() {
  aura::Window* window = window_util::GetTopWindow();
  if (!window) {
    return false;
  }
  if (auto* overview_controller = Shell::Get()->overview_controller();
      overview_controller->InOverviewSession() &&
      overview_controller->overview_session()->IsWindowInOverview(window)) {
    return false;
  }
  WindowState* window_state = WindowState::Get(window);
  if (window_state->IsMinimized()) {
    // Attempt to restore the top window, i.e. the window that would be cycled
    // through next from the launcher.
    window_state->Activate();
    return true;
  }
  if (!window_state->CanMinimize()) {
    return false;
  }
  window_state->Minimize();
  return true;
}

void ToggleMouseKeys() {
  Shell::Get()->accessibility_controller()->ToggleMouseKeys();
}

void ToggleSnapGroupsMinimize() {
  // TODO(b/333772909): Remove this workaroound to disable shortcut when the
  // mojom conversion is disabled for deprecated shortcuts.
  base::DoNothing();
}

void ToggleResizeLockMenu() {
  aura::Window* window = GetTargetWindow();
  auto* frame_view = NonClientFrameViewAsh::Get(window);
  frame_view->GetToggleResizeLockMenuCallback().Run();
}

void ToggleMessageCenterBubble() {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  NotificationCenterTray* tray = RootWindowController::ForWindow(target_root)
                                     ->GetStatusAreaWidget()
                                     ->notification_center_tray();

  // Show a toast if there are no notifications.
  if (!tray->GetVisible()) {
    ShowToast(kNotificationCenterTrayNoNotificationsToastId,
              ash::ToastCatalogName::kNotificationCenterTrayNoNotifications,
              l10n_util::GetStringUTF16(
                  IDS_ASH_MESSAGE_CENTER_ACCELERATOR_NO_NOTIFICATIONS));
    return;
  }

  if (tray->GetBubbleWidget()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble();
  }
}

void ToggleMirrorMode() {
  bool mirror = !Shell::Get()->display_manager()->IsInMirrorMode();
  Shell::Get()->display_configuration_controller()->SetMirrorMode(
      mirror, true /* throttle */);
}

void ToggleMultitaskMenu() {
  aura::Window* window = GetTargetWindow();
  DCHECK(window);
  if (display::Screen::GetScreen()->InTabletMode()) {
    auto* multitask_menu_controller =
        Shell::Get()
            ->tablet_mode_controller()
            ->tablet_mode_window_manager()
            ->tablet_mode_multitask_menu_controller();
    // Does nothing if the menu is already shown.
    multitask_menu_controller->ShowMultitaskMenu(window);
    return;
  }
  auto* frame_view = NonClientFrameViewAsh::Get(window);
  if (!frame_view) {
    // If `window` doesn't have a frame, it must be the multitask menu and have
    // a transient parent for `CanToggleMultitaskMenu()` to arrive here.
    auto* transient_parent = wm::GetTransientParent(window);
    DCHECK(transient_parent);
    frame_view = NonClientFrameViewAsh::Get(transient_parent);
  }
  DCHECK(frame_view);
  auto* size_button =
      frame_view->GetHeaderView()->caption_button_container()->size_button();
  static_cast<chromeos::FrameSizeButton*>(size_button)->ToggleMultitaskMenu();
}

void ToggleOverview() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview(OverviewEndAction::kAccelerator);
  else
    overview_controller->StartOverview(OverviewStartAction::kAccelerator);
}

void TogglePrivacyScreen() {
  PrivacyScreenController* controller =
      Shell::Get()->privacy_screen_controller();
  controller->SetEnabled(!controller->GetEnabled());
}

void ToggleProjectorMarker() {
  if (auto* annotator_controller = AnnotatorControllerBase::Get()) {
    annotator_controller->ToggleAnnotationTray();
  }
}

void ToggleStylusTools() {
  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetStatusAreaWidget();
  if (status_area_widget) {
    ToggleTray(status_area_widget->palette_tray());
  }
}

void ToggleSystemTrayBubble() {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  UnifiedSystemTray* tray = RootWindowController::ForWindow(target_root)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  if (tray->IsBubbleShown()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble();
    tray->ActivateBubble();
  }
}

void ToggleUnifiedDesktop() {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(
      !Shell::Get()->display_manager()->unified_desktop_enabled());
}

void ToggleWifi() {
  Shell::Get()->system_tray_notifier()->NotifyRequestToggleWifi();
}

void TopWindowMinimizeOnBack() {
  WindowState::Get(GetTargetWindow())->Minimize();
}

void TouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void TouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
}

void UnpinWindow() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    WindowState::Get(pinned_window)->Restore();
}

void VolumeDown() {
  auto* audio_handler = CrasAudioHandler::Get();

  // Only plays the audio if unmuted.
  if (!audio_handler->IsOutputMuted()) {
    AcceleratorController::PlayVolumeAdjustmentSound();
  }
  audio_handler->DecreaseOutputVolumeByOneStep(kStepPercentage);
}

void VolumeMute() {
  CrasAudioHandler::Get()->SetOutputMute(
      true, CrasAudioHandler::AudioSettingsChangeSource::kAccelerator);
}

void VolumeMuteToggle() {
  auto* audio_handler = CrasAudioHandler::Get();
  CHECK(audio_handler);
  audio_handler->SetOutputMute(
      !audio_handler->IsOutputMuted(),
      CrasAudioHandler::AudioSettingsChangeSource::kAccelerator);
}

void VolumeUp() {
  auto* audio_handler = CrasAudioHandler::Get();
  bool play_sound = false;
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputMute(false);
  }
  play_sound = audio_handler->GetOutputVolumePercent() != 100;
  audio_handler->IncreaseOutputVolumeByOneStep(kStepPercentage);

  if (play_sound) {
    AcceleratorController::PlayVolumeAdjustmentSound();
  }
}

void WindowMinimize() {
  ToggleMinimized();
}

void WindowSnap(AcceleratorAction action) {
  Shell* shell = Shell::Get();
  const bool in_tablet = display::Screen::GetScreen()->InTabletMode();
  const bool in_overview = shell->overview_controller()->InOverviewSession();
  if (action == AcceleratorAction::kWindowCycleSnapLeft) {
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

  aura::Window* window = GetTargetWindow();
  DCHECK(window);

  // For displays rotated 90 or 180 degrees, they are considered upside down.
  // Here, primary snap does not match physical left or top. The accelerators
  // should always match the physical left or top.
  const bool physical_left_or_top =
      (action == AcceleratorAction::kWindowCycleSnapLeft);
  chromeos::SnapDirection snap_direction =
      chromeos::GetSnapDirectionForWindow(window, physical_left_or_top);

  const WindowSnapWMEvent event(
      snap_direction == chromeos::SnapDirection::kPrimary
          ? WM_EVENT_CYCLE_SNAP_PRIMARY
          : WM_EVENT_CYCLE_SNAP_SECONDARY,
      WindowSnapActionSource::kKeyboardShortcutToSnap);
  WindowState::Get(window)->OnWMEvent(&event);
}

bool ZoomDisplay(bool up) {
  if (up)
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Up"));
  else
    base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Down"));

  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  return display_manager->ZoomDisplay(display.id(), up);
}

void TouchFingerprintSensor(int finger_id) {
  // This function only called with [1,3]. If the range is changed in
  // the caller AcceleratorControllerImpl::PerformAction function then
  // this should be changed accordingly.
  DCHECK(1 <= finger_id && finger_id <= 3);
  FakeBiodClient* client = FakeBiodClient::Get();
  if (!client) {
    LOG(ERROR) << "FakeBiod is not initialized.";
    return;
  }
  client->TouchFingerprintSensor(finger_id);
}

}  // namespace accelerators
}  // namespace ash
