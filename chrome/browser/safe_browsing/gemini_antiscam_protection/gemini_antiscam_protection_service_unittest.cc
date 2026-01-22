// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "base/test/task_environment.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(
      base::CancelableTaskTracker::TaskId,
      GetVisibleVisitCountToHost,
      (const GURL& url,
       history::HistoryService::GetVisibleVisitCountToHostCallback callback,
       base::CancelableTaskTracker* tracker),
      (override));
};

std::unique_ptr<KeyedService> BuildMockHistoryService(
    content::BrowserContext* context) {
  return std::make_unique<MockHistoryService>();
}

}  // namespace

namespace safe_browsing {

class GeminiAntiscamProtectionServiceTest : public testing::Test {
 public:
  GeminiAntiscamProtectionServiceTest() = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockHistoryService));
    profile_ = profile_builder.Build();
    mock_history_service_ =
        static_cast<MockHistoryService*>(HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
    service_ = std::make_unique<GeminiAntiscamProtectionService>(
        nullptr, mock_history_service_);
  }

  void TearDown() override { service_.reset(); }

  void ExpectGetVisibleVisitCountToHost(
      int count,
      bool success = true,
      const GURL& url = GURL("https://example.com")) {
    EXPECT_CALL(*mock_history_service_,
                GetVisibleVisitCountToHost(url, testing::_, testing::_))
        .WillOnce(
            [count, success](
                const GURL& url,
                history::HistoryService::GetVisibleVisitCountToHostCallback
                    callback,
                base::CancelableTaskTracker* tracker) {
              history::VisibleVisitCountToHostResult result;
              result.count = count;
              result.success = success;
              std::move(callback).Run(result);
              return base::CancelableTaskTracker::TaskId();
            });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<GeminiAntiscamProtectionService> service_;
  raw_ptr<MockHistoryService> mock_history_service_;
};

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_NotForceRequest) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GURL("https://example.com"),
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_DidMatchAllowlist) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/true,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_ShouldShowScamWarning) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/true,
      /*is_phishing=*/false, "page text");
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_IsPhishing) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/true, "page text");
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_HistoryCheckFails) {
  GURL url("https://example.com");
  ExpectGetVisibleVisitCountToHost(/*count=*/0, /*success=*/false, url);
  service_->MaybeStartAntiscamProtection(
      url, ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
  task_environment_.RunUntilIdle();
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_VisitedBefore) {
  GURL url("https://example.com");
  ExpectGetVisibleVisitCountToHost(/*count=*/2, /*success=*/true, url);
  service_->MaybeStartAntiscamProtection(
      url, ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
  task_environment_.RunUntilIdle();
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_StartAntiscamProtection) {
  GURL url("https://example.com");
  ExpectGetVisibleVisitCountToHost(/*count=*/0, /*success=*/true, url);
  service_->MaybeStartAntiscamProtection(
      url, ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      /*should_show_scam_warning=*/false,
      /*is_phishing=*/false, "page text");
  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
