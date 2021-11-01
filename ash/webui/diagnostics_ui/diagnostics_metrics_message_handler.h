// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_MESSAGE_HANDLER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_MESSAGE_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUI;
}

namespace ash {
namespace diagnostics {
namespace metrics {

// ChromeOS Diagnostics app metrics handler for recording metrics from the UI.
class DiagnosticsMetricsMessageHandler : public content::WebUIMessageHandler {
 public:
  DiagnosticsMetricsMessageHandler() = default;
  DiagnosticsMetricsMessageHandler(DiagnosticsMetricsMessageHandler&) = delete;
  DiagnosticsMetricsMessageHandler& operator=(
      DiagnosticsMetricsMessageHandler&) = delete;
  ~DiagnosticsMetricsMessageHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  // Test helper functions:
  void SetWebUiForTesting(content::WebUI* web_ui);
};
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_MESSAGE_HANDLER_H_
