// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_power_monitor.h"

#include "base/feature_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

ThreadControllerPowerMonitor::ThreadControllerPowerMonitor() = default;

ThreadControllerPowerMonitor::~ThreadControllerPowerMonitor() {
  PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void ThreadControllerPowerMonitor::BindToCurrentThread() {
  // Occasionally registration happens twice (i.e. when the
  // ThreadController::SetDefaultTaskRunner() re-initializes the
  // ThreadController).
  auto* power_monitor = PowerMonitor::GetInstance();
  if (is_observer_registered_)
    power_monitor->RemovePowerSuspendObserver(this);

  // Register the observer to deliver notifications on the current thread.
  power_monitor->AddPowerSuspendObserver(this);
  is_observer_registered_ = true;
}

bool ThreadControllerPowerMonitor::IsProcessInPowerSuspendState() {
  return is_power_suspended_;
}

void ThreadControllerPowerMonitor::OnSuspend() {
  DCHECK(!is_power_suspended_);

  TRACE_EVENT_BEGIN("base", "ThreadController::Suspended",
                    perfetto::Track(reinterpret_cast<uint64_t>(this),
                                    perfetto::ThreadTrack::Current()));
  is_power_suspended_ = true;
}

void ThreadControllerPowerMonitor::OnResume() {
  // It is possible a suspend was already happening before the observer was
  // added to the power monitor. Ignoring the resume notification in that case.
  if (is_power_suspended_) {
    TRACE_EVENT_END("base" /* ThreadController::Suspended */,
                    perfetto::Track(reinterpret_cast<uint64_t>(this),
                                    perfetto::ThreadTrack::Current()));
    is_power_suspended_ = false;
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
