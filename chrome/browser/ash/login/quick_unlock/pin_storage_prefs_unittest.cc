// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_unlock {
namespace {

class PinStoragePrefsUnitTest : public testing::Test {
 public:
  PinStoragePrefsUnitTest(const PinStoragePrefsUnitTest&) = delete;
  PinStoragePrefsUnitTest& operator=(const PinStoragePrefsUnitTest&) = delete;

 protected:
  PinStoragePrefsUnitTest() : profile_(std::make_unique<TestingProfile>()) {}
  ~PinStoragePrefsUnitTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_api_ = std::make_unique<TestApi>(/*override_quick_unlock=*/true);
    test_api_->EnablePinByPolicy(Purpose::kAny);
    UserDataAuthClient::InitializeFake();
  }

  PinStoragePrefs* PinStoragePrefs() const {
    return QuickUnlockFactory::GetForProfile(profile_.get())
        ->pin_storage_prefs();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestApi> test_api_;
};

}  // namespace

// Provides test-only PinStoragePrefs APIs.
class PinStoragePrefsTestApi {
 public:
  // Does *not* take ownership over `pin_storage`.
  explicit PinStoragePrefsTestApi(PinStoragePrefs* pin_storage)
      : pin_storage_(pin_storage) {}

  PinStoragePrefsTestApi(const PinStoragePrefsTestApi&) = delete;
  PinStoragePrefsTestApi& operator=(const PinStoragePrefsTestApi&) = delete;

  std::string PinSalt() const { return pin_storage_->PinSalt(); }

  std::string PinSecret() const { return pin_storage_->PinSecret(); }

  bool IsPinAuthenticationAvailable() const {
    return pin_storage_->IsPinAuthenticationAvailable(Purpose::kAny);
  }
  bool TryAuthenticatePin(const std::string& secret, Key::KeyType key_type) {
    return pin_storage_->TryAuthenticatePin(Key(key_type, "" /*salt*/, secret),
                                            Purpose::kAny);
  }

 private:
  raw_ptr<PinStoragePrefs> pin_storage_;
};

// Verifies that:
// 1. Prefs are initially empty
// 2. Setting a PIN will update the pref system.
// 3. Removing a PIN clears prefs.
TEST_F(PinStoragePrefsUnitTest, PinStorageWritesToPrefs) {
  PrefService* prefs = profile_->GetPrefs();

  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSalt));
  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSecret));

  PinStoragePrefsTestApi pin_storage_test(PinStoragePrefs());

  PinStoragePrefs()->SetPin("1111");
  EXPECT_TRUE(PinStoragePrefs()->IsPinSet());
  EXPECT_EQ(pin_storage_test.PinSalt(),
            prefs->GetString(prefs::kQuickUnlockPinSalt));
  EXPECT_EQ(pin_storage_test.PinSecret(),
            prefs->GetString(prefs::kQuickUnlockPinSecret));
  EXPECT_NE("", pin_storage_test.PinSalt());
  EXPECT_NE("", pin_storage_test.PinSecret());

  PinStoragePrefs()->RemovePin();
  EXPECT_FALSE(PinStoragePrefs()->IsPinSet());
  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSalt));
  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSecret));
}

// Verifies that:
// 1. Initial unlock attempt count is zero.
// 2. Attempting unlock attempts correctly increases unlock attempt count.
// 3. Resetting unlock attempt count correctly sets attempt count to 0.
TEST_F(PinStoragePrefsUnitTest, UnlockAttemptCount) {
  EXPECT_EQ(0, PinStoragePrefs()->unlock_attempt_count());

  PinStoragePrefs()->AddUnlockAttempt();
  PinStoragePrefs()->AddUnlockAttempt();
  PinStoragePrefs()->AddUnlockAttempt();
  EXPECT_EQ(3, PinStoragePrefs()->unlock_attempt_count());

  PinStoragePrefs()->ResetUnlockAttemptCount();
  EXPECT_EQ(0, PinStoragePrefs()->unlock_attempt_count());
}

// Verifies that the correct pin can be used to authenticate.
TEST_F(PinStoragePrefsUnitTest, AuthenticationSucceedsWithRightPin) {
  PinStoragePrefsTestApi pin_storage_test(PinStoragePrefs());

  PinStoragePrefs()->SetPin("1111");

  EXPECT_TRUE(pin_storage_test.TryAuthenticatePin(
      "1111", Key::KEY_TYPE_PASSWORD_PLAIN));
}

// Verifies that the correct pin will fail to authenticate if too many
// authentication attempts have been made.
TEST_F(PinStoragePrefsUnitTest, AuthenticationFailsFromTooManyAttempts) {
  PinStoragePrefsTestApi pin_storage_test(PinStoragePrefs());

  PinStoragePrefs()->SetPin("1111");

  // Use up all of the authentication attempts so authentication fails.
  EXPECT_TRUE(pin_storage_test.IsPinAuthenticationAvailable());
  for (int i = 0; i < PinStoragePrefs::kMaximumUnlockAttempts; ++i) {
    EXPECT_FALSE(pin_storage_test.TryAuthenticatePin(
        "foobar", Key::KEY_TYPE_PASSWORD_PLAIN));
  }

  // We used up all of the attempts, so entering the right PIN will still fail.
  EXPECT_FALSE(pin_storage_test.IsPinAuthenticationAvailable());
  EXPECT_FALSE(pin_storage_test.TryAuthenticatePin(
      "1111", Key::KEY_TYPE_PASSWORD_PLAIN));
}

// Verifies that hashed pin can be used to authenticate.
TEST_F(PinStoragePrefsUnitTest, AuthenticationWithHashedPin) {
  PinStoragePrefsTestApi pin_storage_test(PinStoragePrefs());

  PinStoragePrefs()->SetPin("1111");
  std::string hashed_pin = pin_storage_test.PinSecret();

  // Verify that hashed pin can be used to authenticate.
  EXPECT_TRUE(pin_storage_test.TryAuthenticatePin(
      hashed_pin, Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234));

  // Use key type of Key::KEY_TYPE_PASSWORD_PLAIN should fail the
  // authentication.
  EXPECT_FALSE(pin_storage_test.TryAuthenticatePin(
      hashed_pin, Key::KEY_TYPE_PASSWORD_PLAIN));
}

}  // namespace quick_unlock
}  // namespace ash
