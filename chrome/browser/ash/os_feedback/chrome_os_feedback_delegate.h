// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
#define CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_

#include <optional>
#include <string>

#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

class Profile;
class PrefService;

namespace extensions {
class FeedbackService;
}  // namespace extensions

namespace ash {

class ChromeOsFeedbackDelegate : public OsFeedbackDelegate {
 public:
  explicit ChromeOsFeedbackDelegate(content::WebUI* web_ui);
  ~ChromeOsFeedbackDelegate() override;

  ChromeOsFeedbackDelegate(const ChromeOsFeedbackDelegate&) = delete;
  ChromeOsFeedbackDelegate& operator=(const ChromeOsFeedbackDelegate&) = delete;

  // Return true if the kUserFeedbackWithLowLevelDebugDataAllowed policy
  // contains
  // - "all" or
  // - "wifi"
  static bool IsWifiDebugLogsAllowed(const PrefService* prefs);

  static ChromeOsFeedbackDelegate CreateForTesting(Profile* profile);

  static ChromeOsFeedbackDelegate CreateForTesting(
      Profile* profile,
      scoped_refptr<extensions::FeedbackService> feedback_service);

  // OsFeedbackDelegate:
  std::string GetApplicationLocale() override;
  std::optional<GURL> GetLastActivePageUrl() override;
  std::optional<std::string> GetSignedInUserEmail() const override;
  std::optional<std::string> GetLinkedPhoneMacAddress() override;
  bool IsWifiDebugLogsAllowed() const override;
  int GetPerformanceTraceId() override;
  void GetScreenshotPng(GetScreenshotPngCallback callback) override;
  void SendReport(os_feedback_ui::mojom::ReportPtr report,
                  SendReportCallback callback) override;
  void OpenDiagnosticsApp() override;
  void OpenExploreApp() override;
  void OpenMetricsDialog() override;
  void OpenSystemInfoDialog() override;
  void OpenAutofillMetadataDialog(
      const std::string& autofill_metadata) override;
  bool IsChildAccount() override;

 private:
  explicit ChromeOsFeedbackDelegate(Profile* profile);
  ChromeOsFeedbackDelegate(
      Profile* profile,
      scoped_refptr<extensions::FeedbackService> feedback_service);
  void OnSendFeedbackDone(SendReportCallback callback, bool status);
  void OpenWebDialog(GURL url, const std::string& args);
  // Loading system logs could be slow. Preload them to reduce potential user
  // wait time when sending reports.
  void PreloadSystemLogs();
  void PreloadSystemLogsDone(
      base::TimeTicks fetch_start_time,
      std::unique_ptr<system_logs::SystemLogsResponse> response);

  // TODO(xiangdongkong): make sure the profile_ cannot be destroyed while
  // operations are pending.
  raw_ptr<Profile> profile_;
  scoped_refptr<extensions::FeedbackService> feedback_service_;
  std::optional<GURL> page_url_;
  // Used to store system logs that may be needed when sending the report (i.e.,
  // the user opted in to include system logs in the report).
  std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response_;

  base::WeakPtrFactory<ChromeOsFeedbackDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
