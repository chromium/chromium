// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kFakeToken[] = "fake-token";
constexpr char kFakeEmail[] = "fake-email@example.com";
constexpr char kKnownUserPref[] = "KnownUsers";

constexpr char kTokenHandlePref[] = "PasswordTokenHandle";
constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr char kTokenHandleStatusInvalid[] = "invalid";
constexpr char kTokenHandleStatusValid[] = "valid";

}  // namespace

class TokenHandleStoreTest : public ::testing::Test {
 public:
  TokenHandleStoreTest() = default;
  ~TokenHandleStoreTest() override = default;

  void SetUp() override {
    local_state_.registry()->RegisterDictionaryPref(kTokenHandlePref);
    local_state_.registry()->RegisterListPref(kKnownUserPref);
  }

 protected:
  TestingPrefServiceSimple local_state_;
};

TEST_F(TokenHandleStoreTest, HasTokenReturnsTrueWhenTokenIsOnDisk) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool has_token = token_handle_store->HasToken(account_id);

  EXPECT_EQ(has_token, true);
}

TEST_F(TokenHandleStoreTest, HasTokenReturnsFalseWhenTokenIsEmpty) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, std::string());
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool has_token = token_handle_store->HasToken(account_id);

  EXPECT_EQ(has_token, false);
}

TEST_F(TokenHandleStoreTest, HasTokenReturnsFalseWhenNoTokenOnDisk) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool has_token = token_handle_store->HasToken(account_id);

  EXPECT_EQ(has_token, false);
}

TEST_F(TokenHandleStoreTest,
       ShouldObtainHandleReturnsTrueIfTokenExsitsAndIsInvalid) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id, kTokenHandleStatusPref,
                            kTokenHandleStatusInvalid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool should_obtain_handle =
      token_handle_store->ShouldObtainHandle(account_id);

  EXPECT_EQ(should_obtain_handle, true);
}

TEST_F(TokenHandleStoreTest, ShouldObtainHandleReturnsTrueIfTokenDoesNotExist) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool should_obtain_handle =
      token_handle_store->ShouldObtainHandle(account_id);

  EXPECT_EQ(should_obtain_handle, true);
}

TEST_F(TokenHandleStoreTest,
       ShouldObtainHandleReturnsFalseIfTokenExistsAndIsValid) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id, kTokenHandleStatusPref,
                            kTokenHandleStatusValid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool should_obtain_handle =
      token_handle_store->ShouldObtainHandle(account_id);

  EXPECT_EQ(should_obtain_handle, false);
}

}  // namespace ash
