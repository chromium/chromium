// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_safe_browsing_checker_client.h"

#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/safety_hub/mock_safe_browsing_database_manager.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

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

}  // namespace

class AutoPictureInPictureSafeBrowsingCheckerClientTest
    : public ::testing::Test {
 public:
  AutoPictureInPictureSafeBrowsingCheckerClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();

    mock_database_manager_ = new MockSafeBrowsingDatabaseManager();

    safe_browsing_check_client_ =
        std::make_unique<AutoPictureInPictureSafeBrowsingCheckerClient>(
            mock_database_manager(), /*v5_get_hash_protocol_manager=*/nullptr,
            kSafeBrowsingCheckDelay, report_url_safety_cb().Get());
  }

  void TearDown() override { mock_database_manager_.reset(); }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
  }

  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

  base::MockRepeatingCallback<void(bool)>& report_url_safety_cb() {
    return report_url_safety_cb_;
  }

  AutoPictureInPictureSafeBrowsingCheckerClient* safe_browsing_check_client() {
    return safe_browsing_check_client_.get();
  }

 protected:
  const GURL kPageUrl = GURL("https://example1.com");
  const base::TimeDelta kSafeBrowsingCheckDelay = base::Milliseconds(500);
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  std::unique_ptr<AutoPictureInPictureSafeBrowsingCheckerClient>
      safe_browsing_check_client_;
  base::MockRepeatingCallback<void(bool)> report_url_safety_cb_;
};

#if DCHECK_IS_ON()
TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest, InvalidDelay) {
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore =
          std::make_unique<AutoPictureInPictureSafeBrowsingCheckerClient>(
              mock_database_manager(), /*v5_get_hash_protocol_manager=*/nullptr,
              base::Milliseconds(1), report_url_safety_cb().Get()),
      "");
}
#endif  // DCHECK_IS_ON()

TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest,
       CheckCanceledOnNewRequestWhileTimerRunning) {
  safe_browsing_check_client()->CheckUrlSafety(kPageUrl);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());
  EXPECT_CALL(report_url_safety_cb(), Run(false));
  safe_browsing_check_client()->CheckUrlSafety(kPageUrl);
  EXPECT_TRUE(mock_database_manager()->HasCalledCancelCheck());
}

TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest,
       CheckCanceledOnCheckBlocklistTimeout) {
  base::HistogramTester histogram_tester;
  safe_browsing_check_client()->CheckUrlSafety(kPageUrl);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());

  EXPECT_CALL(report_url_safety_cb(), Run(false));
  safe_browsing_check_client()->OnCheckBlocklistTimeout();
  EXPECT_TRUE(mock_database_manager()->HasCalledCancelCheck());
  histogram_tester.ExpectBucketCount(
      "Media.AutoPictureInPicture.SafeBrowsingCheckResult",
      AutoPictureInPictureSafeBrowsingCheckerClient::CheckResult::kTimeout, 1);
}

TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest,
       GetV5GetHashProtocolManager) {
  scoped_refptr<V5TestingDatabaseManager> v5_db_manager =
      base::MakeRefCounted<V5TestingDatabaseManager>();

  safe_browsing::V5GetHashProtocolManager v5_protocol_manager(
      /*url_loader_factory=*/nullptr,
      safe_browsing::V4ProtocolConfig("test", false, "key", "1.0"),
      /*cache=*/nullptr);

  AutoPictureInPictureSafeBrowsingCheckerClient client(
      v5_db_manager, v5_protocol_manager.GetWeakPtr(), kSafeBrowsingCheckDelay,
      report_url_safety_cb().Get());

  EXPECT_EQ(client.GetV5GetHashProtocolManager().get(), &v5_protocol_manager);

  client.CheckUrlSafety(kPageUrl);

  EXPECT_EQ(v5_db_manager->v5_manager_from_client().get(),
            &v5_protocol_manager);
}

TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest,
       CheckPerformedSynchronously) {
  base::HistogramTester histogram_tester;
  MockSafeBrowsingDatabaseManager::ScopedSimulateSafeSynchronousResponse
      synchronous_response_scope =
          mock_database_manager()->CreateSimulateSafeSynchronousResponseScope(
              /*simulate_safe_synchronous_response=*/true);
  EXPECT_TRUE(mock_database_manager()->SimulateSafeSynchronousResponse());

  EXPECT_CALL(report_url_safety_cb(), Run(true));
  safe_browsing_check_client()->CheckUrlSafety(kPageUrl);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());
  histogram_tester.ExpectBucketCount(
      "Media.AutoPictureInPicture.SafeBrowsingCheckResult",
      AutoPictureInPictureSafeBrowsingCheckerClient::CheckResult::kSafe, 1);
}

TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest,
       CheckerReportsURLAsSafe) {
  base::HistogramTester histogram_tester;
  mock_database_manager()->SetThreatTypeForUrl(
      kPageUrl, safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE);
  safe_browsing_check_client()->CheckUrlSafety(kPageUrl);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());

  EXPECT_CALL(report_url_safety_cb(), Run(true));
  FastForwardBy(kSafeBrowsingCheckDelay);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());
  histogram_tester.ExpectBucketCount(
      "Media.AutoPictureInPicture.SafeBrowsingCheckResult",
      AutoPictureInPictureSafeBrowsingCheckerClient::CheckResult::kSafe, 1);
}

TEST_F(AutoPictureInPictureSafeBrowsingCheckerClientTest,
       CheckerReportsURLAsNotSafe) {
  base::HistogramTester histogram_tester;
  mock_database_manager()->SetThreatTypeForUrl(
      kPageUrl, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  safe_browsing_check_client()->CheckUrlSafety(kPageUrl);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());

  EXPECT_CALL(report_url_safety_cb(), Run(false));
  FastForwardBy(kSafeBrowsingCheckDelay);
  EXPECT_FALSE(mock_database_manager()->HasCalledCancelCheck());
  histogram_tester.ExpectBucketCount(
      "Media.AutoPictureInPicture.SafeBrowsingCheckResult",
      AutoPictureInPictureSafeBrowsingCheckerClient::CheckResult::kUnsafe, 1);
}
