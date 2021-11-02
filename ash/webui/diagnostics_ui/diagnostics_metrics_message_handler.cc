// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"

#include "base/check.h"
#include "base/time/time.h"
#include "content/public/browser/web_ui.h"

namespace ash {
namespace diagnostics {
namespace metrics {

DiagnosticsMetricsMessageHandler::DiagnosticsMetricsMessageHandler(
    NavigationView initial_view)
    : current_view_(initial_view) {
  navigation_started_ = base::Time::Now();
}

// content::WebUIMessageHandler:
void DiagnosticsMetricsMessageHandler::RegisterMessages() {
  DCHECK(web_ui());
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
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
