// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"

#include "base/functional/concurrent_closures.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PasswordChangeErrorCase = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeErrorCase;
using ProtoTreeUpdate = optimization_guide::proto::AXTreeUpdate;
using SubmissionOutcome = PasswordChangeSubmissionVerifier::SubmissionOutcome;
using FinalModelStatus = optimization_guide::proto::FinalModelStatus;
using QualityLogEntry =
    std::unique_ptr<optimization_guide::ModelQualityLogEntry>;
using page_content_annotations::PageContentExtractionService;

// Max numbers of nodes for the AX Tree Update Snapshot.
constexpr int kMaxNodesInAXTreeSnapshot = 5000;

constexpr char kSubmissionOutcomeHistogramName[] =
    "PasswordManager.PasswordChangeSubmissionOutcome";

void LogSubmissionOutcome(SubmissionOutcome outcome, ukm::SourceId ukm_id) {
  base::UmaHistogramEnumeration(kSubmissionOutcomeHistogramName, outcome);
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
  auto options = blink::mojom::AIPageContentOptions::New();
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  options->on_critical_path = true;
  return options;
}

}  // namespace

PasswordChangeSubmissionVerifier::PasswordChangeSubmissionVerifier(
    content::WebContents* web_contents,
    ModelQualityLogsUploader* logs_uploader)
    : web_contents_(web_contents->GetWeakPtr()),
      capture_annotated_page_content_(
          base::BindOnce(&optimization_guide::GetAIPageContent,
                         web_contents,
                         GetAIPageContentOptions())),
      logs_uploader_(logs_uploader) {}

PasswordChangeSubmissionVerifier::~PasswordChangeSubmissionVerifier() = default;

void PasswordChangeSubmissionVerifier::CheckSubmissionOutcome(
    FormSubmissionResultCallback callback) {
  CHECK(web_contents_);
  callback_ = std::move(callback);

  base::ConcurrentClosures concurrent_closures;
  std::move(capture_annotated_page_content_)
      .Run(
          base::BindOnce(
              &PasswordChangeSubmissionVerifier::OnAnnotatedPageContentReceived,
              weak_ptr_factory_.GetWeakPtr())
              .Then(concurrent_closures.CreateClosure()));
  // TODO(crbug.com/409946698): Delete this when removing support for AX tree
  // prompts.
  web_contents_->RequestAXTreeSnapshot(
      base::BindOnce(&PasswordChangeSubmissionVerifier::OnAxTreeReceived,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(concurrent_closures.CreateClosure()),
      ui::AXMode::kWebContents, kMaxNodesInAXTreeSnapshot,
      /* timeout= */ {}, content::WebContents::AXTreeSnapshotPolicy::kAll);
  std::move(concurrent_closures)
      .Done(base::BindOnce(
          &PasswordChangeSubmissionVerifier::CheckSubmissionSuccessful,
          weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeSubmissionVerifier::OnAnnotatedPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> page_content) {
  if (page_content.has_value()) {
    *check_submission_successful_request_.mutable_page_context()
         ->mutable_annotated_page_content() = std::move(page_content->proto);
  }
}

void PasswordChangeSubmissionVerifier::OnAxTreeReceived(
    ui::AXTreeUpdate& ax_tree_update) {
  ProtoTreeUpdate ax_tree_proto;
  optimization_guide::PopulateAXTreeUpdateProto(ax_tree_update, &ax_tree_proto);
  // Construct request.
  *check_submission_successful_request_.mutable_page_context()
       ->mutable_ax_tree_data() = std::move(ax_tree_proto);
}

void PasswordChangeSubmissionVerifier::CheckSubmissionSuccessful() {
  CHECK(callback_);
  if (!check_submission_successful_request_.has_page_context() ||
      !check_submission_successful_request_.page_context()
           .has_annotated_page_content()) {
    // TODO (crbug.com/413318086): Add metrics to handle failure of capturing
    // annotated page content.
    std::move(callback_).Run(false);
    return;
  }
  server_request_start_time_ = base::Time::Now();
  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>
      wrapper_callback = password_manager::metrics_util::TimeCallback(
          base::BindOnce(
              &PasswordChangeSubmissionVerifier::OnExecutionResponseCallback,
              weak_ptr_factory_.GetWeakPtr()),
          kPasswordChangeVerificationTimeHistogram);

  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      check_submission_successful_request_,
      /*execution_timeout=*/std::nullopt, std::move(wrapper_callback));
}

OptimizationGuideKeyedService*
PasswordChangeSubmissionVerifier::GetOptimizationService() const {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
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

  if (logging_data) {
    logs_uploader_->SetVerifySubmissionQuality(
        response, std::move(logging_data), server_request_start_time_);
  }

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
