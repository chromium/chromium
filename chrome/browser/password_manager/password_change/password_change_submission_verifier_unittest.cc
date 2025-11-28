// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::WithArg;
using PasswordChangeOutcome = ::optimization_guide::proto::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using UkmEntry = ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome;

std::unique_ptr<KeyedService> CreateOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

template <PasswordChangeOutcome outcome>
void PostResponse(
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  optimization_guide::proto::PasswordChangeResponse response;
  response.mutable_outcome_data()->set_submission_outcome(outcome);
  auto result = optimization_guide::OptimizationGuideModelExecutionResult(
      optimization_guide::AnyWrapProto(response),
      /*execution_info=*/nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                /*log_entry=*/nullptr));
}

const ukm::mojom::UkmEntry* GetUkmEntry(
    const ukm::TestAutoSetUkmRecorder& test_ukm_recorder) {
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  CHECK_EQ(ukm_entries.size(), 1u);
  return ukm_entries[0];
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
    logs_uploader_ =
        std::make_unique<ModelQualityLogsUploader>(web_contents(), GURL());
  }

  void TearDown() override {
    logs_uploader_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ModelQualityLogsUploader* logs_uploader() { return logs_uploader_.get(); }

  MockOptimizationGuideKeyedService* optimization_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

 private:
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
};

TEST_F(PasswordChangeSubmissionVerifierTest, Succeeded) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto verifier = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents(), logs_uploader());

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(
          &PostResponse<
              PasswordChangeOutcome::
                  PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME>));
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_TRUE(verifier->capturer());
  verifier->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());

  EXPECT_TRUE(completion_future.Get());
  histogram_tester.ExpectTotalCount(
      PasswordChangeSubmissionVerifier::
          kPasswordChangeVerificationTimeHistogram,
      1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeSubmissionVerifier::kSubmissionOutcomeHistogramName,
      PasswordChangeSubmissionVerifier::SubmissionOutcome::kSuccess, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(
          PasswordChangeSubmissionVerifier::SubmissionOutcome::kSuccess));
}

TEST_F(PasswordChangeSubmissionVerifierTest, Failed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto verifier = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents(), logs_uploader());

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(
          &PostResponse<
              PasswordChangeOutcome::
                  PasswordChangeSubmissionData_PasswordChangeOutcome_UNSUCCESSFUL_OUTCOME>));
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_TRUE(verifier->capturer());
  verifier->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());

  EXPECT_FALSE(completion_future.Get());

  histogram_tester.ExpectUniqueSample(
      PasswordChangeSubmissionVerifier::kSubmissionOutcomeHistogramName,
      PasswordChangeSubmissionVerifier::SubmissionOutcome::kUncategorizedError,
      1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(PasswordChangeSubmissionVerifier::SubmissionOutcome::
                           kUncategorizedError));
}

TEST_F(PasswordChangeSubmissionVerifierTest, UnknownOutcome) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto verifier = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents(), logs_uploader());

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*optimization_service(), ExecuteModel)
      .WillOnce(WithArg<3>(
          &PostResponse<
              PasswordChangeOutcome::
                  PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME>));
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_TRUE(verifier->capturer());
  verifier->capturer()->ReplyWithContent(
      optimization_guide::AIPageContentResult());

  EXPECT_TRUE(completion_future.Get());
  histogram_tester.ExpectTotalCount(
      PasswordChangeSubmissionVerifier::
          kPasswordChangeVerificationTimeHistogram,
      1);
  histogram_tester.ExpectUniqueSample(
      PasswordChangeSubmissionVerifier::kSubmissionOutcomeHistogramName,
      PasswordChangeSubmissionVerifier::SubmissionOutcome::kUnknown, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome::
          kPasswordChangeSubmissionOutcomeName,
      static_cast<int>(
          PasswordChangeSubmissionVerifier::SubmissionOutcome::kUnknown));
}

TEST_F(PasswordChangeSubmissionVerifierTest,
       FailsCapturingAnnotatedPageContent) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::MockCallback<
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>>
      capture_annotated_page_content;

  auto verifier = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents(), logs_uploader());

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*optimization_service(), ExecuteModel).Times(0);
  verifier->CheckSubmissionOutcome(completion_future.GetCallback());

  EXPECT_TRUE(verifier->capturer());
  verifier->capturer()->ReplyWithContent(
      base::unexpected("APC Capture Failed"));

  EXPECT_FALSE(completion_future.Get());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.FailedCapturingPageContent",
      password_manager::metrics_util::PasswordChangeFlowStep::
          kVerifySubmissionStep,
      1);
}

TEST_F(PasswordChangeSubmissionVerifierTest, DurationRecordedOnDestruction) {
  auto verifier = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents(), logs_uploader());

  task_environment()->FastForwardBy(base::Milliseconds(4543));

  verifier.reset();
  EXPECT_EQ(4543, logs_uploader()
                      ->GetFinalLog()
                      .password_change_submission()
                      .quality()
                      .verify_submission()
                      .request_latency_ms());
}
