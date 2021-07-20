// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter_test_delegate.h"
#include "base/memory/ptr_util.h"

namespace chromeos {
namespace reporting {

LoginLogoutReporterTestDelegate::~LoginLogoutReporterTestDelegate() = default;

LoginLogoutReporterTestDelegate::LoginLogoutReporterTestDelegate(
    bool should_report_event,
    bool should_report_user,
    policy::DMToken dm_token,
    std::unique_ptr<::reporting::MockReportQueue> mock_queue,
    AccountId account_id)
    : should_report_event_(should_report_event),
      should_report_user_(should_report_user),
      dm_token_(dm_token),
      mock_queue_(std::move(mock_queue)),
      account_id_(account_id) {}

bool LoginLogoutReporterTestDelegate::ShouldReportEvent() const {
  return should_report_event_;
}

bool LoginLogoutReporterTestDelegate::ShouldReportUser(
    base::StringPiece) const {
  return should_report_user_;
}

policy::DMToken LoginLogoutReporterTestDelegate::GetDMToken() const {
  return dm_token_;
}

void LoginLogoutReporterTestDelegate::CreateReportingQueue(
    std::unique_ptr<::reporting::ReportQueueConfiguration> config,
    ::reporting::ReportQueueProvider::CreateReportQueueCallback
        create_queue_cb) {
  std::move(create_queue_cb).Run(std::move(mock_queue_));
}

AccountId LoginLogoutReporterTestDelegate::GetLastLoginAttemptAccountId()
    const {
  return account_id_;
}
}  // namespace reporting
}  // namespace chromeos
