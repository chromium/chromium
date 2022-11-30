// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_TEST_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_TEST_DELEGATE_H_

#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"

#include "components/account_id/account_id.h"

namespace ash {
namespace reporting {

class LoginLogoutReporterTestDelegate : public LoginLogoutReporter::Delegate {
 public:
  explicit LoginLogoutReporterTestDelegate(
      const AccountId& account_id = EmptyAccountId());
  ~LoginLogoutReporterTestDelegate() override;

  AccountId GetLastLoginAttemptAccountId() const override;

 private:
  const AccountId account_id_;
};

}  // namespace reporting
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_TEST_DELEGATE_H_
