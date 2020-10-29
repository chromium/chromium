// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_MOCK_REPORT_SENDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_MOCK_REPORT_SENDER_H_

#include "net/url_request/report_sender.h"

namespace safe_browsing {

// A mock ReportSender that keeps track of the last report sent and the number
// of reports sent.
class MockReportSender : public net::ReportSender {
 public:
  MockReportSender();

  ~MockReportSender() override;

  void Send(
      const GURL& report_uri,
      base::StringPiece content_type,
      base::StringPiece report,
      base::OnceCallback<void()> success_callback,
      base::OnceCallback<void(const GURL&, int, int)> error_callback) override;

  const GURL& latest_report_uri();

  const std::string& latest_report();

  const std::string& latest_content_type();

  int GetAndResetNumberOfReportsSent();

  void WaitForReportSent();

 private:
  GURL latest_report_uri_;
  std::string latest_report_;
  std::string latest_content_type_;
  int number_of_reports_;
  base::Closure quit_closure_;

  void NotifyReportSentOnUIThread();

  DISALLOW_COPY_AND_ASSIGN(MockReportSender);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_MOCK_REPORT_SENDER_H_
