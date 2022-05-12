// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/constants/ash_features.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/focus_cycler.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"

// Keep the functions in this file in alphabetical order.
namespace ash {
namespace accelerators {

namespace {

views::Widget* FindPipWidget() {
  return Shell::Get()->focus_cycler()->FindWidget(
      base::BindRepeating([](views::Widget* widget) {
        return WindowState::Get(widget->GetNativeWindow())->IsPip();
      }));
}

}  // namespace

void DumpCalendarModel() {
  Shell::Get()->system_tray_model()->calendar_model()->DebugDump();
}

void CycleBackwardMru() {
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kBackward);
}

void FocusPip() {
  auto* widget = FindPipWidget();
  if (widget)
    Shell::Get()->focus_cycler()->FocusWidget(widget);
}

void CycleForwardMru() {
  Shell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::WindowCyclingDirection::kForward);
}

void DisableCapsLock() {
  Shell::Get()->ime_controller()->SetCapsLockEnabled(false);
}

void LaunchAppN(int n) {
  Shelf::LaunchShelfItem(n);
}

void LaunchLastApp() {
  Shelf::LaunchShelfItem(-1);
}

void LockScreen() {
  Shell::Get()->session_controller()->LockScreen();
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

  audio_handler->SetInputMute(mute);
}

void NewIncognitoWindow() {
  NewWindowDelegate::GetPrimary()->NewWindow(
      /*is_incognito=*/true,
      /*should_trigger_session_restore=*/false);
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

void ResetDisplayZoom() {
  base::RecordAction(base::UserMetricsAction("Accel_Scale_Ui_Reset"));
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  display_manager->ResetDisplayZoom(display.id());
}

void RestoreTab() {
  NewWindowDelegate::GetPrimary()->RestoreTab();
}

void ShiftPrimaryDisplay() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();

  CHECK_GE(display_manager->GetNumDisplays(), 2U);

  const int64_t primary_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();

  const display::Displays& active_display_list =
      display_manager->active_display_list();

  auto primary_display_iter =
      std::find_if(active_display_list.begin(), active_display_list.end(),
                   [id = primary_display_id](const display::Display& display) {
                     return display.id() == id;
                   });

  DCHECK(primary_display_iter != active_display_list.end());

  ++primary_display_iter;

  // If we've reach the end of |active_display_list|, wrap back around to the
  // front.
  if (primary_display_iter == active_display_list.end())
    primary_display_iter = active_display_list.begin();

  Shell::Get()->display_configuration_controller()->SetPrimaryDisplayId(
      primary_display_iter->id(), true /* throttle */);
}

void ToggleCalendar() {
  aura::Window* target_root = Shell::GetRootWindowForNewWindows();
  UnifiedSystemTray* tray = RootWindowController::ForWindow(target_root)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  // If currently showing the calendar view, close it.
  if (tray->IsShowingCalendarView()) {
    tray->CloseBubble();
    return;
  }

  // If currently not showing the calendar view, show the bubble if needed then
  // show the calendar view.
  if (!tray->IsBubbleShown())
    tray->ShowBubble();
  tray->ActivateBubble();
  tray->bubble()->ShowCalendarView(
      calendar_metrics::CalendarViewShowSource::kAccelerator,
      calendar_metrics::CalendarEventSource::kKeyboard);
}

void ToggleFullscreen() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(active_window)->OnWMEvent(&event);
}

void ToggleKeyboardBacklight() {
  KeyboardBrightnessControlDelegate* delegate =
      Shell::Get()->keyboard_brightness_control_delegate();
  delegate->HandleToggleKeyboardBacklight();
}

void ToggleMaximized() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Maximized"));
  WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(active_window)->OnWMEvent(&event);
}

bool ToggleMinimized() {
  aura::Window* window = window_util::GetActiveWindow();
  // Attempt to restore the window that would be cycled through next from
  // the launcher when there is no active window.
  if (!window) {
    // Do not unminimize a window on an inactive desk, since this will cause
    // desks to switch and that will be unintentional for the user.
    MruWindowTracker::WindowList mru_windows(
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
    if (!mru_windows.empty())
      WindowState::Get(mru_windows.front())->Activate();
    return true;
  }
  WindowState* window_state = WindowState::Get(window);
  if (!window_state->CanMinimize())
    return false;
  window_state->Minimize();
  return true;
}

void ToggleResizeLockMenu() {
  aura::Window* active_window = window_util::GetActiveWindow();
  auto* frame_view = ash::NonClientFrameViewAsh::Get(active_window);
  frame_view->GetToggleResizeLockMenuCallback().Run();
}

void UnpinWindow() {
  aura::Window* pinned_window =
      Shell::Get()->screen_pinning_controller()->pinned_window();
  if (pinned_window)
    WindowState::Get(pinned_window)->Restore();
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

}  // namespace accelerators
}  // namespace ash
