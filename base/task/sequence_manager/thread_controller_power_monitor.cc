// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_power_monitor.h"

#include "base/feature_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

namespace {

// Activate the power management events that affect task scheduling.
const Feature kUsePowerMonitorWithThreadController{
    "UsePowerMonitorWithThreadController", FEATURE_ENABLED_BY_DEFAULT};

// TODO(1074332): Remove this when the experiment becomes the default.
bool g_use_thread_controller_power_monitor_ = false;

}  // namespace

ThreadControllerPowerMonitor::ThreadControllerPowerMonitor() = default;

ThreadControllerPowerMonitor::~ThreadControllerPowerMonitor() {
  PowerMonitor::RemovePowerSuspendObserver(this);
}

void ThreadControllerPowerMonitor::BindToCurrentThread() {
  // Occasionally registration happens twice (i.e. when the
  // ThreadController::SetDefaultTaskRunner() re-initializes the
  // ThreadController).
  if (is_observer_registered_)
    PowerMonitor::RemovePowerSuspendObserver(this);

  // Register the observer to deliver notifications on the current thread.
  PowerMonitor::AddPowerSuspendObserver(this);
  is_observer_registered_ = true;
}

bool ThreadControllerPowerMonitor::IsProcessInPowerSuspendState() {
  return is_power_suspended_;
}

// static
void ThreadControllerPowerMonitor::InitializeOnMainThread() {
  DCHECK(!g_use_thread_controller_power_monitor_);
  g_use_thread_controller_power_monitor_ =
      FeatureList::IsEnabled(kUsePowerMonitorWithThreadController);
}

// static
void ThreadControllerPowerMonitor::OverrideUsePowerMonitorForTesting(
    bool use_power_monitor) {
  g_use_thread_controller_power_monitor_ = use_power_monitor;
}

// static
void ThreadControllerPowerMonitor::ResetForTesting() {
  g_use_thread_controller_power_monitor_ = false;
}

void ThreadControllerPowerMonitor::OnSuspend() {
  if (!g_use_thread_controller_power_monitor_)
    return;
  DCHECK(!is_power_suspended_);

  TRACE_EVENT_BEGIN("base", "ThreadController::Suspended",
                    perfetto::Track(reinterpret_cast<uint64_t>(this),
                                    perfetto::ThreadTrack::Current()));
  is_power_suspended_ = true;
}

void ThreadControllerPowerMonitor::OnResume() {
  if (!g_use_thread_controller_power_monitor_)
    return;

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
