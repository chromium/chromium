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

void PowerMonitor::AddObserver(PowerObserver* obs) {
  GetInstance()->observers_->AddObserver(obs);
}

void PowerMonitor::RemoveObserver(PowerObserver* obs) {
  GetInstance()->observers_->RemoveObserver(obs);
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
PowerObserver::DeviceThermalState PowerMonitor::GetCurrentThermalState() {
  DCHECK(IsInitialized());
  return GetInstance()->source_->GetCurrentThermalState();
}

// static
void PowerMonitor::SetCurrentThermalState(
    PowerObserver::DeviceThermalState state) {
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
  GetInstance()->observers_->Notify(
      FROM_HERE, &PowerObserver::OnPowerStateChange, battery_in_use);
}

void PowerMonitor::NotifySuspend() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifySuspend",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Suspending";
  g_is_process_suspended.store(true, std::memory_order_relaxed);
  GetInstance()->observers_->Notify(FROM_HERE, &PowerObserver::OnSuspend);
}

void PowerMonitor::NotifyResume() {
  DCHECK(IsInitialized());
  TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifyResume",
                       TRACE_EVENT_SCOPE_PROCESS);
  DVLOG(1) << "Power Resuming";
  g_is_process_suspended.store(false, std::memory_order_relaxed);
  GetInstance()->observers_->Notify(FROM_HERE, &PowerObserver::OnResume);
}

void PowerMonitor::NotifyThermalStateChange(
    PowerObserver::DeviceThermalState new_state) {
  DCHECK(IsInitialized());
  DVLOG(1) << "ThermalStateChange: "
           << PowerMonitorSource::DeviceThermalStateToString(new_state);
  GetInstance()->observers_->Notify(
      FROM_HERE, &PowerObserver::OnThermalStateChange, new_state);
}

PowerMonitor* PowerMonitor::GetInstance() {
  static base::NoDestructor<PowerMonitor> power_monitor;
  return power_monitor.get();
}

PowerMonitor::PowerMonitor()
    : observers_(
          base::MakeRefCounted<ObserverListThreadSafe<PowerObserver>>()) {}

PowerMonitor::~PowerMonitor() = default;

}  // namespace base
