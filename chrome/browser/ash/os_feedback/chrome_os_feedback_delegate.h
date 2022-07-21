// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
#define CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_

#include <string>

#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace extensions {
class FeedbackService;
}  // namespace extensions

namespace ash {

class ChromeOsFeedbackDelegate : public OsFeedbackDelegate {
 public:
  explicit ChromeOsFeedbackDelegate(Profile* profile);
  ChromeOsFeedbackDelegate(
      Profile* profile,
      scoped_refptr<extensions::FeedbackService> feedback_service);
  ~ChromeOsFeedbackDelegate() override;

  ChromeOsFeedbackDelegate(const ChromeOsFeedbackDelegate&) = delete;
  ChromeOsFeedbackDelegate& operator=(const ChromeOsFeedbackDelegate&) = delete;

  // OsFeedbackDelegate:
  std::string GetApplicationLocale() override;
  absl::optional<GURL> GetLastActivePageUrl() override;
  absl::optional<std::string> GetSignedInUserEmail() const override;
  void GetScreenshotPng(GetScreenshotPngCallback callback) override;
  void SendReport(os_feedback_ui::mojom::ReportPtr report,
                  SendReportCallback callback) override;
  void OpenDiagnosticsApp() override;
  void OpenExploreApp() override;
  void OpenMetricsDialog() override;
  void OpenSystemInfoDialog() override;

 private:
  void OnSendFeedbackDone(SendReportCallback callback, bool status);
  void OpenWebDialog(GURL url);

  // TODO(xiangdongkong): make sure the profile_ cannot be destroyed while
  // operations are pending.
  raw_ptr<Profile> profile_;
  scoped_refptr<extensions::FeedbackService> feedback_service_;
  absl::optional<GURL> page_url_;

  base::WeakPtrFactory<ChromeOsFeedbackDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
