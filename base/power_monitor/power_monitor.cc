// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "power_observer.h"

namespace base {
namespace {

perfetto::StaticString BatteryStatusToString(
    PowerStateObserver::BatteryPowerStatus status) {
  switch (status) {
    case PowerStateObserver::BatteryPowerStatus::kBatteryPower:
      return "BatteryPower";
    case PowerStateObserver::BatteryPowerStatus::kExternalPower:
      return "ExternalPower";
    case PowerStateObserver::BatteryPowerStatus::kUnknown:
      return "Unknown";
  }
  NOTREACHED();
}

}  // namespace

void PowerMonitor::Initialize(std::unique_ptr<PowerMonitorSource> source,
                              bool emit_global_event) {
  DCHECK(!IsInitialized());
  source_ = std::move(source);
  emit_global_event_ = emit_global_event;
  if (emit_global_event_) {
    TrackEvent::AddSessionObserver(this);
  }

  // When a power source is associated with the power monitor, ensure the
  // initial state is propagated to observers, if needed.
  NotifyPowerStateChange(Source()->GetBatteryPowerStatus());

  NotifyThermalStateChange(Source()->GetCurrentThermalState());

  NotifySpeedLimitChange(Source()->GetInitialSpeedLimit());
}

bool PowerMonitor::IsInitialized() const {
  return source_ != nullptr;
}

void PowerMonitor::AddPowerSuspendObserver(PowerSuspendObserver* obs) {
  power_suspend_observers_->AddObserver(obs);
}

void PowerMonitor::RemovePowerSuspendObserver(PowerSuspendObserver* obs) {
  power_suspend_observers_->RemoveObserver(obs);
}

void PowerMonitor::AddPowerStateObserver(PowerStateObserver* obs) {
  power_state_observers_->AddObserver(obs);
}

void PowerMonitor::RemovePowerStateObserver(PowerStateObserver* obs) {
  power_state_observers_->RemoveObserver(obs);
}

void PowerMonitor::AddPowerThermalObserver(PowerThermalObserver* obs) {
  thermal_state_observers_->AddObserver(obs);
}

void PowerMonitor::RemovePowerThermalObserver(PowerThermalObserver* obs) {
  thermal_state_observers_->RemoveObserver(obs);
}

bool PowerMonitor::AddPowerSuspendObserverAndReturnSuspendedState(
    PowerSuspendObserver* obs) {
  AutoLock auto_lock(is_system_suspended_lock_);
  power_suspend_observers_->AddObserver(obs);
  return is_system_suspended_;
}

PowerStateObserver::BatteryPowerStatus
PowerMonitor::AddPowerStateObserverAndReturnBatteryPowerStatus(
    PowerStateObserver* obs) {
  AutoLock auto_lock(battery_power_status_lock_);
  power_state_observers_->AddObserver(obs);
  return battery_power_status_;
}

// static
PowerThermalObserver::DeviceThermalState
PowerMonitor::AddPowerStateObserverAndReturnPowerThermalState(
    PowerThermalObserver* obs) {
  AutoLock auto_lock(power_thermal_state_lock_);
  thermal_state_observers_->AddObserver(obs);
  return power_thermal_state_;
}

const PowerMonitorSource* PowerMonitor::Source() const {
  return source_.get();
}

bool PowerMonitor::IsOnBatteryPower() const {
  DCHECK(IsInitialized());
  return GetBatteryPowerStatus() ==
         PowerStateObserver::BatteryPowerStatus::kBatteryPower;
}

PowerStateObserver::BatteryPowerStatus PowerMonitor::GetBatteryPowerStatus()
    const {
  DCHECK(IsInitialized());
  AutoLock auto_lock(battery_power_status_lock_);
  return battery_power_status_;
}

TimeTicks PowerMonitor::GetLastSystemResumeTime() const {
  AutoLock auto_lock(is_system_suspended_lock_);
  return last_system_resume_time_;
}

void PowerMonitor::ShutdownForTesting() {
  source_ = nullptr;

  {
    AutoLock auto_lock(is_system_suspended_lock_);
    is_system_suspended_ = false;
    last_system_resume_time_ = TimeTicks();
  }
  {
    AutoLock auto_lock(battery_power_status_lock_);
    battery_power_status_ = PowerStateObserver::BatteryPowerStatus::kUnknown;
  }
  {
    AutoLock auto_lock(power_thermal_state_lock_);
    power_thermal_state_ = PowerThermalObserver::DeviceThermalState::kUnknown;
  }
}

// static
PowerThermalObserver::DeviceThermalState PowerMonitor::GetCurrentThermalState()
    const {
  DCHECK(IsInitialized());
  return source_->GetCurrentThermalState();
}

// static
void PowerMonitor::SetCurrentThermalState(
    PowerThermalObserver::DeviceThermalState state) {
  DCHECK(IsInitialized());
  source_->SetCurrentThermalState(state);
}

void PowerMonitor::NotifyPowerStateChange(bool on_battery_power) {
  DCHECK(IsInitialized());
  NotifyPowerStateChange(
      on_battery_power
          ? PowerStateObserver::BatteryPowerStatus::kBatteryPower
          : PowerStateObserver::BatteryPowerStatus::kExternalPower);
}

void PowerMonitor::NotifyPowerStateChange(
    PowerStateObserver::BatteryPowerStatus battery_power_status) {
  DCHECK(IsInitialized());

  if (battery_power_status ==
      PowerStateObserver::BatteryPowerStatus::kUnknown) {
    DVLOG(1) << "PowerStateChange: with unknown value";
  } else {
    DVLOG(1) << "PowerStateChange: "
             << (battery_power_status ==
                         PowerStateObserver::BatteryPowerStatus::kBatteryPower
                     ? "On"
                     : "Off")
             << " battery";
    if (emit_global_event_) {
      TRACE_EVENT_END("base.power", battery_power_track_);
      TRACE_EVENT_BEGIN("base.power",
                        BatteryStatusToString(battery_power_status),
                        battery_power_track_);
    }
  }

  AutoLock auto_lock(battery_power_status_lock_);
  if (battery_power_status_ != battery_power_status) {
    battery_power_status_ = battery_power_status;
    power_state_observers_->Notify(
        FROM_HERE, &PowerStateObserver::OnBatteryPowerStatusChange,
        battery_power_status);
  }
}

void PowerMonitor::NotifySuspend() {
  DCHECK(IsInitialized());
  DVLOG(1) << "Power Suspending";
  if (emit_global_event_) {
    TRACE_EVENT_INSTANT("base.power", "PowerMonitor::NotifySuspend",
                        suspend_track_);
  }

  AutoLock auto_lock(is_system_suspended_lock_);
  if (!is_system_suspended_) {
    if (emit_global_event_) {
      TRACE_EVENT_BEGIN("base.power", "PowerMonitor::Suspended",
                        suspend_track_);
    }
    is_system_suspended_ = true;
    last_system_resume_time_ = TimeTicks::Max();
    power_suspend_observers_->Notify(FROM_HERE,
                                     &PowerSuspendObserver::OnSuspend);
  }
}

void PowerMonitor::NotifyResume() {
  DCHECK(IsInitialized());
  DVLOG(1) << "Power Resuming";
  if (emit_global_event_) {
    TRACE_EVENT_INSTANT("base.power", "PowerMonitor::NotifyResume",
                        suspend_track_);
  }

  TimeTicks resume_time = TimeTicks::Now();

  AutoLock auto_lock(is_system_suspended_lock_);
  if (is_system_suspended_) {
    if (emit_global_event_) {
      TRACE_EVENT_END("base.power", suspend_track_);
    }
    is_system_suspended_ = false;
    last_system_resume_time_ = resume_time;
    power_suspend_observers_->Notify(FROM_HERE,
                                     &PowerSuspendObserver::OnResume);
  }
}

void PowerMonitor::NotifyThermalStateChange(
    PowerThermalObserver::DeviceThermalState new_state) {
  DCHECK(IsInitialized());
  DVLOG(1) << "ThermalStateChange: "
           << PowerMonitorSource::DeviceThermalStateToString(new_state);

  AutoLock auto_lock(power_thermal_state_lock_);
  if (power_thermal_state_ != new_state) {
    power_thermal_state_ = new_state;
    thermal_state_observers_->Notify(
        FROM_HERE, &PowerThermalObserver::OnThermalStateChange, new_state);
  }
}

void PowerMonitor::NotifySpeedLimitChange(int speed_limit) {
  DCHECK(IsInitialized());
  DVLOG(1) << "SpeedLimitChange: " << speed_limit;

  AutoLock auto_lock(power_thermal_state_lock_);
  if (speed_limit_ != speed_limit) {
    speed_limit_ = speed_limit;
    thermal_state_observers_->Notify(
        FROM_HERE, &PowerThermalObserver::OnSpeedLimitChange, speed_limit);
  }
}

PowerMonitor* PowerMonitor::GetInstance() {
  static base::NoDestructor<PowerMonitor> power_monitor;
  return power_monitor.get();
}

PowerMonitor::PowerMonitor()
    : suspend_track_("SuspendStatus", 0, perfetto::Track()),
      battery_power_track_("BatteryPowerStatus", 0, perfetto::Track()),
      power_state_observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerStateObserver>>()),
      power_suspend_observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerSuspendObserver>>()),
      thermal_state_observers_(
          base::MakeRefCounted<
              ObserverListThreadSafe<PowerThermalObserver>>()) {}

PowerMonitor::~PowerMonitor() = default;

void PowerMonitor::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  if (!emit_global_event_) {
    return;
  }
  {
    AutoLock auto_lock(is_system_suspended_lock_);
    if (is_system_suspended_) {
      TRACE_EVENT_END("base.power", suspend_track_);
      TRACE_EVENT_BEGIN("base.power", "PowerMonitor::Suspended",
                        suspend_track_);
    }
  }
  {
    AutoLock auto_lock(battery_power_status_lock_);
    TRACE_EVENT_END("base.power", battery_power_track_);
    TRACE_EVENT_BEGIN("base.power",
                      BatteryStatusToString(battery_power_status_),
                      battery_power_track_);
  }
}

}  // namespace base
