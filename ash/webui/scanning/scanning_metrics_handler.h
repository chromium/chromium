// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_SCANNING_METRICS_HANDLER_H_
#define ASH_WEBUI_SCANNING_SCANNING_METRICS_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

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
  void HandleRecordNumScanSettingChanges(const base::ListValue* args);

  // Records the action taken after a completed scan job.
  void HandleRecordScanCompleteAction(const base::ListValue* args);

  // Records the settings for a scan job.
  void HandleRecordScanJobSettings(const base::ListValue* args);

  // Records the number of completed scans in a Scan app session.
  void HandleRecordNumCompletedScans(const base::ListValue* args);
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNING_SCANNING_METRICS_HANDLER_H_
