// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "power_observer.h"

namespace base {

void PowerMonitor::Initialize(std::unique_ptr<PowerMonitorSource> source) {
  DCHECK(!IsInitialized());
  source_ = std::move(source);

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

// static
bool PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(
    PowerStateObserver* obs) {
  return AddPowerStateObserverAndReturnBatteryPowerStatus(obs) ==
         PowerStateObserver::BatteryPowerStatus::kBatteryPower;
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

#if BUILDFLAG(IS_ANDROID)
int PowerMonitor::GetRemainingBatteryCapacity() const {
  DCHECK(IsInitialized());
  return Source()->GetRemainingBatteryCapacity();
}
#endif  // BUILDFLAG(IS_ANDROID)

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
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifySuspend",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Suspending";

  AutoLock auto_lock(is_system_suspended_lock_);
  if (!is_system_suspended_) {
    is_system_suspended_ = true;
    last_system_resume_time_ = TimeTicks::Max();
    power_suspend_observers_->Notify(FROM_HERE,
                                     &PowerSuspendObserver::OnSuspend);
  }
}

void PowerMonitor::NotifyResume() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifyResume",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Resuming";

  TimeTicks resume_time = TimeTicks::Now();

  AutoLock auto_lock(is_system_suspended_lock_);
  if (is_system_suspended_) {
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
    : power_state_observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerStateObserver>>()),
      power_suspend_observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerSuspendObserver>>()),
      thermal_state_observers_(
          base::MakeRefCounted<
              ObserverListThreadSafe<PowerThermalObserver>>()) {}

PowerMonitor::~PowerMonitor() = default;

}  // namespace base
