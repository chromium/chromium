// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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

std::unique_ptr<KeyedService> BuildTestOptimizationGuideKeyedService(
    content::BrowserContext* browser_context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
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
    profile_builder.AddTestingFactory(
        OptimizationGuideKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestOptimizationGuideKeyedService));
    profile_ = profile_builder.Build();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();
    mock_history_service_ =
        static_cast<MockHistoryService*>(HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(
                profile_.get()));
    service_ = std::make_unique<GeminiAntiscamProtectionService>(
        mock_optimization_guide_keyed_service_, mock_history_service_);
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

  void ExpectExecuteModel(
      optimization_guide::OptimizationGuideModelExecutionResult result) {
    EXPECT_CALL(*mock_optimization_guide_keyed_service_,
                ExecuteModel(testing::_, testing::_, testing::_, testing::_))
        .WillOnce([result = std::move(result)](
                      optimization_guide::ModelBasedCapabilityKey feature,
                      const google::protobuf::MessageLite& request_metadata,
                      const optimization_guide::ModelExecutionOptions& options,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultCallback
                              callback) mutable {
          std::move(callback).Run(std::move(result), nullptr);
        });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<GeminiAntiscamProtectionService> service_;
  raw_ptr<MockHistoryService> mock_history_service_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
};

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_NotForceRequest) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              ExecuteModel(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_DidMatchAllowlist) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              ExecuteModel(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/true,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_UrlDoesNotMatchLastCommittedUrl) {
  EXPECT_CALL(*mock_history_service_,
              GetVisibleVisitCountToHost(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              ExecuteModel(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      GURL("https://example.com"), ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_HistoryCheckFails) {
  base::HistogramTester histogram_tester;
  web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL();
  ExpectGetVisibleVisitCountToHost(
      /*count=*/0, /*success=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              ExecuteModel(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*expected_count=*/0);
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_VisitedBefore) {
  base::HistogramTester histogram_tester;
  ExpectGetVisibleVisitCountToHost(
      /*count=*/2, /*success=*/true,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              ExecuteModel(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(
    GeminiAntiscamProtectionServiceTest,
    TestMaybeStartAntiscamProtection_EmptyContentCategory) {
  base::HistogramTester histogram_tester;
  ExpectGetVisibleVisitCountToHost(
      /*count=*/0, /*success=*/true,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL());

  auto response = optimization_guide::proto::GeminiAntiscamProtectionResponse();
  // response.set_content_category("phishing");
  response.set_scam_score(0.5);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  ExpectExecuteModel(std::move(result));
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.Empty.ScamScore",
      /*sample=*/50, /*expected_bucket_count=*/1);
}

TEST_F(
    GeminiAntiscamProtectionServiceTest,
    TestMaybeStartAntiscamProtection_ContentCategoryNoMatchFound) {
  base::HistogramTester histogram_tester;
  ExpectGetVisibleVisitCountToHost(
      /*count=*/0, /*success=*/true,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL());

  auto response = optimization_guide::proto::GeminiAntiscamProtectionResponse();
  response.set_content_category("no_match_found");
  response.set_scam_score(0.5);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  ExpectExecuteModel(std::move(result));
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.NoMatchFound.ScamScore",
      /*sample=*/50, /*expected_bucket_count=*/1);
}

TEST_F(GeminiAntiscamProtectionServiceTest,
       TestMaybeStartAntiscamProtection_PhishingContentCategory) {
  base::HistogramTester histogram_tester;
  ExpectGetVisibleVisitCountToHost(
      /*count=*/0, /*success=*/true,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL());

  auto response = optimization_guide::proto::GeminiAntiscamProtectionResponse();
  response.set_content_category("phishing");
  response.set_scam_score(0.5);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  ExpectExecuteModel(std::move(result));
  service_->MaybeStartAntiscamProtection(
      GeminiAntiscamProtectionService::BuildGeminiAntiscamProtectionMetadata(
          web_contents_.get()),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/false,
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(), "page text");
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.IsHistoryServiceResultValid",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.ShouldSkipDueToPreviousVisit",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.GeminiAntiscamProtection.Phishing.ScamScore",
      /*sample=*/50, /*expected_bucket_count=*/1);
}

// TODO(crbug.com/467358093): Add tests, where autofill fields are present.

}  // namespace safe_browsing
