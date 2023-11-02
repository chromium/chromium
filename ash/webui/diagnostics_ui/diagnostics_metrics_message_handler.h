// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_MESSAGE_HANDLER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_MESSAGE_HANDLER_H_

#include "base/time/time.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUI;
}

namespace ash {
namespace diagnostics {
namespace metrics {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml: CrosDiagnosticsNavigationView.
enum class NavigationView {
  kSystem = 0,
  kConnectivity = 1,
  kInput = 2,
  kMaxValue = kInput,
};

// ChromeOS Diagnostics app metrics handler for recording metrics from the UI.
class DiagnosticsMetricsMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit DiagnosticsMetricsMessageHandler(NavigationView initial_view);
  DiagnosticsMetricsMessageHandler(DiagnosticsMetricsMessageHandler&) = delete;
  DiagnosticsMetricsMessageHandler& operator=(
      DiagnosticsMetricsMessageHandler&) = delete;
  ~DiagnosticsMetricsMessageHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Test helper functions:
  NavigationView GetCurrentViewForTesting();
  base::TimeDelta GetElapsedNavigationTimeDeltaForTesting();
  void SetWebUiForTesting(content::WebUI* web_ui);

 private:
  // Records navigation events between screens within Diagnostics App.
  void HandleRecordNavigation(const base::Value::List& args);

  NavigationView current_view_;
  base::Time navigation_started_;
};
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_MESSAGE_HANDLER_H_
