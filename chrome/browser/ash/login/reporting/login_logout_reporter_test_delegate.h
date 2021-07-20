// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_TEST_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_TEST_DELEGATE_H_

#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"

#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"

namespace chromeos {
namespace reporting {

class LoginLogoutReporterTestDelegate : public LoginLogoutReporter::Delegate {
 public:
  LoginLogoutReporterTestDelegate(
      bool should_report_event,
      bool should_report_user,
      policy::DMToken dm_token,
      std::unique_ptr<::reporting::MockReportQueue> mock_queue,
      AccountId account_id = EmptyAccountId());
  ~LoginLogoutReporterTestDelegate() override;

  void CreateReportingQueue(
      std::unique_ptr<::reporting::ReportQueueConfiguration> config,
      ::reporting::ReportQueueProvider::CreateReportQueueCallback
          create_queue_cb) override;

  bool ShouldReportEvent() const override;

  bool ShouldReportUser(base::StringPiece) const override;

  policy::DMToken GetDMToken() const override;

  AccountId GetLastLoginAttemptAccountId() const override;

 private:
  const bool should_report_event_;
  const bool should_report_user_;
  const policy::DMToken dm_token_;
  std::unique_ptr<::reporting::MockReportQueue> mock_queue_;
  const AccountId account_id_;
};
}  // namespace reporting
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_TEST_DELEGATE_H_
