// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_BASE_TEST_H_

#include <memory>

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AuthEventsRecorder;

namespace {

const char kFirstSAMLUserId[] = "12345";
const char kFirstSAMLUserEmail[] = "bob@corp.example.com";
const char kSecondSAMLUserId[] = "67891";
const char kSecondSAMLUserEmail[] = "alice@corp.example.com";

const char kFirstGaiaUserId[] = "88888";
const char kFirstGaiaUserEmail[] = "bob@gaia.example.com";
const char kSecondGaiaUserId[] = "88884";
const char kSecondGaiaUserEmail[] = "alice@gaia.example.com";

}  // namespace

class ExistingUserControllerBaseTest : public ::testing::Test {
 protected:
  ExistingUserControllerBaseTest();
  ~ExistingUserControllerBaseTest() override;

  FakeChromeUserManager* GetFakeUserManager();

  const AccountId saml_login_account1_id_ =
      AccountId::FromUserEmailGaiaId(kFirstSAMLUserEmail, kFirstSAMLUserId);

  const AccountId saml_login_account2_id_ =
      AccountId::FromUserEmailGaiaId(kSecondSAMLUserEmail, kSecondSAMLUserId);

  const AccountId gaia_login_account1_id_ =
      AccountId::FromUserEmailGaiaId(kFirstGaiaUserEmail, kFirstGaiaUserId);

  const AccountId gaia_login_account2_id_ =
      AccountId::FromUserEmailGaiaId(kSecondGaiaUserEmail, kSecondGaiaUserId);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  ScopedTestingLocalState scoped_local_state_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<AuthEventsRecorder> auth_events_recorder_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_BASE_TEST_H_
