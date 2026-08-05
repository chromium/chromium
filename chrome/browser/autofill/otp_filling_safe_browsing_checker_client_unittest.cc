// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/otp_filling_safe_browsing_checker_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD(void,
              CancelCheck,
              (safe_browsing::SafeBrowsingDatabaseManager::Client*),
              (override));

  MOCK_METHOD(bool,
              CheckBrowseUrl,
              (const GURL&,
               const safe_browsing::SBThreatTypeSet&,
               safe_browsing::SafeBrowsingDatabaseManager::Client*,
               safe_browsing::CheckBrowseUrlType),
              (override));

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

class V5TestingDatabaseManager : public MockSafeBrowsingDatabaseManager {
 public:
  V5TestingDatabaseManager() = default;

  bool CheckBrowseUrl(const GURL& gurl,
                      const safe_browsing::SBThreatTypeSet& threat_types,
                      Client* client,
                      safe_browsing::CheckBrowseUrlType check_type) override {
    if (client) {
      v5_manager_from_client_ = client->GetV5GetHashProtocolManager();
    }
    return MockSafeBrowsingDatabaseManager::CheckBrowseUrl(gurl, threat_types,
                                                           client, check_type);
  }

  base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
  v5_manager_from_client() const {
    return v5_manager_from_client_;
  }

 private:
  ~V5TestingDatabaseManager() override = default;

  base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
      v5_manager_from_client_;
};

class OtpFillingSafeBrowsingCheckerClientTest : public testing::Test {
 public:
  OtpFillingSafeBrowsingCheckerClientTest()
      : main_frame_url_("https://main-frame.example.com"),
        frame_to_fill_url_("https://iframe.example.com"),
        database_manager_(
            base::MakeRefCounted<MockSafeBrowsingDatabaseManager>()) {}

 protected:
  static constexpr base::TimeDelta kCheckDelay =
      OtpFillingSafeBrowsingCheckerClient::kDefaultCheckDelay;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  GURL main_frame_url_;
  GURL frame_to_fill_url_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
};

// Test that when both URLs are safe synchronously, the callback is run with
// false (not malicious).
TEST_F(OtpFillingSafeBrowsingCheckerClientTest, BothUrlsSafeSynchronously) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      database_manager_, /*v5_get_hash_protocol_manager=*/nullptr, kCheckDelay,
      main_frame_url_, frame_to_fill_url_, future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kSafe, 2);
}

// Test that when the main frame URL and target frame-to-fill URL are identical,
// Safe Browsing only checks the URL once.
TEST_F(OtpFillingSafeBrowsingCheckerClientTest, IdenticalUrlsCheckedOnlyOnce) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      database_manager_, /*v5_get_hash_protocol_manager=*/nullptr, kCheckDelay,
      main_frame_url_, main_frame_url_, future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kSafe, 1);
}

// Test that when the first URL is unsafe asynchronously, the check stops and
// reports unsafe without checking the second URL.
TEST_F(OtpFillingSafeBrowsingCheckerClientTest, FirstUrlUnsafeAsynchronously) {
  base::HistogramTester histogram_tester;
  safe_browsing::SafeBrowsingDatabaseManager::Client* sb_client = nullptr;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&sb_client), Return(false)));

  // We should not check the second URL if the first is unsafe.
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .Times(0);

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      database_manager_, /*v5_get_hash_protocol_manager=*/nullptr, kCheckDelay,
      main_frame_url_, frame_to_fill_url_, future.GetCallback());

  ASSERT_TRUE(sb_client);
  // Report phishing for main frame URL
  sb_client->OnCheckBrowseUrlResult(
      main_frame_url_,
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);

  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kUnsafe, 1);
}

// Test that when the first URL is safe asynchronously, it proceeds to check the
// second URL.
TEST_F(OtpFillingSafeBrowsingCheckerClientTest,
       FirstUrlSafeAsynchronouslySecondUrlSafe) {
  base::HistogramTester histogram_tester;
  safe_browsing::SafeBrowsingDatabaseManager::Client* sb_client = nullptr;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&sb_client), Return(false)));

  EXPECT_CALL(*database_manager_, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      database_manager_, /*v5_get_hash_protocol_manager=*/nullptr, kCheckDelay,
      main_frame_url_, frame_to_fill_url_, future.GetCallback());

  ASSERT_TRUE(sb_client);
  // Report safe for main frame URL
  sb_client->OnCheckBrowseUrlResult(
      main_frame_url_, safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE);

  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kSafe, 2);
}

// Test that when a check times out, it cancels the check and reports true
// (malicious/unsafe).
TEST_F(OtpFillingSafeBrowsingCheckerClientTest, FirstUrlTimeout) {
  base::HistogramTester histogram_tester;
  safe_browsing::SafeBrowsingDatabaseManager::Client* sb_client = nullptr;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&sb_client), Return(false)));
  EXPECT_CALL(*database_manager_, CancelCheck);

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      database_manager_, /*v5_get_hash_protocol_manager=*/nullptr, kCheckDelay,
      main_frame_url_, frame_to_fill_url_, future.GetCallback());

  // Advance clock right by the delay to trigger timeout.
  task_environment_.FastForwardBy(kCheckDelay);

  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kTimeout, 1);
}

// Test that when first is safe, but second times out, it cancels check and
// reports true (malicious/unsafe).
TEST_F(OtpFillingSafeBrowsingCheckerClientTest, SecondUrlTimeout) {
  base::HistogramTester histogram_tester;
  safe_browsing::SafeBrowsingDatabaseManager::Client* sb_client = nullptr;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&sb_client), Return(false)));

  safe_browsing::SafeBrowsingDatabaseManager::Client* sb_client2 = nullptr;
  EXPECT_CALL(*database_manager_, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&sb_client2), Return(false)));

  EXPECT_CALL(*database_manager_, CancelCheck);

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      database_manager_, /*v5_get_hash_protocol_manager=*/nullptr, kCheckDelay,
      main_frame_url_, frame_to_fill_url_, future.GetCallback());

  ASSERT_TRUE(sb_client);
  // First URL is safe.
  sb_client->OnCheckBrowseUrlResult(
      main_frame_url_, safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE);

  // Now second check is running. Fast forward right past delay to trigger
  // timeout on second check.
  task_environment_.FastForwardBy(kCheckDelay);

  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectBucketCount(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kSafe, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.OtpFilling.SafeBrowsingCheckResult",
      OtpFillingSafeBrowsingCheckerClient::CheckResult::kTimeout, 1);
}

TEST_F(OtpFillingSafeBrowsingCheckerClientTest, GetV5GetHashProtocolManager) {
  scoped_refptr<V5TestingDatabaseManager> v5_db_manager =
      base::MakeRefCounted<V5TestingDatabaseManager>();

  safe_browsing::V5GetHashProtocolManager v5_protocol_manager(
      /*url_loader_factory=*/nullptr,
      safe_browsing::V4ProtocolConfig("test", false, "key", "1.0"),
      /*cache=*/nullptr);

  EXPECT_CALL(*v5_db_manager, CheckBrowseUrl(main_frame_url_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*v5_db_manager, CheckBrowseUrl(frame_to_fill_url_, _, _, _))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> future;
  auto checker_client = OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
      v5_db_manager, v5_protocol_manager.GetWeakPtr(), kCheckDelay,
      main_frame_url_, frame_to_fill_url_, future.GetCallback());

  EXPECT_EQ(checker_client->GetV5GetHashProtocolManager().get(),
            &v5_protocol_manager);

  EXPECT_FALSE(future.Get());

  EXPECT_EQ(v5_db_manager->v5_manager_from_client().get(),
            &v5_protocol_manager);
}

}  // namespace autofill
