// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter_test_delegate.h"

namespace ash {
namespace reporting {

LoginLogoutReporterTestDelegate::~LoginLogoutReporterTestDelegate() = default;

LoginLogoutReporterTestDelegate::LoginLogoutReporterTestDelegate(
    const AccountId& account_id)
    : account_id_(account_id) {}

AccountId LoginLogoutReporterTestDelegate::GetLastLoginAttemptAccountId()
    const {
  return account_id_;
}

}  // namespace reporting
}  // namespace ash
