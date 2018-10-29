// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_confirmation_dialog.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/debug.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_move_window_util.h"
#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/magnifier/docked_magnifier_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/media_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/new_window_controller.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/window_rotation.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/message_center/notification_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/user_manager/user_type.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/message_center/message_center.h"

namespace ash {

const char kNotifierAccelerator[] = "ash.accelerator-controller";

const char kHighContrastToggleAccelNotificationId[] =
    "chrome://settings/accessibility/highcontrast";
const char kDockedMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/dockedmagnifier";
const char kFullscreenMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/fullscreenmagnifier";

namespace {

using base::UserMetricsAction;
using message_center::Notification;
using message_center::SystemNotificationWarningLevel;

// Toast id and duration for voice interaction shortcuts
const char kVoiceInteractionErrorToastId[] = "voice_interaction_error";
const int kToastDurationMs = 2500;

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

  std::unique_ptr<Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE), message,
          base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kNotifierAccelerator),
          message_center::RichNotificationData(), std::move(delegate),
          kNotificationKeyboardIcon, SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void ShowToast(std::string id, const base::string16& text) {
  ToastData toast(id, text, kToastDurationMs, base::nullopt);
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
                            ImeSwitchType::kAccelerator, ImeSwitchType::kCount);
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
  // TODO(jamescook): Should this be GetRootWindowForNewWindows()?
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
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

void HandleMediaPlayPause() {
  base::RecordAction(UserMetricsAction("Accel_Media_PlayPause"));
  Shell::Get()->media_controller()->HandleMediaPlayPause();
}

void HandleMediaPrevTrack() {
  base::RecordAction(UserMetricsAction("Accel_Media_Prev_Track"));
  Shell::Get()->media_controller()->HandleMediaPrevTrack();
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
  Shell::Get()->new_window_controller()->NewWindow(true /* is_incognito */);
}

void HandleNewTab(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_T)
    base::RecordAction(UserMetricsAction("Accel_NewTab_T"));
  Shell::Get()->new_window_controller()->NewTab();
}

void HandleNewWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Window"));
  Shell::Get()->new_window_controller()->NewWindow(false /* is_incognito */);
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
  return !keyboard::KeyboardController::Get()->IsKeyboardVisible();
}

void HandleNextIme() {
  base::RecordAction(UserMetricsAction("Accel_Next_Ime"));
  RecordImeSwitchByAccelerator();
  Shell::Get()->ime_controller()->SwitchToNextIme();
}

void HandleOpenFeedbackPage() {
  base::RecordAction(UserMetricsAction("Accel_Open_Feedback_Page"));
  Shell::Get()->new_window_controller()->OpenFeedbackPage();
}

void HandlePreviousIme(const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Previous_Ime"));
  if (accelerator.key_state() == ui::Accelerator::KeyState::PRESSED) {
    RecordImeSwitchByAccelerator();
    Shell::Get()->ime_controller()->SwitchToPreviousIme();
  }
  // Else: consume the Ctrl+Space ET_KEY_RELEASED event but do not do anything.
}

display::Display::Rotation GetNextRotation(display::Display::Rotation current) {
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

void RotateScreen() {
  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  const display::ManagedDisplayInfo& display_info =
      Shell::Get()->display_manager()->GetDisplayInfo(display.id());
  Shell::Get()->display_configuration_controller()->SetDisplayRotation(
      display.id(), GetNextRotation(display_info.GetActiveRotation()),
      display::Display::RotationSource::USER);
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
        base::BindOnce([]() {
          RotateScreen();
          Shell::Get()
              ->accessibility_controller()
              ->SetDisplayRotationAcceleratorDialogBeenAccepted();
        }));
  } else {
    RotateScreen();
  }
}

void HandleRestoreTab() {
  base::RecordAction(UserMetricsAction("Accel_Restore_Tab"));
  Shell::Get()->new_window_controller()->RestoreTab();
}

// Rotate the active window.
void HandleRotateActiveWindow() {
  base::RecordAction(UserMetricsAction("Accel_Rotate_Active_Window"));
  aura::Window* active_window = wm::GetActiveWindow();
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
  Shell::Get()->new_window_controller()->ShowKeyboardShortcutViewer();
}

void HandleTakeWindowScreenshot() {
  base::RecordAction(UserMetricsAction("Accel_Take_Window_Screenshot"));
  Shell::Get()->screenshot_controller()->StartWindowScreenshotSession();
}

void HandleTakePartialScreenshot() {
  base::RecordAction(UserMetricsAction("Accel_Take_Partial_Screenshot"));
  Shell::Get()->screenshot_controller()->StartPartialScreenshotSession(
      true /* draw_overlay_immediately */);
}

void HandleTakeScreenshot() {
  base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
  Shell::Get()->screenshot_controller()->TakeScreenshotForAllRootWindows();
}

void HandleToggleSystemTrayBubbleInternal() {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  UnifiedSystemTray* tray = RootWindowController::ForWindow(target_root)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  if (tray->IsBubbleShown()) {
    tray->CloseBubble();
  } else {
    tray->ShowBubble(false /* show_by_click */);
    tray->ActivateBubble();
  }
}

void HandleToggleSystemTrayBubble() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_System_Tray_Bubble"));
  HandleToggleSystemTrayBubbleInternal();
}

void HandleToggleMessageCenterBubble() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Message_Center_Bubble"));
  HandleToggleSystemTrayBubbleInternal();
}

void HandleShowTaskManager() {
  base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
  Shell::Get()->new_window_controller()->ShowTaskManager();
}

void HandleSwapPrimaryDisplay() {
  base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));

  // TODO(rjkroege): This is not correct behaviour on devices with more than
  // two screens. Behave the same as mirroring: fail and notify if there are
  // three or more screens.
  Shell::Get()->display_configuration_controller()->SetPrimaryDisplayId(
      Shell::Get()->display_manager()->GetSecondaryDisplay().id(),
      true /* throttle */);
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
    if (Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled())
      return false;
  }
  return true;
}

void HandleToggleAppList(const ui::Accelerator& accelerator) {
  if (Shell::Get()
          ->app_list_controller()
          ->IsHomeLauncherEnabledInTabletMode()) {
    return;
  }
  if (accelerator.key_code() == ui::VKEY_LWIN)
    base::RecordAction(UserMetricsAction("Accel_Search_LWin"));

  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(Shell::GetRootWindowForNewWindows())
          .id(),
      app_list::kSearchKey, accelerator.time_stamp());
}

void HandleToggleFullscreen(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_MEDIA_LAUNCH_APP2)
    base::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
  accelerators::ToggleFullscreen();
}

void HandleToggleOverview() {
  base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
  Shell::Get()->window_selector_controller()->ToggleOverview();
}

void HandleToggleUnifiedDesktop() {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(
      !Shell::Get()->display_manager()->unified_desktop_enabled());
}

bool CanHandleWindowSnap() {
  aura::Window* active_window = wm::GetActiveWindow();
  if (!active_window)
    return false;
  wm::WindowState* window_state = wm::GetWindowState(active_window);
  // Disable window snapping shortcut key for full screen window due to
  // http://crbug.com/135487.
  return (window_state && window_state->IsUserPositionable() &&
          !window_state->IsFullscreen());
}

void HandleWindowSnap(AcceleratorAction action) {
  if (action == WINDOW_CYCLE_SNAP_LEFT)
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Left"));
  else
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Right"));

  const wm::WMEvent event(action == WINDOW_CYCLE_SNAP_LEFT
                              ? wm::WM_EVENT_CYCLE_SNAP_LEFT
                              : wm::WM_EVENT_CYCLE_SNAP_RIGHT);
  aura::Window* active_window = wm::GetActiveWindow();
  DCHECK(active_window);
  wm::GetWindowState(active_window)->OnWMEvent(&event);
}

void HandleWindowMinimize() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
  accelerators::ToggleMinimized();
}

bool CanHandlePositionCenter() {
  return wm::GetActiveWindow() != nullptr;
}

void HandlePositionCenter() {
  base::RecordAction(UserMetricsAction("Accel_Window_Position_Center"));
  wm::CenterWindow(wm::GetActiveWindow());
}

void HandleShowImeMenuBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Ime_Menu_Bubble"));

  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetStatusAreaWidget();
  if (status_area_widget) {
    ImeMenuTray* ime_menu_tray = status_area_widget->ime_menu_tray();
    if (ime_menu_tray && ime_menu_tray->visible() &&
        !ime_menu_tray->GetBubbleView()) {
      ime_menu_tray->ShowBubble(false /* show_by_click */);
    }
  }
}

void HandleCrosh() {
  base::RecordAction(UserMetricsAction("Accel_Open_Crosh"));

  Shell::Get()->new_window_controller()->OpenCrosh();
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

  Shell::Get()->new_window_controller()->OpenFileManager();
}

void HandleGetHelp() {
  Shell::Get()->new_window_controller()->OpenGetHelp();
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

bool CanHandleStartVoiceInteraction() {
  return chromeos::switches::IsVoiceInteractionFlagsEnabled() ||
         chromeos::switches::IsAssistantEnabled();
}

void HandleToggleVoiceInteraction(const ui::Accelerator& accelerator) {
  if (accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_SPACE) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_Space"));
  } else if (accelerator.IsCmdDown() && accelerator.key_code() == ui::VKEY_A) {
    // Search+A shortcut is disabled on device with an assistant key.
    if (ui::DeviceUsesKeyboardLayout2())
      return;

    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Search_A"));
  } else if (accelerator.key_code() == ui::VKEY_ASSISTANT) {
    base::RecordAction(
        base::UserMetricsAction("VoiceInteraction.Started.Assistant"));
  }

  switch (Shell::Get()->voice_interaction_controller()->allowed_state()) {
    case mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER:
      // Show a toast if the active user is not primary.
      ShowToast(kVoiceInteractionErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_VOICE_INTERACTION_SECONDARY_USER_TOAST_MESSAGE));
      return;
    case mojom::AssistantAllowedState::DISALLOWED_BY_LOCALE:
      // Show a toast if voice interaction is disabled due to unsupported
      // locales.
      ShowToast(
          kVoiceInteractionErrorToastId,
          l10n_util::GetStringUTF16(
              IDS_ASH_VOICE_INTERACTION_LOCALE_UNSUPPORTED_TOAST_MESSAGE));
      return;
    case mojom::AssistantAllowedState::DISALLOWED_BY_ARC_POLICY:
      // Show a toast if voice interaction is disabled due to enterprise policy.
      ShowToast(kVoiceInteractionErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_VOICE_INTERACTION_DISABLED_BY_POLICY_MESSAGE));
      return;
    case mojom::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE:
      // Show a toast if voice interaction is disabled due to being in Demo
      // Mode.
      ShowToast(kVoiceInteractionErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_VOICE_INTERACTION_DISABLED_IN_DEMO_MODE_MESSAGE));
      return;
    case mojom::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION:
      // Show a toast if voice interaction is disabled due to being in Demo
      // Mode.
      ShowToast(kVoiceInteractionErrorToastId,
                l10n_util::GetStringUTF16(
                    IDS_ASH_VOICE_INTERACTION_DISABLED_IN_DEMO_MODE_MESSAGE));
      return;
    case mojom::AssistantAllowedState::DISALLOWED_BY_ARC_DISALLOWED:
    case mojom::AssistantAllowedState::DISALLOWED_BY_FLAG:
    case mojom::AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER:
    case mojom::AssistantAllowedState::DISALLOWED_BY_CHILD_USER:
    case mojom::AssistantAllowedState::DISALLOWED_BY_INCOGNITO:
      // TODO(xiaohuic): show a specific toast.
      return;
    case mojom::AssistantAllowedState::ALLOWED:
      // Nothing need to do if allowed.
      break;
  }

  if (!chromeos::switches::IsAssistantEnabled()) {
    Shell::Get()->app_list_controller()->ToggleVoiceInteractionSession();
  } else {
    Shell::Get()->assistant_controller()->ui_controller()->ToggleUi(
        AssistantSource::kHotkey);
  }
}

void HandleSuspend() {
  base::RecordAction(UserMetricsAction("Accel_Suspend"));
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestSuspend();
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
  ImeController* ime_controller = Shell::Get()->ime_controller();
  ime_controller->SetCapsLockEnabled(!ime_controller->IsCapsLockEnabled());
}

bool CanHandleToggleDictation() {
  return Shell::Get()->accessibility_controller()->IsDictationEnabled();
}

void HandleToggleDictation() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Dictation"));
  Shell::Get()->accessibility_controller()->ToggleDictationFromSource(
      mojom::DictationToggleSource::kKeyboard);
}

bool CanHandleToggleDockedMagnifier() {
  return features::IsDockedMagnifierEnabled();
}

void CreateAndShowStickyNotification(const int title_id,
                                     const int message_id,
                                     const std::string& notification_id) {
  std::unique_ptr<Notification> notification =
      Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id),
          base::string16() /* display source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kNotifierAccelerator),
          message_center::RichNotificationData(), nullptr,
          kNotificationAccessibilityIcon,
          SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void RemoveStickyNotitification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           false /* by_user */);
}

void SetDockedMagnifierEnabled(bool enabled) {
  if (enabled) {
    CreateAndShowStickyNotification(IDS_DOCKED_MAGNIFIER_ACCEL_TITLE,
                                    IDS_DOCKED_MAGNIFIER_ACCEL_MSG,
                                    kDockedMagnifierToggleAccelNotificationId);
  } else {
    RemoveStickyNotitification(kDockedMagnifierToggleAccelNotificationId);
  }
  Shell::Get()->docked_magnifier_controller()->SetEnabled(enabled);
}

void HandleToggleDockedMagnifier() {
  DCHECK(features::IsDockedMagnifierEnabled());
  base::RecordAction(UserMetricsAction("Accel_Toggle_Docked_Magnifier"));

  DockedMagnifierController* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  const bool current_enabled = docked_magnifier_controller->GetEnabled();
  const bool dialog_ever_accepted =
      Shell::Get()
          ->accessibility_controller()
          ->HasDockedMagnifierAcceleratorDialogBeenAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    Shell::Get()->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_DOCKED_MAGNIFIER_TITLE, IDS_ASH_DOCKED_MAGNIFIER_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->SetDockedMagnifierAcceleratorDialogAccepted();
          SetDockedMagnifierEnabled(true);
        }));
  } else {
    SetDockedMagnifierEnabled(!current_enabled);
  }
}

void SetFullscreenMagnifierEnabled(bool enabled) {
  if (enabled) {
    CreateAndShowStickyNotification(
        IDS_FULLSCREEN_MAGNIFIER_ACCEL_TITLE,
        IDS_FULLSCREEN_MAGNIFIER_ACCEL_MSG,
        kFullscreenMagnifierToggleAccelNotificationId);
  } else {
    RemoveStickyNotitification(kFullscreenMagnifierToggleAccelNotificationId);
  }

  // TODO (afakhry): Move the below into a single call (crbug/817157).
  // Necessary to make magnification controller in ash observe changes to the
  // prefs iteself.
  Shell* shell = Shell::Get();
  shell->accessibility_controller()->SetFullscreenMagnifierEnabled(enabled);
  shell->magnification_controller()->SetEnabled(enabled);
}

void SetHighContrastEnabled(bool enabled) {
  if (enabled) {
    CreateAndShowStickyNotification(IDS_HIGH_CONTRAST_ACCEL_TITLE,
                                    IDS_HIGH_CONTRAST_ACCEL_MSG,
                                    kHighContrastToggleAccelNotificationId);
  } else {
    RemoveStickyNotitification(kHighContrastToggleAccelNotificationId);
  }
  Shell::Get()->accessibility_controller()->SetHighContrastEnabled(enabled);
}

void HandleToggleHighContrast() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_High_Contrast"));

  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  const bool current_enabled = controller->IsHighContrastEnabled();
  const bool dialog_ever_accepted =
      controller->HasHighContrastAcceleratorDialogBeenAccepted();

  if (!current_enabled && !dialog_ever_accepted) {
    Shell::Get()->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_HIGH_CONTRAST_TITLE, IDS_ASH_HIGH_CONTRAST_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->SetHighContrastAcceleratorDialogAccepted();
          SetHighContrastEnabled(true);
        }));
  } else {
    SetHighContrastEnabled(!current_enabled);
  }
}

void HandleToggleFullscreenMagnifier() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Fullscreen_Magnifier"));

  MagnificationController* controller =
      Shell::Get()->magnification_controller();
  const bool current_enabled = controller->IsEnabled();
  const bool dialog_ever_accepted =
      Shell::Get()
          ->accessibility_controller()
          ->HasScreenMagnifierAcceleratorDialogBeenAccepted();
  if (!current_enabled && !dialog_ever_accepted) {
    Shell::Get()->accelerator_controller()->MaybeShowConfirmationDialog(
        IDS_ASH_SCREEN_MAGNIFIER_TITLE, IDS_ASH_SCREEN_MAGNIFIER_BODY,
        base::BindOnce([]() {
          Shell::Get()
              ->accessibility_controller()
              ->SetScreenMagnifierAcceleratorDialogAccepted();
          SetFullscreenMagnifierEnabled(true);
        }));
  } else {
    SetFullscreenMagnifierEnabled(!current_enabled);
  }
}

void HandleToggleSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));

  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(!controller->IsSpokenFeedbackEnabled(),
                                       A11Y_NOTIFICATION_SHOW);
}

void HandleVolumeDown(mojom::VolumeController* volume_controller,
                      const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_DOWN)
    base::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));

  if (volume_controller)
    volume_controller->VolumeDown();
}

void HandleVolumeMute(mojom::VolumeController* volume_controller,
                      const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
    base::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));

  if (volume_controller)
    volume_controller->VolumeMuteToggle();
}

void HandleVolumeUp(mojom::VolumeController* volume_controller,
                    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_UP)
    base::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));

  if (volume_controller)
    volume_controller->VolumeUp();
}

bool CanHandleActiveMagnifierZoom() {
  return Shell::Get()->magnification_controller()->IsEnabled() ||
         (features::IsDockedMagnifierEnabled() &&
          Shell::Get()->docked_magnifier_controller()->GetEnabled());
}

// Change the scale of the active magnifier.
void HandleActiveMagnifierZoom(int delta_index) {
  if (Shell::Get()->magnification_controller()->IsEnabled()) {
    Shell::Get()->magnification_controller()->StepToNextScaleValue(delta_index);
    return;
  }

  if (features::IsDockedMagnifierEnabled() &&
      Shell::Get()->docked_magnifier_controller()->GetEnabled()) {
    Shell::Get()->docked_magnifier_controller()->StepToNextScaleValue(
        delta_index);
  }
}

bool CanHandleTouchHud() {
  return RootWindowController::ForTargetRootWindow()->touch_observer_hud();
}

void HandleTouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_observer_hud()->Clear();
}

void HandleTouchHudModeChange() {
  RootWindowController::ForTargetRootWindow()
      ->touch_observer_hud()
      ->ChangeToNextMode();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, public:

AcceleratorController::AcceleratorController()
    : accelerator_manager_(std::make_unique<ui::AcceleratorManager>()),
      accelerator_history_(std::make_unique<ui::AcceleratorHistory>()) {
  Init();
}

AcceleratorController::~AcceleratorController() = default;

void AcceleratorController::Register(
    const std::vector<ui::Accelerator>& accelerators,
    ui::AcceleratorTarget* target) {
  accelerator_manager_->Register(
      accelerators, ui::AcceleratorManager::kNormalPriority, target);
}

void AcceleratorController::Unregister(const ui::Accelerator& accelerator,
                                       ui::AcceleratorTarget* target) {
  accelerator_manager_->Unregister(accelerator, target);
}

void AcceleratorController::UnregisterAll(ui::AcceleratorTarget* target) {
  accelerator_manager_->UnregisterAll(target);
}

bool AcceleratorController::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator it =
      accelerators_.find(accelerator);
  return it != accelerators_.end() && CanPerformAction(it->second, accelerator);
}

bool AcceleratorController::Process(const ui::Accelerator& accelerator) {
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorController::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->IsRegistered(accelerator);
}

bool AcceleratorController::IsPreferred(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return preferred_actions_.find(iter->second) != preferred_actions_.end();
}

bool AcceleratorController::IsReserved(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return reserved_actions_.find(iter->second) != reserved_actions_.end();
}

bool AcceleratorController::IsDeprecated(
    const ui::Accelerator& accelerator) const {
  return deprecated_accelerators_.count(accelerator) != 0;
}

bool AcceleratorController::PerformActionIfEnabled(AcceleratorAction action) {
  if (CanPerformAction(action, ui::Accelerator())) {
    PerformAction(action, ui::Accelerator());
    return true;
  }
  return false;
}

AcceleratorController::AcceleratorProcessingRestriction
AcceleratorController::GetCurrentAcceleratorRestriction() {
  return GetAcceleratorProcessingRestriction(-1);
}

bool AcceleratorController::ShouldCloseMenuAndRepostAccelerator(
    const ui::Accelerator& accelerator) const {
  auto itr = accelerators_.find(accelerator);
  if (itr == accelerators_.end())
    return false;  // Menu shouldn't be closed for an invalid accelerator.

  AcceleratorAction action = itr->second;
  return actions_keeping_menu_open_.count(action) == 0;
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, ui::AcceleratorTarget implementation:

bool AcceleratorController::AcceleratorPressed(
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

bool AcceleratorController::CanHandleAccelerators() const {
  return true;
}

void AcceleratorController::BindRequest(
    mojom::AcceleratorControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AcceleratorController::SetVolumeController(
    mojom::VolumeControllerPtr controller) {
  volume_controller_ = std::move(controller);
}

///////////////////////////////////////////////////////////////////////////////
// AcceleratorController, private:

void AcceleratorController::Init() {
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
  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i)
    actions_needing_window_.insert(kActionsNeedingWindow[i]);
  for (size_t i = 0; i < kActionsKeepingMenuOpenLength; ++i)
    actions_keeping_menu_open_.insert(kActionsKeepingMenuOpen[i]);

  RegisterAccelerators(kAcceleratorData, kAcceleratorDataLength);

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

void AcceleratorController::RegisterAccelerators(
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

void AcceleratorController::RegisterDeprecatedAccelerators() {
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

bool AcceleratorController::CanPerformAction(
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
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_QUICK_LAUNCH:
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
    case NEXT_IME:
      return CanCycleInputMethod();
    case PREVIOUS_IME:
      return CanCycleInputMethod();
    case ROTATE_SCREEN:
      return true;
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
      return true;
    case SHOW_STYLUS_TOOLS:
      return CanHandleShowStylusTools();
    case START_VOICE_INTERACTION:
      return CanHandleStartVoiceInteraction();
    case SWAP_PRIMARY_DISPLAY:
      return display::Screen::GetScreen()->GetNumDisplays() > 1;
    case SWITCH_IME:
      return CanHandleSwitchIme(accelerator);
    case SWITCH_TO_PREVIOUS_USER:
    case SWITCH_TO_NEXT_USER:
      return CanHandleCycleUser();
    case TOGGLE_APP_LIST:
      return CanHandleToggleAppList(accelerator, previous_accelerator);
    case TOGGLE_CAPS_LOCK:
      return CanHandleToggleCapsLock(
          accelerator, previous_accelerator,
          accelerator_history_->currently_pressed_keys());
    case TOGGLE_DICTATION:
      return CanHandleToggleDictation();
    case TOGGLE_DOCKED_MAGNIFIER:
      return CanHandleToggleDockedMagnifier();
    case TOGGLE_FULLSCREEN_MAGNIFIER:
      return true;
    case TOGGLE_MESSAGE_CENTER_BUBBLE:
      return true;
    case TOGGLE_MIRROR_MODE:
      return true;
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      return CanHandleTouchHud();
    case UNPIN:
      return accelerators::CanUnpinWindow();
    case WINDOW_CYCLE_SNAP_LEFT:
    case WINDOW_CYCLE_SNAP_RIGHT:
      return CanHandleWindowSnap();
    case WINDOW_POSITION_CENTER:
      return CanHandlePositionCenter();

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
    case MEDIA_NEXT_TRACK:
    case MEDIA_PLAY_PAUSE:
    case MEDIA_PREV_TRACK:
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
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
    case TOGGLE_FULLSCREEN:
    case TOGGLE_HIGH_CONTRAST:
    case TOGGLE_MAXIMIZED:
    case TOGGLE_OVERVIEW:
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

void AcceleratorController::PerformAction(AcceleratorAction action,
                                          const ui::Accelerator& accelerator) {
  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return;

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
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_QUICK_LAUNCH:
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
    case MEDIA_NEXT_TRACK:
      HandleMediaNextTrack();
      break;
    case MEDIA_PLAY_PAUSE:
      HandleMediaPlayPause();
      break;
    case MEDIA_PREV_TRACK:
      HandleMediaPrevTrack();
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
    case NEXT_IME:
      HandleNextIme();
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
    case PREVIOUS_IME:
      HandlePreviousIme(accelerator);
      break;
    case PRINT_UI_HIERARCHIES:
      debug::PrintUIHierarchies();
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
    case START_VOICE_INTERACTION:
      HandleToggleVoiceInteraction(accelerator);
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
      HandleToggleAppList(accelerator);
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
      HandleVolumeDown(volume_controller_.get(), accelerator);
      break;
    case VOLUME_MUTE:
      HandleVolumeMute(volume_controller_.get(), accelerator);
      break;
    case VOLUME_UP:
      HandleVolumeUp(volume_controller_.get(), accelerator);
      break;
    case WINDOW_CYCLE_SNAP_LEFT:
    case WINDOW_CYCLE_SNAP_RIGHT:
      HandleWindowSnap(action);
      break;
    case WINDOW_MINIMIZE:
      HandleWindowMinimize();
      break;
    case WINDOW_POSITION_CENTER:
      HandlePositionCenter();
      break;
  }
}

bool AcceleratorController::ShouldActionConsumeKeyEvent(
    AcceleratorAction action) {
  // Adding new exceptions is *STRONGLY* discouraged.
  return true;
}

AcceleratorController::AcceleratorProcessingRestriction
AcceleratorController::GetAcceleratorProcessingRestriction(int action) const {
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
      !base::ContainsKey(actions_allowed_at_power_menu_, action)) {
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
  if (Shell::Get()->mru_window_tracker()->BuildMruWindowList().empty() &&
      actions_needing_window_.find(action) != actions_needing_window_.end()) {
    Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
        mojom::AccessibilityAlert::WINDOW_NEEDED);
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  return RESTRICTION_NONE;
}

AcceleratorController::AcceleratorProcessingStatus
AcceleratorController::MaybeDeprecatedAcceleratorPressed(
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

void AcceleratorController::MaybeShowConfirmationDialog(
    int window_title_text_id,
    int dialog_text_id,
    base::OnceClosure on_accept_callback) {
  // An active dialog exists already.
  if (confirmation_dialog_)
    return;

  auto* dialog = new AcceleratorConfirmationDialog(
      window_title_text_id, dialog_text_id, std::move(on_accept_callback));
  confirmation_dialog_ = dialog->GetWeakPtr();
}

AcceleratorConfirmationDialog*
AcceleratorController::confirmation_dialog_for_testing() {
  return confirmation_dialog_.get();
}

}  // namespace ash
