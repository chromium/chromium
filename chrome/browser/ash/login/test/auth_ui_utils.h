// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_AUTH_UI_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_AUTH_UI_UTILS_H_

#include <memory>

#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"

namespace ash::test {

// Password change scenario
// page for entering old password
std::unique_ptr<test::TestConditionWaiter> CreateOldPasswordEnterPageWaiter();
void PasswordChangedTypeOldPassword(const std::string& text);
void PasswordChangedSubmitOldPassword();
std::unique_ptr<test::TestConditionWaiter>
PasswordChangedInvalidPasswordFeedback();
void PasswordChangedForgotPasswordAction();

// page for cryptohome re-creation suggestion
std::unique_ptr<test::TestConditionWaiter> LocalDataLossWarningPageWaiter();
void LocalDataLossWarningPageCancelAction();
void LocalDataLossWarningPageGoBackAction();
void LocalDataLossWarningPageProceedAction();

void LocalDataLossWarningPageExpectGoBack();
void LocalDataLossWarningPageExpectProceed();

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_AUTH_UI_UTILS_H_
