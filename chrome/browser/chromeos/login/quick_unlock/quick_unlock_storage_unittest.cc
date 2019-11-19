// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/chromeos/login/quick_unlock/auth_token.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using AuthToken = quick_unlock::AuthToken;
using QuickUnlockStorage = quick_unlock::QuickUnlockStorage;

namespace {

void SetConfirmationFrequency(
    PrefService* pref_service,
    quick_unlock::PasswordConfirmationFrequency frequency) {
  pref_service->SetInteger(prefs::kQuickUnlockTimeout,
                           static_cast<int>(frequency));
}

base::TimeDelta GetExpirationTime(PrefService* pref_service) {
  int frequency = pref_service->GetInteger(prefs::kQuickUnlockTimeout);
  return quick_unlock::PasswordConfirmationFrequencyToTimeDelta(
      static_cast<quick_unlock::PasswordConfirmationFrequency>(frequency));
}

}  // namespace

class QuickUnlockStorageUnitTest : public testing::Test {
 protected:
  QuickUnlockStorageUnitTest() : profile_(std::make_unique<TestingProfile>()) {}
  ~QuickUnlockStorageUnitTest() override {}

  // testing::Test:
  void SetUp() override { quick_unlock::EnabledForTesting(true); }
  void TearDown() override { quick_unlock::EnabledForTesting(false); }

  void ExpireAuthToken() {
    quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
        ->auth_token_->Reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockStorageUnitTest);
};

// Provides test-only QuickUnlockStorage APIs.
class QuickUnlockStorageTestApi {
 public:
  // Does *not* take ownership over |quick_unlock_storage|.
  explicit QuickUnlockStorageTestApi(QuickUnlockStorage* quick_unlock_storage)
      : quick_unlock_storage_(quick_unlock_storage) {}

  // Reduces the amount of strong auth time available by |time_delta|.
  void ReduceRemainingStrongAuthTimeBy(const base::TimeDelta& time_delta) {
    quick_unlock_storage_->last_strong_auth_ -= time_delta;
  }

  bool HasStrongAuthInfo() {
    return !quick_unlock_storage_->last_strong_auth_.is_null();
  }

 private:
  QuickUnlockStorage* quick_unlock_storage_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockStorageTestApi);
};

// Verifies that marking the strong auth makes TimeSinceLastStrongAuth a > zero
// value.
TEST_F(QuickUnlockStorageUnitTest,
       TimeSinceLastStrongAuthReturnsPositiveValue) {
  QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get());
  PrefService* pref_service = profile_->GetPrefs();
  QuickUnlockStorageTestApi test_api(quick_unlock_storage);

  EXPECT_FALSE(test_api.HasStrongAuthInfo());

  quick_unlock_storage->MarkStrongAuth();

  EXPECT_TRUE(test_api.HasStrongAuthInfo());
  base::TimeDelta expiration_time = GetExpirationTime(pref_service);
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time);

  EXPECT_TRUE(quick_unlock_storage->TimeSinceLastStrongAuth() >=
              (expiration_time / 2));
}

// Verifies that by altering the password confirmation preference, the
// quick unlock storage will request password reconfirmation as expected.
TEST_F(QuickUnlockStorageUnitTest,
       QuickUnlockPasswordConfirmationFrequencyPreference) {
  QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get());
  PrefService* pref_service = profile_->GetPrefs();
  QuickUnlockStorageTestApi test_api(quick_unlock_storage);

  // The default is two days, so verify moving the last strong auth time back 24
  // hours(half of the expiration time) should not request strong auth.
  quick_unlock_storage->MarkStrongAuth();
  base::TimeDelta expiration_time = GetExpirationTime(pref_service);
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time / 2);
  EXPECT_TRUE(quick_unlock_storage->HasStrongAuth());

  // Verify moving the last strong auth time back another half of the expiration
  // time should request strong auth.
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time / 2);
  EXPECT_FALSE(quick_unlock_storage->HasStrongAuth());

  // Verify that by changing the frequency of required password confirmation to
  // six hours, moving the last strong auth interval back by 3 hours(half) will
  // not trigger a request for strong auth, but moving it by an additional 3
  // hours will.
  quick_unlock_storage->MarkStrongAuth();
  SetConfirmationFrequency(
      pref_service, quick_unlock::PasswordConfirmationFrequency::SIX_HOURS);
  expiration_time = GetExpirationTime(pref_service);
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time / 2);
  EXPECT_TRUE(quick_unlock_storage->HasStrongAuth());
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time / 2);
  EXPECT_FALSE(quick_unlock_storage->HasStrongAuth());

  // A valid strong auth becomes invalid if the confirmation frequency is
  // shortened to less than the expiration time.
  quick_unlock_storage->MarkStrongAuth();
  SetConfirmationFrequency(
      pref_service, quick_unlock::PasswordConfirmationFrequency::TWELVE_HOURS);
  expiration_time = GetExpirationTime(pref_service);
  EXPECT_TRUE(quick_unlock_storage->HasStrongAuth());
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time / 2);
  EXPECT_TRUE(quick_unlock_storage->HasStrongAuth());
  SetConfirmationFrequency(
      pref_service, quick_unlock::PasswordConfirmationFrequency::SIX_HOURS);
  EXPECT_FALSE(quick_unlock_storage->HasStrongAuth());

  // An expired strong auth becomes usable if the confirmation frequency gets
  // extended past the expiration time.
  quick_unlock_storage->MarkStrongAuth();
  SetConfirmationFrequency(
      pref_service, quick_unlock::PasswordConfirmationFrequency::SIX_HOURS);
  expiration_time = GetExpirationTime(pref_service);
  EXPECT_TRUE(quick_unlock_storage->HasStrongAuth());
  test_api.ReduceRemainingStrongAuthTimeBy(expiration_time);
  EXPECT_FALSE(quick_unlock_storage->HasStrongAuth());
  SetConfirmationFrequency(
      pref_service, quick_unlock::PasswordConfirmationFrequency::TWELVE_HOURS);
  EXPECT_TRUE(quick_unlock_storage->HasStrongAuth());
}

TEST_F(QuickUnlockStorageUnitTest, AuthToken) {
  QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(quick_unlock_storage->GetAuthToken());

  chromeos::UserContext context;
  std::string auth_token = quick_unlock_storage->CreateAuthToken(context);
  EXPECT_NE(std::string(), auth_token);
  EXPECT_TRUE(quick_unlock_storage->GetAuthToken());
  EXPECT_EQ(auth_token, quick_unlock_storage->GetAuthToken()->Identifier());

  ExpireAuthToken();
  EXPECT_FALSE(quick_unlock_storage->GetAuthToken());
}

}  // namespace chromeos
