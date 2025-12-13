// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PasswordChangeErrorCase = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeErrorCase;
using SubmissionOutcome = PasswordChangeSubmissionVerifier::SubmissionOutcome;

constexpr optimization_guide::proto::PasswordChangeRequest::FlowStep
    kSubmitVerification = optimization_guide::proto::PasswordChangeRequest::
        FlowStep::PasswordChangeRequest_FlowStep_VERIFY_SUBMISSION_STEP;

void LogSubmissionOutcome(SubmissionOutcome outcome, ukm::SourceId ukm_id) {
  base::UmaHistogramEnumeration(
      PasswordChangeSubmissionVerifier::kSubmissionOutcomeHistogramName,
      outcome);
  ukm::builders::PasswordManager_PasswordChangeSubmissionOutcome(ukm_id)
      .SetPasswordChangeSubmissionOutcome(static_cast<int>(outcome))
      .Record(ukm::UkmRecorder::Get());
}

void RecordOutcomeMetrics(
    optimization_guide::proto ::PasswordChangeSubmissionData submission_data,
    ukm::SourceId ukm_id) {
  PasswordChangeOutcome outcome = submission_data.submission_outcome();
  if (outcome ==
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME) {
    LogSubmissionOutcome(SubmissionOutcome::kSuccess, ukm_id);
    return;
  }
  if (outcome ==
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    LogSubmissionOutcome(SubmissionOutcome::kUnknown, ukm_id);
    return;
  }
  for (auto error_case_enum : submission_data.error_case()) {
    PasswordChangeErrorCase error_case = optimization_guide::proto ::
        PasswordChangeSubmissionData_PasswordChangeErrorCase(error_case_enum);
    switch (error_case) {
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_OLD_PASSWORD_INCORRECT:
        LogSubmissionOutcome(SubmissionOutcome::kErrorOldPasswordIncorrect,
                             ukm_id);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_PASSWORDS_DO_NOT_MATCH:
        LogSubmissionOutcome(SubmissionOutcome::kErrorOldPasswordDoNotMatch,
                             ukm_id);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_NEW_PASSWORD_INCORRECT:
        LogSubmissionOutcome(SubmissionOutcome::kErrorNewPasswordIncorrect,
                             ukm_id);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_PAGE_ERROR:
        LogSubmissionOutcome(SubmissionOutcome::kPageError, ukm_id);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_UNKNOWN_CASE:
      default:
        LogSubmissionOutcome(SubmissionOutcome::kUncategorizedError, ukm_id);
        break;
    }
  }

  if (!submission_data.error_case_size()) {
    LogSubmissionOutcome(SubmissionOutcome::kUncategorizedError, ukm_id);
  }
}

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  auto options = optimization_guide::ActionableAIPageContentOptions(
      /*on_critical_path =*/true);
  return options;
}

OptimizationGuideKeyedService* GetOptimizationService(
    content::WebContents* web_contents) {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

}  // namespace

char PasswordChangeSubmissionVerifier::kPasswordChangeVerificationTimeHistogram
    [] = "PasswordManager.PasswordChangeVerificationTime";
char PasswordChangeSubmissionVerifier::kSubmissionOutcomeHistogramName[] =
    "PasswordManager.PasswordChangeSubmissionOutcome";

PasswordChangeSubmissionVerifier::PasswordChangeSubmissionVerifier(
    content::WebContents* web_contents,
    ModelQualityLogsUploader* logs_uploader)
    : creation_time_(base::Time::Now()),
      web_contents_(web_contents),
      logs_uploader_(logs_uploader) {}

PasswordChangeSubmissionVerifier::~PasswordChangeSubmissionVerifier() {
  logs_uploader_->SetStepDuration(kSubmitVerification,
                                  base::Time::Now() - creation_time_);
}

void PasswordChangeSubmissionVerifier::CheckSubmissionOutcome(
    FormSubmissionResultCallback callback) {
  CHECK(web_contents_);
  callback_ = std::move(callback);

  capturer_ = std::make_unique<AnnotatedPageContentCapturer>(
      web_contents_, GetAIPageContentOptions(),
      base::BindOnce(
          &PasswordChangeSubmissionVerifier::CheckSubmissionSuccessful,
          weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeSubmissionVerifier::CheckSubmissionSuccessful(
    optimization_guide::AIPageContentResultOrError page_content) {
  CHECK(callback_);
  CHECK(web_contents_);

  if (!page_content.has_value()) {
    LogPageContentCaptureFailure(
        password_manager::metrics_util::PasswordChangeFlowStep::
            kVerifySubmissionStep);
    std::move(callback_).Run(false);
    return;
  }

  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(kSubmitVerification);
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(page_content->proto);
  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>
      wrapper_callback = password_manager::metrics_util::TimeCallback(
          base::BindOnce(
              &PasswordChangeSubmissionVerifier::OnExecutionResponseCallback,
              weak_ptr_factory_.GetWeakPtr()),
          kPasswordChangeVerificationTimeHistogram);

  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(web_contents_),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request, /*execution_timeout=*/std::nullopt, std::move(wrapper_callback));
}

void PasswordChangeSubmissionVerifier::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  CHECK(callback_);
  CHECK(web_contents_);

  ukm::SourceId source_id =
      web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId();

  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      std::nullopt;

  if (!execution_result.response.has_value()) {
    LogSubmissionOutcome(SubmissionOutcome::kNoResponse, source_id);
  } else {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::PasswordChangeResponse>(
        execution_result.response.value());

    if (!response) {
      LogSubmissionOutcome(SubmissionOutcome::kCouldNotParse, source_id);
    }
  }

  logs_uploader_->SetVerifySubmissionQuality(response, std::move(logging_data));
  if (!response) {
    // Password change failed as the response was empty or
    // unable to be parsed.
    std::move(callback_).Run(false);
    return;
  }

  RecordOutcomeMetrics(response.value().outcome_data(), source_id);
  PasswordChangeOutcome outcome =
      response.value().outcome_data().submission_outcome();
  if (outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME &&
      outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    // Password change was unsuccessful.
    std::move(callback_).Run(false);
    return;
  }
  // Password change was successfully completed.
  std::move(callback_).Run(true);
}
