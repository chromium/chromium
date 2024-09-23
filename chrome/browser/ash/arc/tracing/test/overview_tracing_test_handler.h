// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_TEST_OVERVIEW_TRACING_TEST_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_TEST_OVERVIEW_TRACING_TEST_HANDLER_H_

#include "chrome/browser/ash/arc/tracing/overview_tracing_handler.h"

namespace arc {

// ARC overview tracing handler with test helpers e.g. for timer manipulation
// and responding to callbacks as done by components which are not present in a
// testing environment.
class OverviewTracingTestHandler : public OverviewTracingHandler {
 public:
  explicit OverviewTracingTestHandler(
      ArcWindowFocusChangeCb arc_window_focus_change_callback);

  OverviewTracingTestHandler(const OverviewTracingTestHandler&) = delete;

  OverviewTracingTestHandler& operator=(const OverviewTracingTestHandler&) =
      delete;

  ~OverviewTracingTestHandler() override;

  void set_now(base::Time now) { now_ = now; }

  // Sets the time relative to which SystemTicksNow should return. This is
  // typically a time near the start of the trace.
  void set_trace_time_base(base::Time trace_time_base) {
    trace_time_base_ = trace_time_base;
  }

  void set_non_trace_app_windows(AppWindowList non_trace_app_windows) {
    non_trace_app_windows_ = std::move(non_trace_app_windows);
  }

  // Invokes the callback which the OverviewTracingHandler has requested be
  // called after the trace has started.
  void StartTracingOnControllerRespond();

  // Invokes the callback which the OverviewTracingHandler has requested be
  // called after the trace has finished.
  void StopTracingOnControllerRespond(std::unique_ptr<std::string> trace_data);

  // Asserts that the two ...Respond methods have been called to unblock any
  // callbacks. These ...Respond methods are usually called by test code which
  // is invoked by the code under test, so any missing invocation of ...Respond
  // suggests that the code under test has left some callback "hanging."
  void VerifyNoUnrespondedCallback();

  // OverviewTracingHandler:
  base::Time Now() override;
  base::TimeTicks SystemTicksNow() override;

 private:
  // OverviewTracingHandler:
  void StartTracingOnController(
      const base::trace_event::TraceConfig& trace_config,
      content::TracingController::StartTracingDoneCallback after_start)
      override;
  void StopTracingOnController(
      content::TracingController::CompletionCallback after_stop) override;
  AppWindowList AllAppWindows() const override;

  // Callback which the parent class has requested be called after the
  // trace has finished.
  content::TracingController::StartTracingDoneCallback after_start_;

  // Callback which the parent class has requested be called after the
  // trace has stopped.
  content::TracingController::CompletionCallback after_stop_;

  // The time relative to which SystemTicksNow is calculated. This can be any
  // time before the start of the trace.
  base::Time trace_time_base_;

  // Current time as far as the trace handler logic is concerned.
  base::Time now_;

  // All app windows iterated by ForEachAppWindow, except the active arc window,
  // which is always iterated over.
  AppWindowList non_trace_app_windows_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_TEST_OVERVIEW_TRACING_TEST_HANDLER_H_
