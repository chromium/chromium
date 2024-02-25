// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_auth_attempt.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Fake user ids used in tests.
const char kTestUser1[] = "user1";
const char kTestUser2[] = "user2";

// Fake lock handler to be used in these tests.
class TestLockHandler : public proximity_auth::ScreenlockBridge::LockHandler {
 public:
  // The state of unlock procedure.
  enum AuthState {
    STATE_NONE,
    STATE_ATTEMPTING_UNLOCK,
    STATE_UNLOCK_CANCELED,
    STATE_UNLOCK_DONE
  };

  explicit TestLockHandler(const AccountId& account_id)
      : state_(STATE_NONE),
        auth_type_(proximity_auth::mojom::AuthType::USER_CLICK),
        account_id_(account_id) {}

  TestLockHandler(const TestLockHandler&) = delete;
  TestLockHandler& operator=(const TestLockHandler&) = delete;

  ~TestLockHandler() override {}

  void set_state(AuthState value) { state_ = value; }
  AuthState state() const { return state_; }

  // Not using `SetAuthType` to make sure it's not called during tests.
  void set_auth_type(proximity_auth::mojom::AuthType value) {
    auth_type_ = value;
  }

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override {
    ADD_FAILURE() << "Should not be reached.";
  }

  void EnableInput() override {
    ASSERT_EQ(STATE_ATTEMPTING_UNLOCK, state_);
    state_ = STATE_UNLOCK_CANCELED;
  }

  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const std::u16string& auth_value) override {
    ADD_FAILURE() << "Should not be reached.";
  }

  proximity_auth::mojom::AuthType GetAuthType(
      const AccountId& account_id) const override {
    return auth_type_;
  }

  ScreenType GetScreenType() const override {
    // Return an arbitrary value; this is not used by the test code.
    return LOCK_SCREEN;
  }

  void Unlock(const AccountId& account_id) override {
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != " << account_id.Serialize();
    ASSERT_EQ(STATE_ATTEMPTING_UNLOCK, state_);
    state_ = STATE_UNLOCK_DONE;
  }

  void SetSmartLockState(const AccountId& account_id,
                         SmartLockState state) override {
    GTEST_FAIL();
  }

  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool successful) override {
    GTEST_FAIL();
  }

 private:
  AuthState state_;
  proximity_auth::mojom::AuthType auth_type_;
  const AccountId account_id_;
  std::string expected_secret_;
};

class SmartLockAuthAttemptUnlockTest : public testing::Test {
 public:
  SmartLockAuthAttemptUnlockTest() {}

  SmartLockAuthAttemptUnlockTest(const SmartLockAuthAttemptUnlockTest&) =
      delete;
  SmartLockAuthAttemptUnlockTest& operator=(
      const SmartLockAuthAttemptUnlockTest&) = delete;

  ~SmartLockAuthAttemptUnlockTest() override {}

  void SetUp() override {
    auth_attempt_ = std::make_unique<SmartLockAuthAttempt>(test_account_id1_);
  }

  void TearDown() override {
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
    auth_attempt_.reset();
  }

 protected:
  void InitScreenLock() {
    lock_handler_ = std::make_unique<TestLockHandler>(test_account_id1_);
    lock_handler_->set_state(TestLockHandler::STATE_ATTEMPTING_UNLOCK);
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(
        lock_handler_.get());
  }

  std::unique_ptr<SmartLockAuthAttempt> auth_attempt_;
  std::unique_ptr<TestLockHandler> lock_handler_;

  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const AccountId test_account_id2_ = AccountId::FromUserEmail(kTestUser2);
};

TEST_F(SmartLockAuthAttemptUnlockTest, StartWhenNotLocked) {
  ASSERT_FALSE(proximity_auth::ScreenlockBridge::Get()->IsLocked());

  EXPECT_FALSE(auth_attempt_->Start());
}

TEST_F(SmartLockAuthAttemptUnlockTest, StartWhenAuthTypeIsPassword) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  lock_handler_->set_auth_type(
      proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(SmartLockAuthAttemptUnlockTest, ResetBeforeFinalizeUnlock) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_.reset();

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(SmartLockAuthAttemptUnlockTest, FinalizeUnlockFailure) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, false);

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(SmartLockAuthAttemptUnlockTest, UnlockSucceeds) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  ASSERT_EQ(TestLockHandler::STATE_UNLOCK_DONE, lock_handler_->state());
}

TEST_F(SmartLockAuthAttemptUnlockTest, FinalizeUnlockCalledForWrongUser) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id2_, true);

  // If FinalizeUnlock is called for an incorrect user, it should be ignored
  // rather than cancelling the authentication.
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  // When FinalizeUnlock is called for the correct user, it should work as
  // expected.
  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  ASSERT_EQ(TestLockHandler::STATE_UNLOCK_DONE, lock_handler_->state());
}

}  // namespace
}  // namespace ash
