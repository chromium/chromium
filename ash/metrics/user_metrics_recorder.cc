// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/user_metrics_recorder.h"

#include <memory>

#include "ash/login/ui/lock_screen.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/metrics/desktop_task_switch_metric_recorder.h"
#include "ash/metrics/pointer_metrics_recorder.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/interfaces/accessibility_controller.mojom-shared.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

// Time in seconds between calls to "RecordPeriodicMetrics".
const int kAshPeriodicMetricsTimeInSeconds = 30 * 60;

enum ActiveWindowStateType {
  ACTIVE_WINDOW_STATE_TYPE_NO_ACTIVE_WINDOW,
  ACTIVE_WINDOW_STATE_TYPE_OTHER,
  ACTIVE_WINDOW_STATE_TYPE_MAXIMIZED,
  ACTIVE_WINDOW_STATE_TYPE_FULLSCREEN,
  ACTIVE_WINDOW_STATE_TYPE_SNAPPED,
  ACTIVE_WINDOW_STATE_TYPE_PINNED,
  ACTIVE_WINDOW_STATE_TYPE_TRUSTED_PINNED,
  ACTIVE_WINDOW_STATE_TYPE_COUNT,
};

ActiveWindowStateType GetActiveWindowState() {
  ActiveWindowStateType active_window_state_type =
      ACTIVE_WINDOW_STATE_TYPE_NO_ACTIVE_WINDOW;
  wm::WindowState* active_window_state = ash::wm::GetActiveWindowState();
  if (active_window_state) {
    switch (active_window_state->GetStateType()) {
      case mojom::WindowStateType::MAXIMIZED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_MAXIMIZED;
        break;
      case mojom::WindowStateType::FULLSCREEN:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_FULLSCREEN;
        break;
      case mojom::WindowStateType::LEFT_SNAPPED:
      case mojom::WindowStateType::RIGHT_SNAPPED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_SNAPPED;
        break;
      case mojom::WindowStateType::PINNED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_PINNED;
        break;
      case mojom::WindowStateType::TRUSTED_PINNED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_TRUSTED_PINNED;
        break;
      case mojom::WindowStateType::DEFAULT:
      case mojom::WindowStateType::NORMAL:
      case mojom::WindowStateType::MINIMIZED:
      case mojom::WindowStateType::INACTIVE:
      case mojom::WindowStateType::AUTO_POSITIONED:
      case mojom::WindowStateType::PIP:
        // TODO: We probably want to record PIP state.
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_OTHER;
        break;
    }
  }
  return active_window_state_type;
}

// Returns true if kiosk mode is active.
bool IsKioskModeActive() {
  return Shell::Get()->session_controller()->login_status() ==
         LoginStatus::KIOSK_APP;
}

// Returns true if ARC kiosk mode is active.
bool IsArcKioskModeActive() {
  return Shell::Get()->session_controller()->login_status() ==
         LoginStatus::ARC_KIOSK_APP;
}

// Returns true if there is an active user and their session isn't currently
// locked.
bool IsUserActive() {
  SessionController* session = Shell::Get()->session_controller();
  return session->IsActiveUserSessionStarted() && !session->IsScreenLocked();
}

// Array of window container ids that contain visible windows to be counted for
// UMA statistics. Note the containers are ordered from top most visible
// container to the lowest to allow the |GetNumVisibleWindows| method to short
// circuit when processing a maximized or fullscreen window.
int kVisibleWindowContainerIds[] = {kShellWindowId_AlwaysOnTopContainer,
                                    kShellWindowId_DefaultContainer};

// Returns an approximate count of how many windows are currently visible in the
// primary root window.
int GetNumVisibleWindowsInPrimaryDisplay() {
  int visible_window_count = 0;
  bool maximized_or_fullscreen_window_present = false;

  for (const int& current_container_id : kVisibleWindowContainerIds) {
    if (maximized_or_fullscreen_window_present)
      break;

    const aura::Window::Windows& children =
        Shell::GetContainer(Shell::Get()->GetPrimaryRootWindow(),
                            current_container_id)
            ->children();
    // Reverse iterate over the child windows so that they are processed in
    // visible stacking order.
    for (aura::Window::Windows::const_reverse_iterator it = children.rbegin(),
                                                       rend = children.rend();
         it != rend; ++it) {
      const aura::Window* child_window = *it;
      const wm::WindowState* child_window_state =
          wm::GetWindowState(child_window);

      if (!child_window->IsVisible() || child_window_state->IsMinimized())
        continue;

      // Only count activatable windows for 1 reason:
      //  - Ensures that a browser window and its transient, modal child will
      //    only count as 1 visible window.
      if (child_window_state->CanActivate())
        ++visible_window_count;

      // Stop counting windows that will be hidden by maximized or fullscreen
      // windows. Only windows in the kShellWindowId_DefaultContainer and
      // kShellWindowId_AlwaysOnTopContainer can be maximized or fullscreened
      // and completely obscure windows beneath them.
      if (child_window_state->IsMaximizedOrFullscreenOrPinned()) {
        maximized_or_fullscreen_window_present = true;
        break;
      }
    }
  }
  return visible_window_count;
}

// Records the number of items in the shelf as an UMA statistic.
void RecordShelfItemCounts() {
  int pinned_item_count = 0;
  int unpinned_item_count = 0;
  for (const ShelfItem& item : Shell::Get()->shelf_model()->items()) {
    if (item.type == TYPE_PINNED_APP || item.type == TYPE_BROWSER_SHORTCUT)
      ++pinned_item_count;
    else if (item.type != TYPE_APP_LIST && item.type != TYPE_BACK_BUTTON)
      ++unpinned_item_count;
  }

  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfItems",
                           pinned_item_count + unpinned_item_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfPinnedItems", pinned_item_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfUnpinnedItems",
                           unpinned_item_count);
}

}  // namespace

UserMetricsRecorder::UserMetricsRecorder() {
  StartTimer();
  login_metrics_recorder_ = std::make_unique<LoginMetricsRecorder>();
}

UserMetricsRecorder::UserMetricsRecorder(bool record_periodic_metrics) {
  if (record_periodic_metrics)
    StartTimer();
}

UserMetricsRecorder::~UserMetricsRecorder() {
  timer_.Stop();
}

// static
void UserMetricsRecorder::RecordUserClickOnTray(
    LoginMetricsRecorder::TrayClickTarget target) {
  LoginMetricsRecorder* recorder =
      Shell::Get()->metrics()->login_metrics_recorder();
  recorder->RecordUserTrayClick(target);
}

// static
void UserMetricsRecorder::RecordUserClickOnShelfButton(
    LoginMetricsRecorder::ShelfButtonClickTarget target) {
  LoginMetricsRecorder* recorder =
      Shell::Get()->metrics()->login_metrics_recorder();
  recorder->RecordUserShelfButtonClick(target);
}

// static
void UserMetricsRecorder::RecordUserToggleDictation(
    mojom::DictationToggleSource source) {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosDictation.ToggleDictationMethod",
                            source);
}

void UserMetricsRecorder::RecordUserMetricsAction(UserMetricsAction action) {
  using base::RecordAction;
  using base::UserMetricsAction;

  switch (action) {
    case UMA_DESKTOP_SWITCH_TASK:
      RecordAction(UserMetricsAction("Desktop_SwitchTask"));
      task_switch_metrics_recorder_.OnTaskSwitch(TaskSwitchSource::DESKTOP);
      break;
    case UMA_LAUNCHER_BUTTON_PRESSED_WITH_MOUSE:
      RecordAction(UserMetricsAction("Launcher_ButtonPressed_Mouse"));
      break;
    case UMA_LAUNCHER_BUTTON_PRESSED_WITH_TOUCH:
      RecordAction(UserMetricsAction("Launcher_ButtonPressed_Touch"));
      break;
    case UMA_LAUNCHER_CLICK_ON_APP:
      RecordAction(UserMetricsAction("Launcher_ClickOnApp"));
      break;
    case UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON:
      RecordAction(UserMetricsAction("Launcher_ClickOnApplistButton"));
      break;
    case UMA_LAUNCHER_LAUNCH_TASK:
      RecordAction(UserMetricsAction("Launcher_LaunchTask"));
      task_switch_metrics_recorder_.OnTaskSwitch(TaskSwitchSource::SHELF);
      break;
    case UMA_LAUNCHER_MINIMIZE_TASK:
      RecordAction(UserMetricsAction("Launcher_MinimizeTask"));
      break;
    case UMA_LAUNCHER_SWITCH_TASK:
      RecordAction(UserMetricsAction("Launcher_SwitchTask"));
      task_switch_metrics_recorder_.OnTaskSwitch(TaskSwitchSource::SHELF);
      break;
    case UMA_SHELF_ALIGNMENT_SET_BOTTOM:
      RecordAction(UserMetricsAction("Shelf_AlignmentSetBottom"));
      break;
    case UMA_SHELF_ALIGNMENT_SET_LEFT:
      RecordAction(UserMetricsAction("Shelf_AlignmentSetLeft"));
      break;
    case UMA_SHELF_ALIGNMENT_SET_RIGHT:
      RecordAction(UserMetricsAction("Shelf_AlignmentSetRight"));
      break;
    case UMA_STATUS_AREA_AUDIO_CURRENT_INPUT_DEVICE:
      RecordAction(UserMetricsAction("StatusArea_Audio_CurrentInputDevice"));
      break;
    case UMA_STATUS_AREA_AUDIO_CURRENT_OUTPUT_DEVICE:
      RecordAction(UserMetricsAction("StatusArea_Audio_CurrentOutputDevice"));
      break;
    case UMA_STATUS_AREA_AUDIO_SWITCH_INPUT_DEVICE:
      RecordAction(UserMetricsAction("StatusArea_Audio_SwitchInputDevice"));
      break;
    case UMA_STATUS_AREA_AUDIO_SWITCH_OUTPUT_DEVICE:
      RecordAction(UserMetricsAction("StatusArea_Audio_SwitchOutputDevice"));
      break;
    case UMA_STATUS_AREA_BRIGHTNESS_CHANGED:
      RecordAction(UserMetricsAction("StatusArea_BrightnessChanged"));
      break;
    case UMA_STATUS_AREA_BLUETOOTH_DISABLED:
      RecordAction(UserMetricsAction("StatusArea_Bluetooth_Disabled"));
      break;
    case UMA_STATUS_AREA_BLUETOOTH_ENABLED:
      RecordAction(UserMetricsAction("StatusArea_Bluetooth_Enabled"));
      break;
    case UMA_STATUS_AREA_CAPS_LOCK_DETAILED:
      RecordAction(UserMetricsAction("StatusArea_CapsLock_Detailed"));
      break;
    case UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK:
      RecordAction(UserMetricsAction("StatusArea_CapsLock_DisabledByClick"));
      break;
    case UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK:
      RecordAction(UserMetricsAction("StatusArea_CapsLock_EnabledByClick"));
      break;
    case UMA_STATUS_AREA_CAPS_LOCK_POPUP:
      RecordAction(UserMetricsAction("StatusArea_CapsLock_Popup"));
      break;
    case UMA_STATUS_AREA_CAST_STOP_CAST:
      RecordAction(UserMetricsAction("StatusArea_Cast_StopCast"));
      break;
    case UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK:
      RecordAction(UserMetricsAction("StatusArea_Network_ConnectConfigured"));
      break;
    case UMA_STATUS_AREA_CONNECT_TO_UNCONFIGURED_NETWORK:
      RecordAction(UserMetricsAction("StatusArea_Network_ConnectUnconfigured"));
      break;
    case UMA_STATUS_AREA_CONNECT_TO_VPN:
      RecordAction(UserMetricsAction("StatusArea_VPN_ConnectToNetwork"));
      break;
    case UMA_STATUS_AREA_CHANGED_VOLUME_MENU:
      RecordAction(UserMetricsAction("StatusArea_Volume_ChangedMenu"));
      break;
    case UMA_STATUS_AREA_CHANGED_VOLUME_POPUP:
      RecordAction(UserMetricsAction("StatusArea_Volume_ChangedPopup"));
      break;
    case UMA_STATUS_AREA_DETAILED_ACCESSIBILITY:
      RecordAction(UserMetricsAction("StatusArea_Accessability_DetailedView"));
      break;
    case UMA_STATUS_AREA_DETAILED_AUDIO_VIEW:
      RecordAction(UserMetricsAction("StatusArea_Audio_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW:
      RecordAction(UserMetricsAction("StatusArea_Bluetooth_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_BRIGHTNESS_VIEW:
      RecordAction(UserMetricsAction("StatusArea_Brightness_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_CAST_VIEW:
      RecordAction(UserMetricsAction("StatusArea_Cast_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST:
      RecordAction(UserMetricsAction("StatusArea_Cast_Detailed_Launch_Cast"));
      break;
    case UMA_STATUS_AREA_DETAILED_DRIVE_VIEW:
      RecordAction(UserMetricsAction("StatusArea_Drive_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_NETWORK_VIEW:
      RecordAction(UserMetricsAction("StatusArea_Network_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_SMS_VIEW:
      RecordAction(UserMetricsAction("StatusArea_SMS_Detailed"));
      break;
    case UMA_STATUS_AREA_DETAILED_VPN_VIEW:
      RecordAction(UserMetricsAction("StatusArea_VPN_Detailed"));
      break;
    case UMA_STATUS_AREA_DISPLAY_DEFAULT_SELECTED:
      RecordAction(UserMetricsAction("StatusArea_Display_Default_Selected"));
      break;
    case UMA_STATUS_AREA_DISPLAY_DEFAULT_SHOW_SETTINGS:
      RecordAction(
          UserMetricsAction("StatusArea_Display_Default_ShowSettings"));
      break;
    case UMA_STATUS_AREA_DISPLAY_NOTIFICATION_CREATED:
      RecordAction(
          UserMetricsAction("StatusArea_Display_Notification_Created"));
      break;
    case UMA_STATUS_AREA_DISPLAY_NOTIFICATION_SELECTED:
      RecordAction(
          UserMetricsAction("StatusArea_Display_Notification_Selected"));
      break;
    case UMA_STATUS_AREA_DISPLAY_NOTIFICATION_SHOW_SETTINGS:
      RecordAction(
          UserMetricsAction("StatusArea_Display_Notification_Show_Settings"));
      break;
    case UMA_STATUS_AREA_DISABLE_WIFI:
      RecordAction(UserMetricsAction("StatusArea_Network_WifiDisabled"));
      break;
    case UMA_STATUS_AREA_DRIVE_CANCEL_OPERATION:
      RecordAction(UserMetricsAction("StatusArea_Drive_CancelOperation"));
      break;
    case UMA_STATUS_AREA_DRIVE_SETTINGS:
      RecordAction(UserMetricsAction("StatusArea_Drive_Settings"));
      break;
    case UMA_STATUS_AREA_ENABLE_WIFI:
      RecordAction(UserMetricsAction("StatusArea_Network_WifiEnabled"));
      break;
    case UMA_STATUS_AREA_MENU_OPENED:
      RecordAction(UserMetricsAction("StatusArea_MenuOpened"));
      break;
    case UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED:
      RecordAction(UserMetricsAction("StatusArea_Network_JoinOther"));
      break;
    case UMA_STATUS_AREA_NETWORK_SETTINGS_OPENED:
      RecordAction(UserMetricsAction("StatusArea_Network_Settings"));
      break;
    case UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED:
      RecordAction(UserMetricsAction("StatusArea_OS_Update_Default_Selected"));
      break;
    case UMA_STATUS_AREA_SCREEN_CAPTURE_DEFAULT_STOP:
      RecordAction(UserMetricsAction("StatusArea_ScreenCapture_Default_Stop"));
      break;
    case UMA_STATUS_AREA_SCREEN_CAPTURE_NOTIFICATION_STOP:
      RecordAction(
          UserMetricsAction("StatusArea_ScreenCapture_Notification_Stop"));
      break;
    case UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS:
      RecordAction(UserMetricsAction("StatusArea_Network_ConnectionDetails"));
      break;
    case UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS:
      RecordAction(UserMetricsAction("StatusArea_VPN_ConnectionDetails"));
      break;
    case UMA_STATUS_AREA_SIGN_OUT:
      RecordAction(UserMetricsAction("StatusArea_SignOut"));
      break;
    case UMA_STATUS_AREA_SMS_DETAILED_DISMISS_MSG:
      RecordAction(UserMetricsAction("StatusArea_SMS_Detailed_DismissMsg"));
      break;
    case UMA_STATUS_AREA_SMS_NOTIFICATION_DISMISS_MSG:
      RecordAction(UserMetricsAction("StatusArea_SMS_Notification_DismissMsg"));
      break;
    case UMA_STATUS_AREA_TRACING_DEFAULT_SELECTED:
      RecordAction(UserMetricsAction("StatusArea_Tracing_Default_Selected"));
      break;
    case UMA_STATUS_AREA_VPN_ADD_BUILT_IN_CLICKED:
      RecordAction(UserMetricsAction("StatusArea_VPN_AddBuiltIn"));
      break;
    case UMA_STATUS_AREA_VPN_ADD_THIRD_PARTY_CLICKED:
      RecordAction(UserMetricsAction("StatusArea_VPN_AddThirdParty"));
      break;
    case UMA_STATUS_AREA_VPN_DISCONNECT_CLICKED:
      RecordAction(UserMetricsAction("StatusArea_VPN_Disconnect"));
      break;
    case UMA_STATUS_AREA_VPN_SETTINGS_OPENED:
      RecordAction(UserMetricsAction("StatusArea_VPN_Settings"));
      break;
    case UMA_TRAY_HELP:
      RecordAction(UserMetricsAction("Tray_Help"));
      break;
    case UMA_TRAY_LOCK_SCREEN:
      RecordAction(UserMetricsAction("Tray_LockScreen"));
      break;
    case UMA_TRAY_NIGHT_LIGHT:
      RecordAction(UserMetricsAction("Tray_NightLight"));
      break;
    case UMA_TRAY_OVERVIEW:
      RecordAction(UserMetricsAction("Tray_Overview"));
      break;
    case UMA_TRAY_SETTINGS:
      RecordAction(UserMetricsAction("Tray_Settings"));
      break;
    case UMA_TRAY_SHUT_DOWN:
      RecordAction(UserMetricsAction("Tray_ShutDown"));
      break;
    case UMA_TRAY_SWIPE_TO_CLOSE_SUCCESSFUL:
      RecordAction(UserMetricsAction("Tray_SwipeToClose_Successful"));
      break;
    case UMA_TRAY_SWIPE_TO_CLOSE_UNSUCCESSFUL:
      RecordAction(UserMetricsAction("Tray_SwipeToClose_Unsuccessful"));
      break;
    case UMA_TRAY_SWIPE_TO_OPEN_SUCCESSFUL:
      RecordAction(UserMetricsAction("Tray_SwipeToOpen_Successful"));
      break;
    case UMA_TRAY_SWIPE_TO_OPEN_UNSUCCESSFUL:
      RecordAction(UserMetricsAction("Tray_SwipeToOpen_Unsuccessful"));
      break;
  }
}

void UserMetricsRecorder::StartDemoSessionMetricsRecording() {
  demo_session_metrics_recorder_ =
      std::make_unique<DemoSessionMetricsRecorder>();
}

void UserMetricsRecorder::OnShellInitialized() {
  // Lazy creation of the DesktopTaskSwitchMetricRecorder because it accesses
  // Shell::Get() which is not available when |this| is instantiated.
  if (!desktop_task_switch_metric_recorder_) {
    desktop_task_switch_metric_recorder_.reset(
        new DesktopTaskSwitchMetricRecorder());
  }
  pointer_metrics_recorder_ = std::make_unique<PointerMetricsRecorder>();
}

void UserMetricsRecorder::OnShellShuttingDown() {
  demo_session_metrics_recorder_.reset();
  desktop_task_switch_metric_recorder_.reset();

  // To clean up pointer_metrics_recorder_ properly, a valid shell instance is
  // required, so explicitly delete it before the shell instance becomes
  // invalid.
  pointer_metrics_recorder_.reset();
}

void UserMetricsRecorder::RecordPeriodicMetrics() {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  // TODO(bruthig): Investigating whether the check for |manager| is necessary
  // and add tests if it is.
  if (shelf) {
    // TODO(bruthig): Consider tracking the time spent in each alignment.
    UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentOverTime",
                              static_cast<ShelfAlignmentUmaEnumValue>(
                                  shelf->SelectValueForShelfAlignment(
                                      SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
                                      SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
                                      SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT)),
                              SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
  }

  if (IsUserInActiveDesktopEnvironment()) {
    RecordShelfItemCounts();
    UMA_HISTOGRAM_COUNTS_100("Ash.NumberOfVisibleWindowsInPrimaryDisplay",
                             GetNumVisibleWindowsInPrimaryDisplay());
  }

  // TODO(bruthig): Find out if this should only be logged when the user is
  // active.
  // TODO(bruthig): Consider tracking how long a particular type of window is
  // active at a time.
  UMA_HISTOGRAM_ENUMERATION("Ash.ActiveWindowShowTypeOverTime",
                            GetActiveWindowState(),
                            ACTIVE_WINDOW_STATE_TYPE_COUNT);
}

bool UserMetricsRecorder::IsUserInActiveDesktopEnvironment() const {
  return IsUserActive() && !IsKioskModeActive() && !IsArcKioskModeActive();
}

void UserMetricsRecorder::StartTimer() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAshPeriodicMetricsTimeInSeconds),
               this, &UserMetricsRecorder::RecordPeriodicMetrics);
}

}  // namespace ash
