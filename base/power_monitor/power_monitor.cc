// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include <atomic>
#include <utility>

#include "base/logging.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

namespace {
std::atomic_bool g_is_process_suspended{false};
}

void PowerMonitor::Initialize(std::unique_ptr<PowerMonitorSource> source) {
  DCHECK(!IsInitialized());
  GetInstance()->source_ = std::move(source);
}

bool PowerMonitor::IsInitialized() {
  return GetInstance()->source_.get() != nullptr;
}

// static
void PowerMonitor::AddPowerSuspendObserver(PowerSuspendObserver* obs) {
  GetInstance()->power_suspend_observers_->AddObserver(obs);
}

// static
void PowerMonitor::RemovePowerSuspendObserver(PowerSuspendObserver* obs) {
  GetInstance()->power_suspend_observers_->RemoveObserver(obs);
}

// static
void PowerMonitor::AddPowerStateObserver(PowerStateObserver* obs) {
  GetInstance()->power_state_observers_->AddObserver(obs);
}

// static
void PowerMonitor::RemovePowerStateObserver(PowerStateObserver* obs) {
  GetInstance()->power_state_observers_->RemoveObserver(obs);
}

// static
void PowerMonitor::AddPowerThermalObserver(PowerThermalObserver* obs) {
  GetInstance()->thermal_state_observers_->AddObserver(obs);
}

// static
void PowerMonitor::RemovePowerThermalObserver(PowerThermalObserver* obs) {
  GetInstance()->thermal_state_observers_->RemoveObserver(obs);
}

PowerMonitorSource* PowerMonitor::Source() {
  return GetInstance()->source_.get();
}

bool PowerMonitor::IsOnBatteryPower() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->IsOnBatteryPower();
}

void PowerMonitor::ShutdownForTesting() {
  GetInstance()->source_ = nullptr;
  g_is_process_suspended.store(false, std::memory_order_relaxed);
}

bool PowerMonitor::IsProcessSuspended() {
  return g_is_process_suspended.load(std::memory_order_relaxed);
}

// static
PowerThermalObserver::DeviceThermalState
PowerMonitor::GetCurrentThermalState() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->GetCurrentThermalState();
}

// static
void PowerMonitor::SetCurrentThermalState(
    PowerThermalObserver::DeviceThermalState state) {
  DCHECK(IsInitialized());
  GetInstance()->source_->SetCurrentThermalState(state);
}

#if defined(OS_ANDROID)
int PowerMonitor::GetRemainingBatteryCapacity() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->GetRemainingBatteryCapacity();
}
#endif  // defined(OS_ANDROID)

void PowerMonitor::NotifyPowerStateChange(bool battery_in_use) {
  DCHECK(IsInitialized());
  DVLOG(1) << "PowerStateChange: " << (battery_in_use ? "On" : "Off")
           << " battery";
  GetInstance()->power_state_observers_->Notify(
      FROM_HERE, &PowerStateObserver::OnPowerStateChange, battery_in_use);
}

void PowerMonitor::NotifySuspend() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifySuspend",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Suspending";
  g_is_process_suspended.store(true, std::memory_order_relaxed);
  GetInstance()->power_suspend_observers_->Notify(
      FROM_HERE, &PowerSuspendObserver::OnSuspend);
}

void PowerMonitor::NotifyResume() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifyResume",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Resuming";
  g_is_process_suspended.store(false, std::memory_order_relaxed);
  GetInstance()->power_suspend_observers_->Notify(
      FROM_HERE, &PowerSuspendObserver::OnResume);
}

void PowerMonitor::NotifyThermalStateChange(
    PowerThermalObserver::DeviceThermalState new_state) {
  DCHECK(IsInitialized());
  DVLOG(1) << "ThermalStateChange: "
           << PowerMonitorSource::DeviceThermalStateToString(new_state);
  GetInstance()->thermal_state_observers_->Notify(
      FROM_HERE, &PowerThermalObserver::OnThermalStateChange, new_state);
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
