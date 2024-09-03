// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_device_source.h"

#include <string>

#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/wrapped_window_proc.h"

namespace base {

void ProcessPowerEventHelper(PowerMonitorSource::PowerEvent event) {
  PowerMonitorSource::ProcessPowerEvent(event);
}

namespace {

constexpr wchar_t kWindowClassName[] = L"Base_PowerMessageWindow";

void ProcessWmPowerBroadcastMessage(WPARAM event_id) {
  PowerMonitorSource::PowerEvent power_event;
  switch (event_id) {
    case PBT_APMPOWERSTATUSCHANGE:  // The power status changed.
      power_event = PowerMonitorSource::POWER_STATE_EVENT;
      break;
    case PBT_APMRESUMEAUTOMATIC:  // Resume from suspend.
      // We don't notify for PBT_APMRESUMESUSPEND
      // because, if it occurs, it is always sent as a
      // second event after PBT_APMRESUMEAUTOMATIC.
      power_event = PowerMonitorSource::RESUME_EVENT;
      break;
    case PBT_APMSUSPEND:  // System has been suspended.
      power_event = PowerMonitorSource::SUSPEND_EVENT;
      break;
    default:
      return;

      // Other Power Events:
      // PBT_APMBATTERYLOW - removed in Vista.
      // PBT_APMOEMEVENT - removed in Vista.
      // PBT_APMQUERYSUSPEND - removed in Vista.
      // PBT_APMQUERYSUSPENDFAILED - removed in Vista.
      // PBT_APMRESUMECRITICAL - removed in Vista.
      // PBT_POWERSETTINGCHANGE - user changed the power settings.
  }

  ProcessPowerEventHelper(power_event);
}

}  // namespace

void PowerMonitorDeviceSource::PlatformInit() {
  // Only for testing.
  if (!CurrentUIThread::IsSet()) {
    return;
  }
  speed_limit_observer_ =
      std::make_unique<base::SequenceBound<SpeedLimitObserverWin>>(
          base::ThreadPool::CreateSequencedTaskRunner({}),
          BindRepeating(&PowerMonitorSource::ProcessSpeedLimitEvent));
}

void PowerMonitorDeviceSource::PlatformDestroy() {
  // Because |speed_limit_observer_| is sequence bound, the actual destruction
  // happens asynchronously on its task runner. Until this has completed it is
  // still possible for PowerMonitorSource::ProcessSpeedLimitEvent to be called.
  speed_limit_observer_.reset();
}

PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSource::GetBatteryPowerStatus() const {
  SYSTEM_POWER_STATUS status;
  if (!::GetSystemPowerStatus(&status)) {
    DPLOG(ERROR) << "GetSystemPowerStatus failed";
    return PowerStateObserver::BatteryPowerStatus::kUnknown;
  }
  return (status.ACLineStatus == 0)
             ? PowerStateObserver::BatteryPowerStatus::kBatteryPower
             : PowerStateObserver::BatteryPowerStatus::kExternalPower;
}

int PowerMonitorDeviceSource::GetInitialSpeedLimit() const {
  // Returns the maximum value once at start. Subsequent actual values will be
  // provided asynchronously via callbacks instead.
  return PowerThermalObserver::kSpeedLimitMax;
}

PowerMonitorDeviceSource::PowerMessageWindow::PowerMessageWindow() {
  if (!CurrentUIThread::IsSet()) {
    // Creating this window in (e.g.) a renderer inhibits shutdown on Windows.
    // See http://crbug.com/230122. TODO(vandebo): http://crbug.com/236031
    DLOG(ERROR)
        << "Cannot create windows on non-UI thread, power monitor disabled!";
    return;
  }
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kWindowClassName,
      &base::win::WrappedWindowProc<
          PowerMonitorDeviceSource::PowerMessageWindow::WndProcThunk>,
      0, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, &window_class);
  instance_ = window_class.hInstance;
  ATOM clazz = ::RegisterClassEx(&window_class);
  DCHECK(clazz);

  message_hwnd_ =
      ::CreateWindowEx(WS_EX_NOACTIVATE, kWindowClassName, nullptr, WS_POPUP, 0,
                       0, 0, 0, nullptr, nullptr, instance_, nullptr);
  if (message_hwnd_) {
    // On machines with modern standby calling RegisterSuspendResumeNotification
    // is required in order to get the PBT_APMSUSPEND message.
    power_notify_handle_ = ::RegisterSuspendResumeNotification(
        message_hwnd_, DEVICE_NOTIFY_WINDOW_HANDLE);
  }
}

PowerMonitorDeviceSource::PowerMessageWindow::~PowerMessageWindow() {
  if (message_hwnd_) {
    if (power_notify_handle_)
      ::UnregisterSuspendResumeNotification(power_notify_handle_);

    ::DestroyWindow(message_hwnd_);
    ::UnregisterClass(kWindowClassName, instance_);
  }
}

// static
LRESULT CALLBACK PowerMonitorDeviceSource::PowerMessageWindow::WndProcThunk(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
  switch (message) {
    case WM_POWERBROADCAST:
      ProcessWmPowerBroadcastMessage(wparam);
      return TRUE;
    default:
      return ::DefWindowProc(hwnd, message, wparam, lparam);
  }
}

}  // namespace base
