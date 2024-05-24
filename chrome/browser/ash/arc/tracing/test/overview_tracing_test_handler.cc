// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/test/overview_tracing_test_handler.h"

namespace arc {

OverviewTracingTestHandler::OverviewTracingTestHandler(
    ArcWindowFocusChangeCb arc_window_focus_change_callback)
    : OverviewTracingHandler(std::move(arc_window_focus_change_callback)) {}

OverviewTracingTestHandler::~OverviewTracingTestHandler() = default;

void OverviewTracingTestHandler::StartTracingOnControllerRespond() {
  DCHECK(after_start_);
  std::move(after_start_).Run();
}

void OverviewTracingTestHandler::StopTracingOnControllerRespond(
    std::unique_ptr<std::string> trace_data) {
  DCHECK(after_stop_);
  std::move(after_stop_).Run(std::move(trace_data));
}

void OverviewTracingTestHandler::VerifyNoUnrespondedCallback() {
  CHECK(after_start_.is_null());
  CHECK(after_stop_.is_null());
}

base::Time OverviewTracingTestHandler::Now() {
  return now_;
}

base::TimeTicks OverviewTracingTestHandler::SystemTicksNow() {
  return now_ - trace_time_base_ + base::TimeTicks();
}

void OverviewTracingTestHandler::StartTracingOnController(
    const base::trace_event::TraceConfig& trace_config,
    content::TracingController::StartTracingDoneCallback after_start) {
  after_start_ = std::move(after_start);
}

void OverviewTracingTestHandler::StopTracingOnController(
    content::TracingController::CompletionCallback after_stop) {
  after_stop_ = std::move(after_stop);
}

OverviewTracingHandler::AppWindowList
OverviewTracingTestHandler::AllAppWindows() const {
  AppWindowList windows = non_trace_app_windows_;
  if (arc_active_window_for_testing()) {
    windows.emplace_back(arc_active_window_for_testing());
  }
  return windows;
}

}  // namespace arc
