// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"

#include <memory>
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class FingerprintStorageUnitTest : public testing::Test {
 protected:
  FingerprintStorageUnitTest() : profile_(std::make_unique<TestingProfile>()) {}
  ~FingerprintStorageUnitTest() override {}

  // testing::Test:
  void SetUp() override { quick_unlock::EnabledForTesting(true); }

  void TearDown() override { quick_unlock::EnabledForTesting(false); }

  void SetRecords(int records_number) {
    profile_->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord,
                                     records_number);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintStorageUnitTest);
};

}  // namespace

// Provides test-only FingerprintStorage APIs.
class FingerprintStorageTestApi {
 public:
  // Does *not* take ownership over |fingerprint_storage|.
  explicit FingerprintStorageTestApi(
      quick_unlock::FingerprintStorage* fingerprint_storage)
      : fingerprint_storage_(fingerprint_storage) {}

  bool IsFingerprintAvailable() const {
    return fingerprint_storage_->IsFingerprintAvailable();
  }

 private:
  quick_unlock::FingerprintStorage* fingerprint_storage_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintStorageTestApi);
};

// Verifies that:
// 1. Initial unlock attempt count is zero.
// 2. Attempting unlock attempts correctly increases unlock attempt count.
// 3. Resetting unlock attempt count correctly sets attempt count to 0.
TEST_F(FingerprintStorageUnitTest, UnlockAttemptCount) {
  quick_unlock::FingerprintStorage* fingerprint_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->fingerprint_storage();

  EXPECT_EQ(0, fingerprint_storage->unlock_attempt_count());

  fingerprint_storage->AddUnlockAttempt();
  fingerprint_storage->AddUnlockAttempt();
  fingerprint_storage->AddUnlockAttempt();
  EXPECT_EQ(3, fingerprint_storage->unlock_attempt_count());

  fingerprint_storage->ResetUnlockAttemptCount();
  EXPECT_EQ(0, fingerprint_storage->unlock_attempt_count());
}

// Verifies that authentication is not available when
// 1. No fingerprint records registered.
// 2. Too many authentication attempts.
TEST_F(FingerprintStorageUnitTest, AuthenticationUnAvailable) {
  quick_unlock::FingerprintStorage* fingerprint_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->fingerprint_storage();
  FingerprintStorageTestApi test_api(fingerprint_storage);

  EXPECT_FALSE(fingerprint_storage->HasRecord());
  SetRecords(1);
  EXPECT_TRUE(fingerprint_storage->HasRecord());
  EXPECT_EQ(0, fingerprint_storage->unlock_attempt_count());

  EXPECT_TRUE(test_api.IsFingerprintAvailable());

  // No fingerprint records registered makes fingerprint authentication
  // unavailable.
  SetRecords(0);
  EXPECT_FALSE(test_api.IsFingerprintAvailable());
  SetRecords(1);
  EXPECT_TRUE(test_api.IsFingerprintAvailable());

  // Too many authentication attempts make fingerprint authentication
  // unavailable.
  for (int i = 0; i < quick_unlock::FingerprintStorage::kMaximumUnlockAttempts;
       ++i) {
    fingerprint_storage->AddUnlockAttempt();
  }
  EXPECT_FALSE(test_api.IsFingerprintAvailable());
  fingerprint_storage->ResetUnlockAttemptCount();
  EXPECT_TRUE(test_api.IsFingerprintAvailable());
}

}  // namespace chromeos
