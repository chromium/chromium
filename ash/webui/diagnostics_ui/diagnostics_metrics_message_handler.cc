// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"

#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "content/public/browser/web_ui.h"

namespace ash {
namespace diagnostics {
namespace metrics {
namespace {

// Handler names:
const char kRecordNavigation[] = "recordNavigation";

// Checks Javascript input integrity before parsing.
bool IsValidNavigationViewValue(const base::Value& value) {
  return value.is_int() && value.GetInt() >= 0 &&
         value.GetInt() <= static_cast<int>(NavigationView::kMaxValue);
}

// Converts base::Value<int> to NavigationView based on enum values.
NavigationView ConvertToNavigationView(const base::Value& value) {
  DCHECK(IsValidNavigationViewValue(value));

  return static_cast<NavigationView>(value.GetInt());
}

void EmitScreenOpenDuration(const NavigationView screen,
                            const base::TimeDelta& time_elapsed) {
  // Map of screens within Diagnostics app to matching duration metric name.
  constexpr auto kOpenDurationMetrics =
      base::MakeFixedFlatMap<NavigationView, std::string_view>({
          {NavigationView::kConnectivity,
           "ChromeOS.DiagnosticsUi.Connectivity.OpenDuration"},
          {NavigationView::kInput, "ChromeOS.DiagnosticsUi.Input.OpenDuration"},
          {NavigationView::kSystem,
           "ChromeOS.DiagnosticsUi.System.OpenDuration"},
      });

  auto iter = kOpenDurationMetrics.find(screen);
  if (iter == kOpenDurationMetrics.end()) {
    NOTREACHED() << "Unknown NavigationView requested";
  }

  base::UmaHistogramLongTimes100(iter->second, time_elapsed);
}

}  // namespace

DiagnosticsMetricsMessageHandler::DiagnosticsMetricsMessageHandler(
    NavigationView initial_view)
    : current_view_(initial_view) {
  navigation_started_ = base::Time::Now();
}

DiagnosticsMetricsMessageHandler::~DiagnosticsMetricsMessageHandler() {
  // Emit final navigation event.
  EmitScreenOpenDuration(current_view_,
                         base::Time::Now() - navigation_started_);
}

// content::WebUIMessageHandler:
void DiagnosticsMetricsMessageHandler::RegisterMessages() {
  DCHECK(web_ui());

  web_ui()->RegisterMessageCallback(
      kRecordNavigation,
      base::BindRepeating(
          &DiagnosticsMetricsMessageHandler::HandleRecordNavigation,
          base::Unretained(this)));
}

// Test helpers:
NavigationView DiagnosticsMetricsMessageHandler::GetCurrentViewForTesting() {
  return current_view_;
}

base::TimeDelta
DiagnosticsMetricsMessageHandler::GetElapsedNavigationTimeDeltaForTesting() {
  return base::Time::Now() - navigation_started_;
}

void DiagnosticsMetricsMessageHandler::SetWebUiForTesting(
    content::WebUI* web_ui) {
  DCHECK(web_ui);

  set_web_ui(web_ui);
}

// Private:

// Message Handlers:
void DiagnosticsMetricsMessageHandler::HandleRecordNavigation(
    const base::Value::List& args) {
  // Ensure JS arguments received are valid before using in calls to metrics.
  if (args.size() != 2u || !IsValidNavigationViewValue(args[0]) ||
      !IsValidNavigationViewValue(args[1]) || args[0] == args[1]) {
    return;
  }

  const NavigationView from_view = ConvertToNavigationView(args[0]);
  const NavigationView to_view = ConvertToNavigationView(args[1]);
  const base::Time updated_start_time = base::Time::Now();

  // Recordable navigation event occurred.
  EmitScreenOpenDuration(from_view, updated_start_time - navigation_started_);

  // `current_view_` updated to recorded `to_view` and reset timer.
  current_view_ = to_view;
  navigation_started_ = updated_start_time;
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
