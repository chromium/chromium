// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::WithArg;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

template <bool success>
void PostResponse(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(
      success
          ? PasswordChangeOutcome::
                PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME
          : PasswordChangeOutcome::
                PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

}  // namespace

class PasswordChangeSubmissionVerifierTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangeSubmissionVerifierTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PasswordChangeSubmissionVerifierTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&CreateOptimizationService));
    logs_uploader_ = std::make_unique<ModelQualityLogsUploader>(web_contents());
  }

  void TearDown() override {
    logs_uploader_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<PasswordChangeSubmissionVerifier> CreateVerifier(
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
          capture_annotated_page_content) {
    auto verifier = std::make_unique<PasswordChangeSubmissionVerifier>(
        web_contents(), logs_uploader_.get());
    verifier->set_annotated_page_callback(
        std::move(capture_annotated_page_content));
    return verifier;
  }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

 private:
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
};

TEST_F(PasswordChangeSubmissionVerifierTest, Succeeded) {
  base::HistogramTester histogram_tester;

  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  auto verifier = CreateVerifier(capture_annotated_page_content.Get());

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<true>)));
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_TRUE(completion_future.Get());
  histogram_tester.ExpectTotalCount(
      PasswordChangeSubmissionVerifier::
          kPasswordChangeVerificationTimeHistogram,
      1);
}

TEST_F(PasswordChangeSubmissionVerifierTest, Failed) {
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;
  auto verifier = CreateVerifier(capture_annotated_page_content.Get());

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(
          optimization_guide::AIPageContentResult()));
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(Invoke(&PostResponse<false>)));
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_FALSE(completion_future.Get());
}

TEST_F(PasswordChangeSubmissionVerifierTest,
       FailsCapturingAnnotatedPageContent) {
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;

  auto verifier = CreateVerifier(capture_annotated_page_content.Get());

  base::test::TestFuture<bool> completion_future;

  EXPECT_CALL(capture_annotated_page_content, Run)
      .WillOnce(base::test::RunOnceCallback<0>(std::nullopt));
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_FALSE(completion_future.Get());
}
