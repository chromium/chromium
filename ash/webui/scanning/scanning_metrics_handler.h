// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_SCANNING_METRICS_HANDLER_H_
#define ASH_WEBUI_SCANNING_SCANNING_METRICS_HANDLER_H_

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

// ChromeOS Scan app metrics handler for recording metrics from the UI.
class ScanningMetricsHandler : public content::WebUIMessageHandler {
 public:
  ScanningMetricsHandler();
  ~ScanningMetricsHandler() override;

  ScanningMetricsHandler(const ScanningMetricsHandler&) = delete;
  ScanningMetricsHandler& operator=(const ScanningMetricsHandler&) = delete;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Records the number of scan setting changes before a scan is initiated.
  void HandleRecordNumScanSettingChanges(const base::Value::List& args);

  // Records the action taken after a completed scan job.
  void HandleRecordScanCompleteAction(const base::Value::List& args);

  // Records the settings for a scan job.
  void HandleRecordScanJobSettings(const base::Value::List& args);

  // Records the number of completed scans in a Scan app session.
  void HandleRecordNumCompletedScans(const base::Value::List& args);
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNING_SCANNING_METRICS_HANDLER_H_
