// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace ash {
namespace quick_unlock {
namespace {

const char* kUmaAuthScanResult = "Fingerprint.Auth.ScanResult";
const char* kUmaAuthError = "Fingerprint.Auth.Error";

class FingerprintStorageUnitTest : public testing::Test {
 public:
  FingerprintStorageUnitTest(const FingerprintStorageUnitTest&) = delete;
  FingerprintStorageUnitTest& operator=(const FingerprintStorageUnitTest&) =
      delete;

 protected:
  FingerprintStorageUnitTest() : profile_(std::make_unique<TestingProfile>()) {}
  ~FingerprintStorageUnitTest() override {}

  // testing::Test:
  void SetUp() override {
    test_api_ = std::make_unique<TestApi>(/*override_quick_unlock=*/true);
    test_api_->EnableFingerprintByPolicy(Purpose::kAny);
  }

  void SetRecords(int records_number) {
    profile_->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord,
                                     records_number);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestApi> test_api_;
};

}  // namespace

// Provides test-only FingerprintStorage APIs.
class FingerprintStorageTestApi {
 public:
  FingerprintStorageTestApi(const FingerprintStorageTestApi&) = delete;
  FingerprintStorageTestApi& operator=(const FingerprintStorageTestApi&) =
      delete;

  // Does *not* take ownership over `fingerprint_storage`.
  explicit FingerprintStorageTestApi(FingerprintStorage* fingerprint_storage)
      : fingerprint_storage_(fingerprint_storage) {}

  bool IsFingerprintAvailable() const {
    return fingerprint_storage_->IsFingerprintAvailable(Purpose::kAny);
  }

 private:
  FingerprintStorage* fingerprint_storage_;
};

// Verifies that:
// 1. Initial unlock attempt count is zero.
// 2. Attempting unlock attempts correctly increases unlock attempt count.
// 3. Resetting unlock attempt count correctly sets attempt count to 0.
TEST_F(FingerprintStorageUnitTest, UnlockAttemptCount) {
  FingerprintStorage* fingerprint_storage =
      QuickUnlockFactory::GetForProfile(profile_.get())->fingerprint_storage();

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
  FingerprintStorage* fingerprint_storage =
      QuickUnlockFactory::GetForProfile(profile_.get())->fingerprint_storage();
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
  for (int i = 0; i < FingerprintStorage::kMaximumUnlockAttempts; ++i) {
    fingerprint_storage->AddUnlockAttempt();
  }
  EXPECT_FALSE(test_api.IsFingerprintAvailable());
  fingerprint_storage->ResetUnlockAttemptCount();
  EXPECT_TRUE(test_api.IsFingerprintAvailable());
}

TEST_F(FingerprintStorageUnitTest, TestScanResultIsSentToUma) {
  FingerprintStorage* fingerprint_storage =
      QuickUnlockFactory::GetForProfile(profile_.get())->fingerprint_storage();
  base::HistogramTester histogram_tester;
  base::flat_map<std::string, std::vector<std::string>> empty_matches;
  fingerprint_storage->OnAuthScanDone(
      device::mojom::FingerprintMessage::NewScanResult(
          device::mojom::ScanResult::SUCCESS),
      empty_matches);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kUmaAuthScanResult),
      ElementsAre(base::Bucket(
          static_cast<int>(device::mojom::ScanResult::SUCCESS), /*count=*/1)));

  EXPECT_TRUE(histogram_tester.GetAllSamples(kUmaAuthError).empty());
}

TEST_F(FingerprintStorageUnitTest, TestFingerprintErrorIsSentToUma) {
  FingerprintStorage* fingerprint_storage =
      QuickUnlockFactory::GetForProfile(profile_.get())->fingerprint_storage();
  base::HistogramTester histogram_tester;
  base::flat_map<std::string, std::vector<std::string>> empty_matches;
  fingerprint_storage->OnAuthScanDone(
      device::mojom::FingerprintMessage::NewFingerprintError(
          device::mojom::FingerprintError::UNABLE_TO_PROCESS),
      empty_matches);

  EXPECT_TRUE(histogram_tester.GetAllSamples(kUmaAuthScanResult).empty());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kUmaAuthError),
      ElementsAre(base::Bucket(
          static_cast<int>(device::mojom::FingerprintError::UNABLE_TO_PROCESS),
          /*count=*/1)));
}

}  // namespace quick_unlock
}  // namespace ash
