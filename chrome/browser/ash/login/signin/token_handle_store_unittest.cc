// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/values_util.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
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
constexpr char kTokenHandleLastCheckedPref[] = "TokenHandleLastChecked";
constexpr char kTokenHandleStatusInvalid[] = "invalid";
constexpr char kTokenHandleStatusValid[] = "valid";

constexpr base::TimeDelta kCacheStatusTime = base::Hours(1);

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

TEST_F(TokenHandleStoreTest, IsRecentlyCheckedReturnsFalseIfNeverChecked) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));

  bool is_recently_checked = token_handle_store->IsRecentlyChecked(account_id);

  EXPECT_EQ(is_recently_checked, false);
}

TEST_F(TokenHandleStoreTest,
       IsRecentlyCheckedReturnsTrueIfWithinCacheStatusTime) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  base::ScopedMockClockOverride mock_clock;
  known_user->SetPath(account_id, kTokenHandleLastCheckedPref,
                      base::TimeToValue(base::Time::Now()));
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));
  mock_clock.Advance(kCacheStatusTime - base::Minutes(1));

  bool is_recently_checked = token_handle_store->IsRecentlyChecked(account_id);

  EXPECT_EQ(is_recently_checked, true);
}

TEST_F(TokenHandleStoreTest,
       IsRecentlyCheckedReturnsFalseIfNotWithinCacheStatusTime) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  base::ScopedMockClockOverride mock_clock;
  known_user->SetPath(account_id, kTokenHandleLastCheckedPref,
                      base::TimeToValue(base::Time::Now()));
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(known_user));
  mock_clock.Advance(kCacheStatusTime * 2);

  bool is_recently_checked = token_handle_store->IsRecentlyChecked(account_id);

  EXPECT_EQ(is_recently_checked, false);
}

TEST_F(TokenHandleStoreTest,
       TokenHandleIsStoredWithCorrectStatusAndLastCheckedTime) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  base::ScopedMockClockOverride mock_clock;
  base::Time previous_last_checked = base::Time::Now();
  injected_known_user->SetPath(account_id, kTokenHandleLastCheckedPref,
                               base::TimeToValue(previous_last_checked));
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      std::make_unique<TokenHandleStoreImpl>(std::move(injected_known_user));
  mock_clock.Advance(delta);

  token_handle_store->StoreTokenHandle(account_id, kFakeToken);

  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  EXPECT_EQ(kFakeToken,
            *known_user->FindStringPath(account_id, kTokenHandlePref));
  EXPECT_EQ(kTokenHandleStatusValid,
            *known_user->FindStringPath(account_id, kTokenHandleStatusPref));
  base::Time expected_last_checked_time = previous_last_checked + delta;
  base::Time actual_last_checked_time =
      base::ValueToTime(
          *known_user->FindPath(account_id, kTokenHandleLastCheckedPref))
          .value();
  EXPECT_NEAR(expected_last_checked_time.InMillisecondsSinceUnixEpoch(),
              actual_last_checked_time.InMillisecondsSinceUnixEpoch(),
              (delta / 2).InMilliseconds());
}

}  // namespace ash
