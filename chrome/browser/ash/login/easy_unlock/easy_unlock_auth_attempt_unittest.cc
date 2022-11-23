// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_auth_attempt.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Fake user ids used in tests.
const char kTestUser1[] = "user1";
const char kTestUser2[] = "user2";

const unsigned char kSecret[] = {0x7c, 0x85, 0x82, 0x7d, 0x00, 0x1f,
                                 0x6a, 0x29, 0x2f, 0xc4, 0xb5, 0x60,
                                 0x08, 0x9b, 0xb0, 0x5b};

const unsigned char kSessionKey[] = {0xc3, 0xd9, 0x83, 0x16, 0x52, 0xde,
                                     0x99, 0xd7, 0x4e, 0x60, 0xf9, 0xec,
                                     0xa8, 0x9c, 0x0e, 0xbe};

const unsigned char kWrappedSecret[] = {
    0x3a, 0xea, 0x51, 0xd9, 0x64, 0x64, 0xe1, 0xcd, 0xd8, 0xee, 0x99,
    0xf5, 0xb1, 0xd4, 0x9f, 0xc4, 0x28, 0xd6, 0xfd, 0x69, 0x0b, 0x9e,
    0x06, 0x21, 0xfc, 0x40, 0x1f, 0xeb, 0x75, 0x64, 0x52, 0xd8};

std::string GetSecret() {
  return std::string(reinterpret_cast<const char*>(kSecret),
                     std::size(kSecret));
}

std::string GetWrappedSecret() {
  return std::string(reinterpret_cast<const char*>(kWrappedSecret),
                     std::size(kWrappedSecret));
}

std::string GetSessionKey() {
  return std::string(reinterpret_cast<const char*>(kSessionKey),
                     std::size(kSessionKey));
}

// Fake lock handler to be used in these tests.
class TestLockHandler : public proximity_auth::ScreenlockBridge::LockHandler {
 public:
  // The state of unlock/signin procedure.
  enum AuthState {
    STATE_NONE,
    STATE_ATTEMPTING_UNLOCK,
    STATE_UNLOCK_CANCELED,
    STATE_UNLOCK_DONE,
    STATE_ATTEMPTING_SIGNIN,
    STATE_SIGNIN_CANCELED,
    STATE_SIGNIN_DONE
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

  // Sets the secret that is expected to be sent to `AttemptEasySignin`
  void set_expected_secret(const std::string& value) {
    expected_secret_ = value;
  }

  // Not using `SetAuthType` to make sure it's not called during tests.
  void set_auth_type(proximity_auth::mojom::AuthType value) {
    auth_type_ = value;
  }

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override {
    ADD_FAILURE() << "Should not be reached.";
  }

  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconInfo& icon_info)
      override {
    ADD_FAILURE() << "Should not be reached.";
  }

  void HideUserPodCustomIcon(const AccountId& account_id) override {
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

  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override {
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != " << account_id.Serialize();

    ASSERT_EQ(STATE_ATTEMPTING_SIGNIN, state_);
    if (secret.empty()) {
      state_ = STATE_SIGNIN_CANCELED;
    } else {
      ASSERT_EQ(expected_secret_, secret);
      ASSERT_EQ(EasyUnlockKeyManager::GetKeyLabel(0u), key_label);
      state_ = STATE_SIGNIN_DONE;
    }
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

class EasyUnlockAuthAttemptUnlockTest : public testing::Test {
 public:
  EasyUnlockAuthAttemptUnlockTest() {}

  EasyUnlockAuthAttemptUnlockTest(const EasyUnlockAuthAttemptUnlockTest&) =
      delete;
  EasyUnlockAuthAttemptUnlockTest& operator=(
      const EasyUnlockAuthAttemptUnlockTest&) = delete;

  ~EasyUnlockAuthAttemptUnlockTest() override {}

  void SetUp() override {
    auth_attempt_ = std::make_unique<EasyUnlockAuthAttempt>(
        test_account_id1_, EasyUnlockAuthAttempt::TYPE_UNLOCK);
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

  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;
  std::unique_ptr<TestLockHandler> lock_handler_;

  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const AccountId test_account_id2_ = AccountId::FromUserEmail(kTestUser2);
};

TEST_F(EasyUnlockAuthAttemptUnlockTest, StartWhenNotLocked) {
  ASSERT_FALSE(proximity_auth::ScreenlockBridge::Get()->IsLocked());

  EXPECT_FALSE(auth_attempt_->Start());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, StartWhenAuthTypeIsPassword) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  lock_handler_->set_auth_type(
      proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, ResetBeforeFinalizeUnlock) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_.reset();

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, FinalizeUnlockFailure) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, false);

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, FinalizeSigninCalled) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  // Wrapped secret and key should be irrelevant in this case.
  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, UnlockSucceeds) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  ASSERT_EQ(TestLockHandler::STATE_UNLOCK_DONE, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, FinalizeUnlockCalledForWrongUser) {
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

class EasyUnlockAuthAttemptSigninTest : public testing::Test {
 public:
  EasyUnlockAuthAttemptSigninTest() {}

  EasyUnlockAuthAttemptSigninTest(const EasyUnlockAuthAttemptSigninTest&) =
      delete;
  EasyUnlockAuthAttemptSigninTest& operator=(
      const EasyUnlockAuthAttemptSigninTest&) = delete;

  ~EasyUnlockAuthAttemptSigninTest() override {}

  void SetUp() override {
    auth_attempt_ = std::make_unique<EasyUnlockAuthAttempt>(
        test_account_id1_, EasyUnlockAuthAttempt::TYPE_SIGNIN);
  }

  void TearDown() override {
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
    auth_attempt_.reset();
  }

 protected:
  void InitScreenLock() {
    lock_handler_ = std::make_unique<TestLockHandler>(test_account_id1_);
    lock_handler_->set_state(TestLockHandler::STATE_ATTEMPTING_SIGNIN);
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(
        lock_handler_.get());
  }

  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;
  std::unique_ptr<TestLockHandler> lock_handler_;

  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const AccountId test_account_id2_ = AccountId::FromUserEmail(kTestUser2);
};

TEST_F(EasyUnlockAuthAttemptSigninTest, StartWhenNotLocked) {
  ASSERT_FALSE(proximity_auth::ScreenlockBridge::Get()->IsLocked());

  EXPECT_FALSE(auth_attempt_->Start());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, StartWhenAuthTypeIsPassword) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  lock_handler_->set_auth_type(
      proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, ResetBeforeFinalizeSignin) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_.reset();

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeSigninWithEmtpySecret) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, "", GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeSigninWithEmtpyKey) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(), "");

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, SigninSuccess) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  lock_handler_->set_expected_secret(GetSecret());
  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_DONE, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, WrongWrappedSecret) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, "wrong_secret",
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, InvalidSessionKey) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                "invalid_key");

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeUnlockCalled) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeSigninCalledForWrongUser) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  lock_handler_->set_expected_secret(GetSecret());

  auth_attempt_->FinalizeSignin(test_account_id2_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_DONE, lock_handler_->state());
}

}  // namespace
}  // namespace ash
