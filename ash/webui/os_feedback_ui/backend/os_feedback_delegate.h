// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_

#include <optional>
#include <string>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"

class GURL;

namespace ash {

using GetScreenshotPngCallback =
    base::OnceCallback<void(const std::vector<uint8_t>&)>;
using SendReportCallback =
    base::OnceCallback<void(os_feedback_ui::mojom::SendReportStatus)>;

// A delegate which exposes browser functionality from //chrome to the OS
// Feedback UI.
class OsFeedbackDelegate {
 public:
  OsFeedbackDelegate() = default;
  virtual ~OsFeedbackDelegate() = default;

  // Gets the application locale so that suggested help contents can display
  // localized titles when available.
  virtual std::string GetApplicationLocale() = 0;
  // Gets the mac address associated with the current device.
  virtual std::optional<std::string> GetLinkedPhoneMacAddress() = 0;
  // Returns the last active page url before the feedback tool is opened if any.
  virtual std::optional<GURL> GetLastActivePageUrl() = 0;
  // Returns the normalized email address of the signed-in user associated with
  // the browser context, if any.
  virtual std::optional<std::string> GetSignedInUserEmail() const = 0;
  // Returns whether Wifi debug logs are allowed for the user.
  virtual bool IsWifiDebugLogsAllowed() const = 0;
  // Returns id for performance trace data. If tracing is off, returns zero.
  virtual int GetPerformanceTraceId() = 0;
  // Return the screenshot of the primary display in PNG format. It was taken
  // right before the feedback tool is launched.
  virtual void GetScreenshotPng(GetScreenshotPngCallback callback) = 0;
  // Collect data and send the report to Google.
  virtual void SendReport(os_feedback_ui::mojom::ReportPtr report,
                          SendReportCallback callback) = 0;
  // Open Diagnostics app.
  virtual void OpenDiagnosticsApp() = 0;
  // Open Explore app.
  virtual void OpenExploreApp() = 0;
  // Open metrics dialog (which displays chrome://histograms).
  virtual void OpenMetricsDialog() = 0;
  // Open system info dialog (which displays the system logs
  // to be sent with the report if the user has opted in).
  virtual void OpenSystemInfoDialog() = 0;
  // Open autofill metadata dialog (which displays the autofill logs
  // to be sent with the report if the user has opted in).
  virtual void OpenAutofillMetadataDialog(
      const std::string& autofill_metadata) = 0;
  // Gets the isChild to check if the account is a unicorn account.
  virtual bool IsChildAccount() = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_
