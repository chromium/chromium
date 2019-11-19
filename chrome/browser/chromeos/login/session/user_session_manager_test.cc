// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/user_session_manager.h"

#include "base/macros.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kFakePassword[] = "p4zzw0r(|";

// Publicly exposes lifetime methods. Note that the singleton instance
// UserSessionManager::GetInstance() can't be used since it would be reused
// between tests.
class TestUserSessionManager : public UserSessionManager {
 public:
  TestUserSessionManager() = default;
  ~TestUserSessionManager() override = default;
};

}  // namespace

class UserSessionManagerTest : public testing::Test {
 public:
  UserSessionManagerTest() {
    static_assert(
        static_cast<int>(
            UserSessionManager::PasswordConsumingService::kCount) == 2,
        "Update PasswordConsumerService_* tests");

    SessionManagerClient::InitializeFake();
    user_session_manager_ = std::make_unique<TestUserSessionManager>();
  }

  ~UserSessionManagerTest() override {
    user_session_manager_.reset();
    SessionManagerClient::Shutdown();
  }

 protected:
  void InitLoginPassword() {
    user_session_manager_->mutable_user_context_for_testing()->SetPasswordKey(
        Key(kFakePassword));
    EXPECT_FALSE(user_session_manager_->user_context()
                     .GetPasswordKey()
                     ->GetSecret()
                     .empty());
    EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  }

  // Convenience shortcut to the login password stored in
  // |user_session_manager_|'s user context.
  const std::string& GetUserSessionManagerLoginPassword() const {
    return user_session_manager_->user_context().GetPasswordKey()->GetSecret();
  }

  std::unique_ptr<TestUserSessionManager> user_session_manager_;

  // Allows UserSessionManager to request the NetworkConnectionTracker in its
  // constructor.
  content::BrowserTaskEnvironment task_environment_;

  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};

 private:
  DISALLOW_COPY_AND_ASSIGN(UserSessionManagerTest);
};

// Calling VoteForSavingLoginPassword() with |save_password| set to false for
// all |PasswordConsumerService|s should not send the password to SessionManager
// and clear it from the user context.
TEST_F(UserSessionManagerTest, PasswordConsumerService_NoSave) {
  InitLoginPassword();

  // First service votes no: Should keep password in user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_EQ(kFakePassword, GetUserSessionManagerLoginPassword());

  // Second (last) service votes no: Should remove password from user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());
}

// Calling VoteForSavingLoginPassword() with |save_password| set to true should
// send the password to SessionManager and clear it from the user context once
// all services have voted.
TEST_F(UserSessionManagerTest, PasswordConsumerService_Save) {
  InitLoginPassword();

  // First service votes yes: Should send password and remove from user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());

  // Second service votes yes: Shouldn't change anything.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());
}

// Calling OnPasswordConsumingServicePolicyParsed() with |save_password| set to
// false for one service, followed by true, should send the password to
// SessionManager on the second service and clear it from the user context.
TEST_F(UserSessionManagerTest, PasswordConsumerService_NoSave_Save) {
  InitLoginPassword();

  // First service votes no: Should keep password in user context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_EQ(kFakePassword, GetUserSessionManagerLoginPassword());

  // Second service votes yes: Should save password and remove from user
  // context.
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kKerberos, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(GetUserSessionManagerLoginPassword().empty());
}

}  // namespace chromeos
