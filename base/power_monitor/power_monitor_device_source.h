// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_POWER_MONITOR_DEVICE_SOURCE_H_
#define BASE_POWER_MONITOR_POWER_MONITOR_DEVICE_SOURCE_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // !OS_WIN

#if defined(OS_IOS)
#include <objc/runtime.h>
#endif  // OS_IOS

namespace base {

// A class used to monitor the power state change and notify the observers about
// the change event.
class BASE_EXPORT PowerMonitorDeviceSource : public PowerMonitorSource {
 public:
  PowerMonitorDeviceSource();
  ~PowerMonitorDeviceSource() override;

#if defined(OS_MACOSX)
  // Allocate system resources needed by the PowerMonitor class.
  //
  // This function must be called before instantiating an instance of the class
  // and before the Sandbox is initialized.
#if !defined(OS_IOS)
  static void AllocateSystemIOPorts();
#else
  static void AllocateSystemIOPorts() {}
#endif  // OS_IOS
#endif  // OS_MACOSX

#if defined(OS_CHROMEOS)
  // On Chrome OS, Chrome receives power-related events from powerd, the system
  // power daemon, via D-Bus signals received on the UI thread. base can't
  // directly depend on that code, so this class instead exposes static methods
  // so that events can be passed in.
  static void SetPowerSource(bool on_battery);
  static void HandleSystemSuspending();
  static void HandleSystemResumed();
#endif

 private:
#if defined(OS_WIN)
  // Represents a message-only window for power message handling on Windows.
  // Only allow PowerMonitor to create it.
  class PowerMessageWindow {
   public:
    PowerMessageWindow();
    ~PowerMessageWindow();

   private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam);
    // Instance of the module containing the window procedure.
    HMODULE instance_;
    // A hidden message-only window.
    HWND message_hwnd_;
  };
#endif  // OS_WIN

#if defined(OS_MACOSX)
  void PlatformInit();
  void PlatformDestroy();
#endif

  // Platform-specific method to check whether the system is currently
  // running on battery power.  Returns true if running on batteries,
  // false otherwise.
  bool IsOnBatteryPowerImpl() override;

#if defined(OS_IOS)
  // Holds pointers to system event notification observers.
  std::vector<id> notification_observers_;
#endif

#if defined(OS_WIN)
  PowerMessageWindow power_message_window_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorDeviceSource);
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_POWER_MONITOR_DEVICE_SOURCE_H_
