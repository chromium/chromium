// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_BASE_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_BASE_TEST_H_

// TODO(https://crbug.com/1164001): move to forward declaration when migrated
// to ash namespace
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace user_manager {
class ScopedUserManager;
}

namespace chromeos {
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
 public:
  ExistingUserControllerBaseTest();
  ~ExistingUserControllerBaseTest() override;

  MockUserManager* mock_user_manager();

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
  MockUserManager* const mock_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_BASE_TEST_H_
