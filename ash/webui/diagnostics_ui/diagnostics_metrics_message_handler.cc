// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "content/public/browser/web_ui.h"

namespace ash {
namespace diagnostics {
namespace metrics {

namespace {

// Handler names:
const char kRecordNavigation[] = "recordNavigation";

// Converts base::Value<int> to NavigationView based on enum values.
NavigationView ConvertToNavigationView(const base::Value& value) {
  DCHECK(value.is_int());
  DCHECK_LE(value.GetInt(), static_cast<int>(NavigationView::kMaxValue));

  return static_cast<NavigationView>(value.GetInt());
}
}  // namespace

DiagnosticsMetricsMessageHandler::DiagnosticsMetricsMessageHandler(
    NavigationView initial_view)
    : current_view_(initial_view) {
  navigation_started_ = base::Time::Now();
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

// Message Handlers
void DiagnosticsMetricsMessageHandler::HandleRecordNavigation(
    base::Value::ConstListView args) {
  DCHECK_EQ(2u, args.size());
  DCHECK_NE(args[0], args[1]);

  // `current_view_` updated to recorded `to_view` and reset timer.
  current_view_ = ConvertToNavigationView(args[1]);
  navigation_started_ = base::Time::Now();
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
