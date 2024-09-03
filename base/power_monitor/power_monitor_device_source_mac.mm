// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation based on sample code from
// http://developer.apple.com/library/mac/#qa/qa1340/_index.html.

#include "base/power_monitor/power_monitor_device_source.h"

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"

#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

namespace base {

PowerThermalObserver::DeviceThermalState
PowerMonitorDeviceSource::GetCurrentThermalState() const {
  return thermal_state_observer_->GetCurrentThermalState();
}

int PowerMonitorDeviceSource::GetInitialSpeedLimit() const {
  return thermal_state_observer_->GetCurrentSpeedLimit();
}

void PowerMonitorDeviceSource::GetBatteryState() {
  DCHECK(battery_level_provider_);
  // base::Unretained is safe because the callback is immediately invoked
  // inside `BatteryLevelProvider::GetBatteryState()`.
  battery_level_provider_->GetBatteryState(
      base::BindOnce(&PowerMonitorDeviceSource::OnBatteryStateReceived,
                     base::Unretained(this)));
}

void PowerMonitorDeviceSource::OnBatteryStateReceived(
    const std::optional<BatteryLevelProvider::BatteryState>& battery_state) {
  if (battery_state.has_value()) {
    if (battery_state->is_external_power_connected) {
      battery_power_status_ =
          PowerStateObserver::BatteryPowerStatus::kExternalPower;
    } else {
      battery_power_status_ =
          PowerStateObserver::BatteryPowerStatus::kBatteryPower;
    }
  } else {
    battery_power_status_ = PowerStateObserver::BatteryPowerStatus::kUnknown;
  }
  PowerMonitorSource::ProcessPowerEvent(PowerMonitorSource::POWER_STATE_EVENT);
}

void PowerMonitorDeviceSource::PlatformInit() {
  power_manager_port_ = IORegisterForSystemPower(
      this,
      mac::ScopedIONotificationPortRef::Receiver(notification_port_).get(),
      &SystemPowerEventCallback, &notifier_);
  DCHECK_NE(power_manager_port_, IO_OBJECT_NULL);

  // Add the sleep/wake notification event source to the runloop.
  CFRunLoopAddSource(
      CFRunLoopGetCurrent(),
      IONotificationPortGetRunLoopSource(notification_port_.get()),
      kCFRunLoopCommonModes);

  battery_level_provider_ = BatteryLevelProvider::Create();
  // Get the initial battery power status and register for all
  // future power-source-change events.
  GetBatteryState();
  // base::Unretained is safe because `this` owns `power_source_event_source_`,
  // which exclusively owns the callback.
  power_source_event_source_.Start(base::BindRepeating(
      &PowerMonitorDeviceSource::GetBatteryState, base::Unretained(this)));

  thermal_state_observer_ = std::make_unique<ThermalStateObserverMac>(
      BindRepeating(&PowerMonitorSource::ProcessThermalEvent),
      BindRepeating(&PowerMonitorSource::ProcessSpeedLimitEvent));
}

void PowerMonitorDeviceSource::PlatformDestroy() {
  CFRunLoopRemoveSource(
      CFRunLoopGetCurrent(),
      IONotificationPortGetRunLoopSource(notification_port_.get()),
      kCFRunLoopCommonModes);

  // Deregister for system power notifications.
  IODeregisterForSystemPower(&notifier_);

  // Close the connection to the IOPMrootDomain that was opened in
  // PlatformInit().
  IOServiceClose(power_manager_port_);
  power_manager_port_ = IO_OBJECT_NULL;
}

PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSource::GetBatteryPowerStatus() const {
  return battery_power_status_;
}

void PowerMonitorDeviceSource::SystemPowerEventCallback(
    void* refcon,
    io_service_t service,
    natural_t message_type,
    void* message_argument) {
  auto* thiz = static_cast<PowerMonitorDeviceSource*>(refcon);

  switch (message_type) {
    // If this message is not handled the system may delay sleep for 30 seconds.
    case kIOMessageCanSystemSleep:
      IOAllowPowerChange(thiz->power_manager_port_,
                         reinterpret_cast<intptr_t>(message_argument));
      break;
    case kIOMessageSystemWillSleep:
      PowerMonitorSource::ProcessPowerEvent(PowerMonitorSource::SUSPEND_EVENT);
      IOAllowPowerChange(thiz->power_manager_port_,
                         reinterpret_cast<intptr_t>(message_argument));
      break;

    case kIOMessageSystemWillPowerOn:
      PowerMonitorSource::ProcessPowerEvent(PowerMonitorSource::RESUME_EVENT);
      break;
  }
}

}  // namespace base
