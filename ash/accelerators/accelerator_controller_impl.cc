// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_confirmation_dialog.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/debug.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_move_window_util.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/focus_cycler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/media/media_controller_impl.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/window_rotation.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_accessibility_controller.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_type.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"

namespace ash {

const char kNotifierAccelerator[] = "ash.accelerator-controller";

const char kTabletCountOfVolumeAdjustType[] = "Tablet.CountOfVolumeAdjustType";

const char kHighContrastToggleAccelNotificationId[] =
    "chrome://settings/accessibility/highcontrast";
const char kDockedMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/dockedmagnifier";
const char kFullscreenMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/fullscreenmagnifier";
const char kSpokenFeedbackToggleAccelNotificationId[] =
    "chrome://settings/accessibility/spokenfeedback";

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

using base::UserMetricsAction;
using message_center::Notification;
using message_center::SystemNotificationWarningLevel;

// Toast id and duration for Assistant shortcuts.
constexpr char kAssistantErrorToastId[] = "assistant_error";
constexpr int kToastDurationMs = 2500;

constexpr char kVirtualDesksToastId[] = "virtual_desks_toast";

// Path of the json file that contains side volume button location info.
constexpr char kSideVolumeButtonLocationFilePath[] =
    "/usr/share/chromeos-assets/side_volume_button/location.json";

// The interval between two volume control actions within one volume adjust.
constexpr base::TimeDelta kVolumeAdjustTimeout =
    base::TimeDelta::FromSeconds(2);

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
// Records the result of triggering the rotation accelerator.
enum class RotationAcceleratorAction {
  kCancelledDialog = 0,
  kAcceptedDialog = 1,
  kAlreadyAcceptedDialog = 2,
  kMaxValue = kAlreadyAcceptedDialog,
};

void RecordRotationAcceleratorAction(const RotationAcceleratorAction& action) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Accelerators.Rotation.Usage", action);
}

void RecordWindowSnapAcceleratorAction(WindowSnapAcceleratorAction action) {
  UMA_HISTOGRAM_ENUMERATION(kAccelWindowSnap, action);
}

void RecordTabletVolumeAdjustTypeHistogram(TabletModeVolumeAdjustType type) {
  UMA_HISTOGRAM_ENUMERATION(kTabletCountOfVolumeAdjustType, type);
}

// Ensures that there are no word breaks at the "+"s in the shortcut texts such
// as "Ctrl+Shift+Space".
void EnsureNoWordBreaks(base::string16* shortcut_text) {
  std::vector<base::string16> keys =
      base::SplitString(*shortcut_text, base::ASCIIToUTF16("+"),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (keys.size() < 2U)
    return;

  // The plus sign surrounded by the word joiner to guarantee an non-breaking
  // shortcut.
  const base::string16 non_breaking_plus =
      base::UTF8ToUTF16("\xe2\x81\xa0+\xe2\x81\xa0");
  shortcut_text->clear();
  for (size_t i = 0; i < keys.size() - 1; ++i) {
    *shortcut_text += keys[i];
    *shortcut_text += non_breaking_plus;
  }

  *shortcut_text += keys.back();
}

// Gets the notification message after it formats it in such a way that there
// are no line breaks in the middle of the shortcut texts.
base::string16 GetNotificationText(int message_id,
                                   int old_shortcut_id,
                                   int new_shortcut_id) {
  base::string16 old_shortcut = l10n_util::GetStringUTF16(old_shortcut_id);
  base::string16 new_shortcut = l10n_util::GetStringUTF16(new_shortcut_id);
  EnsureNoWordBreaks(&old_shortcut);
  EnsureNoWordBreaks(&new_shortcut);

  return l10n_util::GetStringFUTF16(message_id, new_shortcut, old_shortcut);
}

// Shows a warning the user is using a deprecated accelerator.
void ShowDeprecatedAcceleratorNotification(const char* const notification_id,
                                           int message_id,
                                           int old_shortcut_id,
                                           int new_shortcut_id) {
  const base::string16 message =
      GetNotificationText(message_id, old_shortcut_id, new_shortcut_id);
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([]() {
            if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
              Shell::Get()->shell_delegate()->OpenKeyboardShortcutHelpPage();
          }));

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE), message,
      base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierAccelerator),
      message_center::RichNotificationData(), std::move(delegate),
      kNotificationKeyboardIcon, SystemNotificationWarningLevel::NORMAL);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void ShowToast(std::string id, const base::string16& text) {
  ToastData toast(id, text, kToastDurationMs, base::nullopt,
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

void HandleCycleBackwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_PrevWindow_Tab"));

  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::BACKWARD);
}

void HandleCycleForwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_NextWindow_Tab"));

  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::FORWARD);
}

void HandleActivateDesk(const ui::Accelerator& accelerator) {
  auto* desks_controller = DesksController::Get();
  const bool success = desks_controller->ActivateAdjacentDesk(
      /*going_left=*/
      (accelerator.key_code() == ui::VKEY_OEM_4 ||
       accelerator.key_code() == ui::VKEY_LEFT),
      DesksSwitchSource::kDeskSwitchShortcut);
  if (!success)
    return;

  switch (accelerator.key_code()) {
    case ui::VKEY_OEM_4:
    case ui::VKEY_LEFT:
      base::RecordAction(base::UserMetricsAction("Accel_Desks_ActivateLeft"));
      break;
    case ui::VKEY_OEM_6:
    case ui::VKEY_RIGHT:
      base::RecordAction(base::UserMetricsAction("Accel_Desks_ActivateRight"));
      break;

    default:
      NOTREACHED();
  }
}

void HandleMoveActiveItem(const ui::Accelerator& accelerator) {
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

  if (!window_to_move)
    return;

  Desk* target_desk = nullptr;
  bool going_left = accelerator.key_code() == ui::VKEY_OEM_4 ||
                    accelerator.key_code() == ui::VKEY_LEFT;
  if (going_left) {
    target_desk = desks_controller->GetPreviousDesk();
    base::RecordAction(base::UserMetricsAction("Accel_Desks_MoveWindowLeft"));
  } else {
    DCHECK(accelerator.key_code() == ui::VKEY_OEM_6 ||
           accelerator.key_code() == ui::VKEY_RIGHT);
    target_desk = desks_controller->GetNextDesk();
    base::RecordAction(base::UserMetricsAction("Accel_Desks_MoveWindowRight"));
  }

  if (!target_desk)
    return;

  if (!in_overview) {
    desks_animations::PerformWindowMoveToDeskAnimation(
        window_to_move,
        /*going_left=*/going_left);
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
    ShowToast(kVirtualDesksToastId,
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
    ShowToast(kVirtualDesksToastId,
              l10n_util::GetStringUTF16(IDS_ASH_DESKS_MIN_NUM_REACHED));
    return;
  }

  if (desks_controller->AreDesksBeingModified())
    return;

  // TODO(afakhry): Finalize the desk removal animation outside of overview with
  // UX. https://crbug.com/977434.
  desks_controller->RemoveDesk(desks_controller->active_desk(),
                               DesksCreationRemovalSource::kKeyboard);
  base::RecordAction(base::UserMetricsAction("Accel_Desks_RemoveDesk"));
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

views::Widget* FindPipWidget() {
  return Shell::Get()->focus_cycler()->FindWidget(
      base::BindRepeating([](views::Widget* widget) {
        return WindowState::Get(widget->GetNativeWindow())->IsPip();
      }));
}

void HandleFocusPip() {
  base::RecordAction(UserMetricsAction("Accel_Focus_Pip"));
  auto* widget = FindPipWidget();
  if (widget)
    Shell::Get()->focus_cycler()->FocusWidget(widget);
}

void HandleLaunchAppN(int n) {
  base::RecordAction(UserMetricsAction("Accel_Launch_App"));
  Shelf::LaunchShelfItem(n);
}

void HandleLaunchLastApp() {
  base::RecordAction(UserMetricsAction("Accel_Launch_Last_App"));
  Shelf::LaunchShelfItem(-1);
}

void HandleMediaNextTrack() {
  base::RecordAction(UserMetricsAction("Accel_Media_Next_Track"));
  Shell::Get()->media_controller()->HandleMediaNextTrack();
}

void HandleMediaFastForward() {
  base::RecordAction(UserMetricsAction("Accel_Media_Fast_Forward"));
  Shell::Get()->media_controller()->HandleMediaSeekForward();
}

void HandleMediaPause() {
  base::RecordAction(UserMetricsAction("Accel_Media_Pause"));
  Shell::Get()->media_controller()->HandleMediaPause();
}

void HandleMediaPlay() {
  base::RecordAction(UserMetricsAction("Accel_Media_Play"));
  Shell::Get()->media_controller()->HandleMediaPlay();
}

void HandleMediaPlayPause() {
  base::RecordAction(UserMetricsAction("Accel_Media_PlayPause"));
  Shell::Get()->media_controller()->HandleMediaPlayPause();
}

void HandleMediaPrevTrack() {
  base::RecordAction(UserMetricsAction("Accel_Media_Prev_Track"));
  Shell::Get()->media_controller()->HandleMediaPrevTrack();
}
void HandleMediaRewind() {
  base::RecordAction(UserMetricsAction("Accel_Media_Rewind"));
  Shell::Get()->media_controller()->HandleMediaSeekBackward();
}

void HandleMediaStop() {
  base::RecordAction(UserMetricsAction("Accel_Media_Stop"));
  Shell::Get()->media_controller()->HandleMediaStop();
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
  base::Optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  return user_type && *user_type != user_manager::USER_TYPE_GUEST;
}

void HandleNewIncognitoWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Incognito_Window"));
  NewWindowDelegate::GetInstance()->NewWindow(true /* is_incognito */);
}

void HandleNewTab(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_T)
    base::RecordAction(UserMetricsAction("Accel_NewTab_T"));
  NewWindowDelegate::GetInstance()->NewTab();
}

void HandleNewWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Window"));
  NewWindowDelegate::GetInstance()->NewWindow(false /* is_incognito */);
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

void HandleOpenFeedbackPage() {
  base::RecordAction(UserMetricsAction("Accel_Open_Feedback_Page"));
  NewWindowDelegate::GetInstance()->OpenFeedbackPage();
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

  if (!display::Display::HasInternalDisplay() ||
      display_id != display::Display::InternalDisplayId()) {
    return GetNextRotationInClamshell(current);
  }

  const OrientationLockType app_requested_lock =
      shell->screen_orientation_controller()
          ->GetCurrentAppRequestedOrientationLock();

  bool add_180_degrees = false;
  switch (app_requested_lock) {
    case OrientationLockType::kCurrent:
    case OrientationLockType::kLandscapePrimary:
    case OrientationLockType::kLandscapeSecondary:
    case OrientationLockType::kPortraitPrimary:
    case OrientationLockType::kPortraitSecondary:
    case OrientationLockType::kNatural:
      // Do not change the current orientation.
      return current;

    case OrientationLockType::kLandscape:
    case OrientationLockType::kPortrait:
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
  return display::Display::HasInternalDisplay() &&
         display_id == display::Display::InternalDisplayId() &&
         Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
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

  // When the device is in a physical tablet state, display rotation requests of
  // the internal display are treated as requests to lock the user rotation.
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

void HandleRestoreTab() {
  base::RecordAction(UserMetricsAction("Accel_Restore_Tab"));
  NewWindowDelegate::GetInstance()->RestoreTab();
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

bool CanHandleScreenshot() {
  // The old screenshot code will handle the different sessions in its own code.
  if (!features::IsCaptureModeEnabled())
    return true;

  return !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

// Tries to enter capture mode image type with |source|. Returns false if
// unsuccessful (capture mode disabled).
bool MaybeEnterImageCaptureMode(CaptureModeSource source) {
  if (!features::IsCaptureModeEnabled())
    return false;

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->SetSource(source);
  capture_mode_controller->SetType(CaptureModeType::kImage);
  capture_mode_controller->Start();
  return true;
}

void HandleTakeWindowScreenshot() {
  base::RecordAction(UserMetricsAction("Accel_Take_Window_Screenshot"));
  if (MaybeEnterImageCaptureMode(CaptureModeSource::kWindow))
    return;

  Shell::Get()->screenshot_controller()->StartWindowScreenshotSession();
}

void HandleTakePartialScreenshot() {
  base::RecordAction(UserMetricsAction("Accel_Take_Partial_Screenshot"));
  if (MaybeEnterImageCaptureMode(CaptureModeSource::kRegion))
    return;

  Shell::Get()->screenshot_controller()->StartPartialScreenshotSession(
      /*draw_overlay_immediately=*/true);
}

void HandleTakeScreenshot() {
  base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
  Shell::Get()->screenshot_controller()->TakeScreenshotForAllRootWindows();
}

void HandleToggleSystemTrayBubbleInternal(bool focus_message_center) {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  UnifiedSystemTray* tray = RootWindowController::ForWindow(target_root)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  if (tray->IsBubbleShown()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble(false /* show_by_click */);
    tray->ActivateBubble();

    if (focus_message_center)
      tray->FocusFirstNotification();
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

bool CanHandleToggleAppList(const ui::Accelerator& accelerator,
                            const ui::Accelerator& previous_accelerator) {
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

void HandleToggleFullscreen(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_MEDIA_LAUNCH_APP2)
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
    overview_controller->EndOverview();
  else
    overview_controller->StartOverview();
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
  // Disable window snapping shortcut key for full screen window due to
  // http://crbug.com/135487.
  return (window_state && window_state->IsUserPositionable() &&
          !window_state->IsFullscreen());
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

  const WMEvent event(action == WINDOW_CYCLE_SNAP_LEFT
                          ? WM_EVENT_CYCLE_SNAP_LEFT
                          : WM_EVENT_CYCLE_SNAP_RIGHT);
  aura::Window* active_window = window_util::GetActiveWindow();
  DCHECK(active_window);
  WindowState::Get(active_window)->OnWMEvent(&event);
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

void HandleShowImeMenuBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Ime_Menu_Bubble"));

  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetStatusAreaWidget();
  if (status_area_widget) {
    ImeMenuTray* ime_menu_tray = status_area_widget->ime_menu_tray();
    if (ime_menu_tray && ime_menu_tray->GetVisible() &&
        !ime_menu_tray->GetBubbleView()) {
      ime_menu_tray->ShowBubble(false /* show_by_click */);
    }
  }
}

void HandleCrosh() {
  base::RecordAction(UserMetricsAction("Accel_Open_Crosh"));

  NewWindowDelegate::GetInstance()->OpenCrosh();
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

void HandleDisableCapsLock() {
  base::RecordAction(UserMetricsAction("Accel_Disable_Caps_Lock"));
  Shell::Get()->ime_controller()->SetCapsLockEnabled(false);
}

void HandleFileManager() {
  base::RecordAction(UserMetricsAction("Accel_Open_File_Manager"));

  NewWindowDelegate::GetInstance()->OpenFileManager();
}

void HandleGetHelp() {
  NewWindowDelegate::GetInstance()->OpenGetHelp();
}

bool CanHandleLock() {
  return Shell::Get()->session_controller()->CanLockScreen();
}

void HandleLock() {
  base::RecordAction(UserMetricsAction("Accel_LockScreen_L"));
  Shell::Get()->session_controller()->LockScreen();
}

PaletteTray* GetPaletteTray() {
  return Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
      ->GetStatusAreaWidget()
      ->palette_tray();
}

void HandleShowStylusTools() {
  base::RecordAction(UserMetricsAction("Accel_Show_Stylus_Tools"));
  GetPaletteTray()->ShowBubble(false /* show_by_click */);
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
    if (ui::DeviceKeyboardHasAssistantKey())
      return;

    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_A"));
  } else if (accelerator.key_code() == ui::VKEY_ASSISTANT) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Assistant"));
  }

  using chromeos::assistant::AssistantAllowedState;
  switch (AssistantState::Get()->allowed_state().value_or(
      AssistantAllowedState::ALLOWED)) {
    case AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER:
      // Show a toast if the active user is not primary.
      ShowToast(kAssistantErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_SECONDARY_USER_TOAST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_LOCALE:
      // Show a toast if the Assistant is disabled due to unsupported
      // locales.
      ShowToast(kAssistantErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_LOCALE_UNSUPPORTED_TOAST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_POLICY:
      // Show a toast if the Assistant is disabled due to enterprise policy.
      ShowToast(kAssistantErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_BY_POLICY_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_DEMO_MODE:
      // Show a toast if the Assistant is disabled due to being in Demo
      // Mode.
      ShowToast(kAssistantErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_DEMO_MODE_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION:
      // Show a toast if the Assistant is disabled due to being in public
      // session.
      ShowToast(kAssistantErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_PUBLIC_SESSION_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER:
      // supervised user is deprecated, wait for the code clean up.
      NOTREACHED();
      return;
    case AssistantAllowedState::DISALLOWED_BY_INCOGNITO:
      ShowToast(kAssistantErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_ASSISTANT_DISABLED_IN_GUEST_MESSAGE));
      return;
    case AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE:
      ShowToast(kAssistantErrorToastId,
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
      /*entry_point=*/chromeos::assistant::AssistantEntryPoint::kHotkey,
      /*exit_point=*/chromeos::assistant::AssistantExitPoint::kHotkey);
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

  // This shortcust is set to be trigger on release. Either the current
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

void CreateAndShowStickyNotification(const base::string16& title,
                                     const base::string16& message,
                                     const std::string& notification_id,
                                     const gfx::VectorIcon& icon) {
  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierAccelerator),
      message_center::RichNotificationData(), nullptr, icon,
      SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void CreateAndShowStickyNotification(
    int title_id,
    int message_id,
    const std::string& notification_id,
    const gfx::VectorIcon& icon = kNotificationAccessibilityIcon) {
  CreateAndShowStickyNotification(l10n_util::GetStringUTF16(title_id),
                                  l10n_util::GetStringUTF16(message_id),
                                  notification_id, icon);
}

void NotifyAccessibilityFeatureDisabledByAdmin(
    int feature_name_id,
    bool feature_state,
    const std::string& notification_id) {
  const base::string16 organization_name =
      base::UTF8ToUTF16(Shell::Get()
                            ->system_tray_model()
                            ->enterprise_domain()
                            ->enterprise_display_domain());
  CreateAndShowStickyNotification(
      l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_FEATURE_SHORTCUT_DISABLED_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_ASH_ACCESSIBILITY_FEATURE_SHORTCUT_DISABLED_MSG,
          organization_name,
          l10n_util::GetStringUTF16(
              feature_state ? IDS_ASH_ACCESSIBILITY_FEATURE_ACTIVATED
                            : IDS_ASH_ACCESSIBILITY_FEATURE_DEACTIVATED),
          l10n_util::GetStringUTF16(feature_name_id)),
      notification_id, kLoginScreenEnterpriseIcon);
}

void RemoveStickyNotitification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           false /* by_user */);
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

  RemoveStickyNotitification(kDockedMagnifierToggleAccelNotificationId);
  if (shell->docked_magnifier_controller()->GetEnabled()) {
    CreateAndShowStickyNotification(IDS_DOCKED_MAGNIFIER_ACCEL_TITLE,
                                    IDS_DOCKED_MAGNIFIER_ACCEL_MSG,
                                    kDockedMagnifierToggleAccelNotificationId);
  }
}

void HandleToggleDockedMagnifier() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Docked_Magnifier"));

  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kDockedMagnifierEnabled);

  base::UmaHistogramBoolean(kAccessibilityDockedMagnifierShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveStickyNotitification(kDockedMagnifierToggleAccelNotificationId);
  if (!is_shortcut_enabled) {
    NotifyAccessibilityFeatureDisabledByAdmin(
        IDS_ASH_DOCKED_MAGNIFIER_SHORTCUT_DISABLED,
        shell->docked_magnifier_controller()->GetEnabled(),
        kDockedMagnifierToggleAccelNotificationId);
    return;
  }

  DockedMagnifierControllerImpl* docked_magnifier_controller =
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

  RemoveStickyNotitification(kFullscreenMagnifierToggleAccelNotificationId);
  if (shell->magnification_controller()->IsEnabled()) {
    CreateAndShowStickyNotification(
        IDS_FULLSCREEN_MAGNIFIER_ACCEL_TITLE,
        IDS_FULLSCREEN_MAGNIFIER_ACCEL_MSG,
        kFullscreenMagnifierToggleAccelNotificationId);
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

  RemoveStickyNotitification(kHighContrastToggleAccelNotificationId);
  if (shell->accessibility_controller()->high_contrast().enabled()) {
    CreateAndShowStickyNotification(IDS_HIGH_CONTRAST_ACCEL_TITLE,
                                    IDS_HIGH_CONTRAST_ACCEL_MSG,
                                    kHighContrastToggleAccelNotificationId);
  }
}

void HandleToggleHighContrast() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_High_Contrast"));

  const bool is_shortcut_enabled =
      IsAccessibilityShortcutEnabled(prefs::kAccessibilityHighContrastEnabled);

  base::UmaHistogramBoolean(kAccessibilityHighContrastShortcut,
                            is_shortcut_enabled);

  Shell* shell = Shell::Get();

  RemoveStickyNotitification(kHighContrastToggleAccelNotificationId);
  if (!is_shortcut_enabled) {
    NotifyAccessibilityFeatureDisabledByAdmin(
        IDS_ASH_HIGH_CONTRAST_SHORTCUT_DISABLED,
        shell->accessibility_controller()->high_contrast().enabled(),
        kHighContrastToggleAccelNotificationId);
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

  RemoveStickyNotitification(kFullscreenMagnifierToggleAccelNotificationId);
  if (!is_shortcut_enabled) {
    NotifyAccessibilityFeatureDisabledByAdmin(
        IDS_ASH_FULLSCREEN_MAGNIFIER_SHORTCUT_DISABLED,
        shell->magnification_controller()->IsEnabled(),
        kFullscreenMagnifierToggleAccelNotificationId);
    return;
  }

  MagnificationController* magnification_controller =
      shell->magnification_controller();
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

  RemoveStickyNotitification(kSpokenFeedbackToggleAccelNotificationId);
  if (!is_shortcut_enabled) {
    NotifyAccessibilityFeatureDisabledByAdmin(
        IDS_ASH_SPOKEN_FEEDBACK_SHORTCUT_DISABLED, old_value,
        kSpokenFeedbackToggleAccelNotificationId);
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

  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
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

  chromeos::CrasAudioHandler::Get()->SetOutputMute(true);
}

void HandleVolumeUp() {
  base::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));

  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
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
  return Shell::Get()->magnification_controller()->IsEnabled() ||
         Shell::Get()->docked_magnifier_controller()->GetEnabled();
}

// Change the scale of the active magnifier.
void HandleActiveMagnifierZoom(int delta_index) {
  if (Shell::Get()->magnification_controller()->IsEnabled()) {
    Shell::Get()->magnification_controller()->StepToNextScaleValue(delta_index);
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

void HandleTouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void HandleTouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
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

const DeprecatedAcceleratorData*
AcceleratorControllerImpl::TestApi::GetDeprecatedAcceleratorData(
    AcceleratorAction action) {
  auto it = controller_->actions_with_deprecations_.find(action);
  if (it == controller_->actions_with_deprecations_.end())
    return nullptr;

  return it->second;
}

AcceleratorConfirmationDialog*
AcceleratorControllerImpl::TestApi::GetConfirmationDialog() {
  return controller_->confirmation_dialog_.get();
}

void AcceleratorControllerImpl::TestApi::SetSideVolumeButtonFilePath(
    base::FilePath path) {
  controller_->side_volume_button_location_file_path_ = path;
}

void AcceleratorControllerImpl::TestApi::SetSideVolumeButtonLocation(
    const std::string& region,
    const std::string& side) {
  controller_->side_volume_button_location_.region = region;
  controller_->side_volume_button_location_.side = side;
}

AcceleratorControllerImpl::AcceleratorControllerImpl()
    : accelerator_manager_(std::make_unique<ui::AcceleratorManager>()),
      accelerator_history_(std::make_unique<ui::AcceleratorHistory>()),
      side_volume_button_location_file_path_(
          base::FilePath(kSideVolumeButtonLocationFilePath)) {
  Init();

  ParseSideVolumeButtonLocationInfo();
}

AcceleratorControllerImpl::~AcceleratorControllerImpl() = default;

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

bool AcceleratorControllerImpl::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator it =
      accelerators_.find(accelerator);
  return it != accelerators_.end() && CanPerformAction(it->second, accelerator);
}

bool AcceleratorControllerImpl::Process(const ui::Accelerator& accelerator) {
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorControllerImpl::IsDeprecated(
    const ui::Accelerator& accelerator) const {
  return deprecated_accelerators_.count(accelerator) != 0;
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
  accelerator_history()->StoreCurrentAccelerator(accelerator);

  auto itr = accelerators_.find(accelerator);
  if (itr == accelerators_.end())
    return false;  // Menu shouldn't be closed for an invalid accelerator.

  AcceleratorAction action = itr->second;
  return actions_keeping_menu_open_.count(action) == 0;
}

bool AcceleratorControllerImpl::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->IsRegistered(accelerator);
}

ui::AcceleratorHistory* AcceleratorControllerImpl::GetAcceleratorHistory() {
  return accelerator_history_.get();
}

bool AcceleratorControllerImpl::IsPreferred(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return preferred_actions_.find(iter->second) != preferred_actions_.end();
}

bool AcceleratorControllerImpl::IsReserved(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return reserved_actions_.find(iter->second) != reserved_actions_.end();
}

AcceleratorControllerImpl::AcceleratorProcessingRestriction
AcceleratorControllerImpl::GetCurrentAcceleratorRestriction() {
  return GetAcceleratorProcessingRestriction(-1);
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerImpl, ui::AcceleratorTarget implementation:

bool AcceleratorControllerImpl::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator it =
      accelerators_.find(accelerator);
  DCHECK(it != accelerators_.end());
  AcceleratorAction action = it->second;
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

  if (::features::IsNewShortcutMappingEnabled()) {
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
    accelerators_.insert(std::make_pair(accelerator, accelerators[i].action));
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
    accelerators_[deprecated_accelerator] = accelerator_data.action;
    deprecated_accelerators_.insert(deprecated_accelerator);
  }
  Register(ui_accelerators, this);
}

bool AcceleratorControllerImpl::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) const {
  if (accelerator.IsRepeat() && !repeatable_actions_.count(action))
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
    case DESKS_ACTIVATE_DESK:
    case DESKS_MOVE_ACTIVE_ITEM:
    case DESKS_NEW_DESK:
    case DESKS_REMOVE_CURRENT_DESK:
      return true;
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
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
    case DEV_TOGGLE_UNIFIED_DESKTOP:
      return debug::DeveloperAcceleratorsEnabled();
    case DISABLE_CAPS_LOCK:
      return CanHandleDisableCapsLock(previous_accelerator);
    case LOCK_SCREEN:
      return CanHandleLock();
    case MAGNIFIER_ZOOM_IN:
    case MAGNIFIER_ZOOM_OUT:
      return CanHandleActiveMagnifierZoom();
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
      return CanHandleToggleAppList(accelerator, previous_accelerator);
    case TOGGLE_CAPS_LOCK:
      return CanHandleToggleCapsLock(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
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
      return accelerators::CanUnpinWindow();
    case WINDOW_CYCLE_SNAP_LEFT:
    case WINDOW_CYCLE_SNAP_RIGHT:
      return CanHandleWindowSnap();
    case FOCUS_PIP:
      return !!FindPipWidget();
    case MINIMIZE_TOP_WINDOW_ON_BACK:
      return window_util::ShouldMinimizeTopWindowOnBack();
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
      return CanHandleScreenshot();

    // The following are always enabled.
    case BRIGHTNESS_DOWN:
    case BRIGHTNESS_UP:
    case EXIT:
    case FOCUS_NEXT_PANE:
    case FOCUS_PREVIOUS_PANE:
    case FOCUS_SHELF:
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
    case OPEN_CROSH:
    case OPEN_FEEDBACK_PAGE:
    case OPEN_FILE_MANAGER:
    case OPEN_GET_HELP:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case PRINT_UI_HIERARCHIES:
    case RESTORE_TAB:
    case ROTATE_WINDOW:
    case SHOW_IME_MENU_BUBBLE:
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
      HandleCycleBackwardMRU(accelerator);
      break;
    case CYCLE_FORWARD_MRU:
      HandleCycleForwardMRU(accelerator);
      break;
    case DESKS_ACTIVATE_DESK:
      HandleActivateDesk(accelerator);
      break;
    case DESKS_MOVE_ACTIVE_ITEM:
      HandleMoveActiveItem(accelerator);
      break;
    case DESKS_NEW_DESK:
      HandleNewDesk();
      break;
    case DESKS_REMOVE_CURRENT_DESK:
      HandleRemoveCurrentDesk();
      break;
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
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
    case DEV_TOGGLE_UNIFIED_DESKTOP:
      HandleToggleUnifiedDesktop();
      break;
    case DISABLE_CAPS_LOCK:
      HandleDisableCapsLock();
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
    case FOCUS_PIP:
      HandleFocusPip();
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
      HandleLaunchAppN(0);
      break;
    case LAUNCH_APP_1:
      HandleLaunchAppN(1);
      break;
    case LAUNCH_APP_2:
      HandleLaunchAppN(2);
      break;
    case LAUNCH_APP_3:
      HandleLaunchAppN(3);
      break;
    case LAUNCH_APP_4:
      HandleLaunchAppN(4);
      break;
    case LAUNCH_APP_5:
      HandleLaunchAppN(5);
      break;
    case LAUNCH_APP_6:
      HandleLaunchAppN(6);
      break;
    case LAUNCH_APP_7:
      HandleLaunchAppN(7);
      break;
    case LAUNCH_LAST_APP:
      HandleLaunchLastApp();
      break;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
      Shell::Get()->power_button_controller()->OnLockButtonEvent(
          action == LOCK_PRESSED, base::TimeTicks());
      break;
    case LOCK_SCREEN:
      HandleLock();
      break;
    case MAGNIFIER_ZOOM_IN:
      HandleActiveMagnifierZoom(1);
      break;
    case MAGNIFIER_ZOOM_OUT:
      HandleActiveMagnifierZoom(-1);
      break;
    case MEDIA_FAST_FORWARD:
      HandleMediaFastForward();
      break;
    case MEDIA_NEXT_TRACK:
      HandleMediaNextTrack();
      break;
    case MEDIA_PAUSE:
      HandleMediaPause();
      break;
    case MEDIA_PLAY:
      HandleMediaPlay();
      break;
    case MEDIA_PLAY_PAUSE:
      HandleMediaPlayPause();
      break;
    case MEDIA_PREV_TRACK:
      HandleMediaPrevTrack();
      break;
    case MEDIA_REWIND:
      HandleMediaRewind();
      break;
    case MEDIA_STOP:
      HandleMediaStop();
      break;
    case MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS:
      display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
      break;
    case NEW_INCOGNITO_WINDOW:
      HandleNewIncognitoWindow();
      break;
    case NEW_TAB:
      HandleNewTab(accelerator);
      break;
    case NEW_WINDOW:
      HandleNewWindow();
      break;
    case OPEN_CROSH:
      HandleCrosh();
      break;
    case OPEN_FEEDBACK_PAGE:
      HandleOpenFeedbackPage();
      break;
    case OPEN_FILE_MANAGER:
      HandleFileManager();
      break;
    case OPEN_GET_HELP:
      HandleGetHelp();
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
      HandleRestoreTab();
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
    case SHOW_IME_MENU_BUBBLE:
      HandleShowImeMenuBubble();
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
      HandleTakePartialScreenshot();
      break;
    case TAKE_SCREENSHOT:
      HandleTakeScreenshot();
      break;
    case TAKE_WINDOW_SCREENSHOT:
      HandleTakeWindowScreenshot();
      break;
    case TOGGLE_APP_LIST:
      HandleToggleAppList(accelerator, kSearchKey);
      break;
    case TOGGLE_APP_LIST_FULLSCREEN:
      HandleToggleAppList(accelerator, kSearchKeyFullscreen);
      break;
    case TOGGLE_CAPS_LOCK:
      HandleToggleCapsLock();
      break;
    case TOGGLE_DICTATION:
      HandleToggleDictation();
      break;
    case TOGGLE_DOCKED_MAGNIFIER:
      HandleToggleDockedMagnifier();
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
      actions_allowed_in_pinned_mode_.find(action) ==
          actions_allowed_in_pinned_mode_.end()) {
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      actions_allowed_at_login_screen_.find(action) ==
          actions_allowed_at_login_screen_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->session_controller()->IsScreenLocked() &&
      actions_allowed_at_lock_screen_.find(action) ==
          actions_allowed_at_lock_screen_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->power_button_controller()->IsMenuOpened() &&
      !base::Contains(actions_allowed_at_power_menu_, action)) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::Get()->session_controller()->IsRunningInAppMode() &&
      actions_allowed_in_app_mode_.find(action) ==
          actions_allowed_in_app_mode_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (Shell::IsSystemModalWindowOpen() &&
      actions_allowed_at_modal_window_.find(action) ==
          actions_allowed_at_modal_window_.end()) {
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
  if (!deprecated_accelerators_.count(accelerator)) {
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

  auto* dialog = new AcceleratorConfirmationDialog(
      window_title_text_id, dialog_text_id, std::move(on_accept_callback),
      std::move(on_cancel_callback));
  confirmation_dialog_ = dialog->GetWeakPtr();
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

  std::unique_ptr<base::DictionaryValue> info_in_dict =
      base::DictionaryValue::From(
          base::JSONReader::ReadDeprecated(location_info));
  if (!info_in_dict) {
    LOG(ERROR) << "JSONReader failed reading side volume button location info: "
               << location_info;
    return;
  }
  info_in_dict->GetString(kVolumeButtonRegion,
                          &side_volume_button_location_.region);
  info_in_dict->GetString(kVolumeButtonSide,
                          &side_volume_button_location_.side);
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
  if (!features::IsSwapSideVolumeButtonsForOrientationEnabled() ||
      !IsInternalKeyboardOrUncategorizedDevice(source_device_id)) {
    return false;
  }

  if (!IsValidSideVolumeButtonLocation())
    return false;

  OrientationLockType screen_orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  const std::string side = side_volume_button_location_.side;
  const bool is_landscape_secondary_or_portrait_primary =
      screen_orientation == OrientationLockType::kLandscapeSecondary ||
      screen_orientation == OrientationLockType::kPortraitPrimary;

  if (side_volume_button_location_.region == kVolumeButtonRegionKeyboard) {
    if (side == kVolumeButtonSideLeft || side == kVolumeButtonSideRight)
      return IsPrimaryOrientation(screen_orientation);
    return is_landscape_secondary_or_portrait_primary;
  }

  DCHECK_EQ(kVolumeButtonRegionScreen, side_volume_button_location_.region);
  if (side == kVolumeButtonSideLeft || side == kVolumeButtonSideRight)
    return !IsPrimaryOrientation(screen_orientation);
  return is_landscape_secondary_or_portrait_primary;
}

void AcceleratorControllerImpl::UpdateTabletModeVolumeAdjustHistogram() {
  const int volume_percent =
      chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  const bool swapped = features::IsSwapSideVolumeButtonsForOrientationEnabled();
  if ((volume_adjust_starts_with_up_ &&
       volume_percent >= initial_volume_percent_) ||
      (!volume_adjust_starts_with_up_ &&
       volume_percent <= initial_volume_percent_)) {
    RecordTabletVolumeAdjustTypeHistogram(
        swapped ? TabletModeVolumeAdjustType::kNormalAdjustWithSwapEnabled
                : TabletModeVolumeAdjustType::kNormalAdjustWithSwapDisabled);
  } else {
    RecordTabletVolumeAdjustTypeHistogram(
        swapped
            ? TabletModeVolumeAdjustType::kAccidentalAdjustWithSwapEnabled
            : TabletModeVolumeAdjustType::kAccidentalAdjustWithSwapDisabled);
  }
}

void AcceleratorControllerImpl::StartTabletModeVolumeAdjustTimer(
    AcceleratorAction action) {
  if (!tablet_mode_volume_adjust_timer_.IsRunning()) {
    volume_adjust_starts_with_up_ = action == VOLUME_UP;
    initial_volume_percent_ =
        chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  }
  tablet_mode_volume_adjust_timer_.Start(
      FROM_HERE, kVolumeAdjustTimeout, this,
      &AcceleratorControllerImpl::UpdateTabletModeVolumeAdjustHistogram);
}

}  // namespace ash
