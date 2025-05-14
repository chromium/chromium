// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_form_submission_verifier.h"

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
#include "components/password_manager/content/browser/content_password_manager_driver.h"
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
using SubmissionOutcome = ChangeFormSubmissionVerifier::SubmissionOutcome;
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

ChangeFormSubmissionVerifier::ChangeFormSubmissionVerifier(
    content::WebContents* web_contents,
    FormSubmissionResultCallback callback,
    ModelQualityLogsUploader* logs_uploader)
    : web_contents_(web_contents->GetWeakPtr()),
      capture_annotated_page_content_(
          base::BindOnce(&optimization_guide::GetAIPageContent,
                         web_contents,
                         GetAIPageContentOptions())),
      callback_(std::move(callback)),
      logs_uploader_(logs_uploader) {}

ChangeFormSubmissionVerifier::ChangeFormSubmissionVerifier(
    base::PassKey<class ChangeFormSubmissionVerifierTest>,
    content::WebContents* web_contents,
    base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
        capture_annotated_page_content,
    FormSubmissionResultCallback callback,
    ModelQualityLogsUploader* logs_uploader)
    : ChangeFormSubmissionVerifier(web_contents,
                                   std::move(callback),
                                   std::move(logs_uploader)) {
  capture_annotated_page_content_ = std::move(capture_annotated_page_content);
}

ChangeFormSubmissionVerifier::~ChangeFormSubmissionVerifier() = default;

void ChangeFormSubmissionVerifier::FillChangePasswordForm(
    password_manager::PasswordFormManager* form_manager,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  CHECK(form_manager);
  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  form_manager_ = form_manager->Clone();
  // PostTask is required because if the form is filled immediately the fields
  // might be cleared by PasswordAutofillAgent if there were no credentials to
  // fill during SendFillInformationToRenderer call.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChangeFormSubmissionVerifier::TriggerFilling,
                     weak_ptr_factory_.GetWeakPtr(),
                     *form_manager->GetParsedObservedForm(),
                     form_manager->GetDriver(), old_password, new_password));

  // Proceed with verifying password on timeout, in case submission was not
  // captured.
  timeout_timer_.Start(FROM_HERE,
                       ChangeFormSubmissionVerifier::kSubmissionWaitingTimeout,
                       this, &ChangeFormSubmissionVerifier::RequestPageContent);
}

void ChangeFormSubmissionVerifier::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (!password_form_submitted_) {
    return;
  }
  if (!web_contents_) {
    return;
  }
  if (web_contents != web_contents_.get()) {
    return;
  }
  if (std::exchange(submission_detected_, true)) {
    return;
  }
  timeout_timer_.FireNow();
}

void ChangeFormSubmissionVerifier::SavePassword(
    const std::u16string& username) {
  CHECK(!callback_);
  CHECK(form_manager_);
  form_manager_->OnUpdateUsernameFromPrompt(username);
  form_manager_->Save();
}

GURL ChangeFormSubmissionVerifier::GetURL() const {
  CHECK(form_manager_);
  return form_manager_->GetURL();
}

void ChangeFormSubmissionVerifier::TriggerFilling(
    const password_manager::PasswordForm& form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  CHECK(form_manager_);
  if (!driver) {
    return;
  }

  driver->FillChangePasswordForm(
      form.password_element_renderer_id, form.new_password_element_renderer_id,
      form.confirmation_password_element_renderer_id, old_password,
      new_password,
      base::BindOnce(&ChangeFormSubmissionVerifier::ChangePasswordFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), driver,
                     form.new_password_element_renderer_id));

  form_manager_->PresaveGeneratedPassword(form.form_data, new_password);
}

void ChangeFormSubmissionVerifier::ChangePasswordFormFilled(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    autofill::FieldRendererId field_id,
    const std::optional<autofill::FormData>& submitted_form) {
  if (!driver) {
    return;
  }

  if (!submitted_form) {
    // TODO(crbug.com/398754700): Change password form disappeared, consider
    // searching for change-pwd form again.
    return;
  }

  form_manager_->ProvisionallySave(
      submitted_form.value(), form_manager_->GetDriver().get(),
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          password_manager::kMaxSingleUsernameFieldsToStore));
  driver->SubmitFormWithEnter(
      field_id, base::BindOnce(&ChangeFormSubmissionVerifier::OnFormSubmitted,
                               weak_ptr_factory_.GetWeakPtr(), driver));
}

void ChangeFormSubmissionVerifier::OnFormSubmitted(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    bool success) {
  if (success) {
    password_form_submitted_ = true;
    return;
  }
  // TODO(crbug.com/407487665): Attempt to submit change password form by
  // looking for a submit button.
}

void ChangeFormSubmissionVerifier::RequestPageContent() {
  // If browser didn't receive confirmation about change password form
  // submission from driver, fail immediately.
  if (!password_form_submitted_ || !web_contents_) {
    std::move(callback_).Run(false);
    return;
  }

  base::UmaHistogramBoolean(kPasswordChangeSubmittedHistogram,
                            submission_detected_);

  base::ConcurrentClosures concurrent_closures;
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(
               &ChangeFormSubmissionVerifier::OnAnnotatedPageContentReceived,
               weak_ptr_factory_.GetWeakPtr())
               .Then(concurrent_closures.CreateClosure()));
  // TODO(crbug.com/409946698): Delete this when removing support for AX tree
  // prompts.
  web_contents_->RequestAXTreeSnapshot(
      base::BindOnce(&ChangeFormSubmissionVerifier::OnAxTreeReceived,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(concurrent_closures.CreateClosure()),
      ui::AXMode::kWebContents, kMaxNodesInAXTreeSnapshot,
      /* timeout= */ {}, content::WebContents::AXTreeSnapshotPolicy::kAll);
  std::move(concurrent_closures)
      .Done(base::BindOnce(
          &ChangeFormSubmissionVerifier::CheckSubmissionSuccessful,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangeFormSubmissionVerifier::OnAnnotatedPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> page_content) {
  if (page_content.has_value()) {
    *check_submission_successful_request_.mutable_page_context()
         ->mutable_annotated_page_content() = std::move(page_content->proto);
  }
}

void ChangeFormSubmissionVerifier::OnAxTreeReceived(
    ui::AXTreeUpdate& ax_tree_update) {
  ProtoTreeUpdate ax_tree_proto;
  optimization_guide::PopulateAXTreeUpdateProto(ax_tree_update, &ax_tree_proto);
  // Construct request.
  *check_submission_successful_request_.mutable_page_context()
       ->mutable_ax_tree_data() = std::move(ax_tree_proto);
}

void ChangeFormSubmissionVerifier::CheckSubmissionSuccessful() {
  if (!check_submission_successful_request_.has_page_context() ||
      !check_submission_successful_request_.page_context()
           .has_annotated_page_content()) {
    // TODO (crbug.com/413318086): Add metrics to handle failure of capturing
    // annotated page content.
    std::move(callback_).Run(false);
    return;
  }
  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::PasswordChangeSubmissionLoggingData>
      wrapper_callback = password_manager::metrics_util::TimeCallback(
          base::BindOnce(
              &ChangeFormSubmissionVerifier::OnExecutionResponseCallback,
              weak_ptr_factory_.GetWeakPtr()),
          kPasswordChangeVerificationTimeHistogram);

  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      check_submission_successful_request_,
      /*execution_timeout=*/std::nullopt, std::move(wrapper_callback));
}

OptimizationGuideKeyedService*
ChangeFormSubmissionVerifier::GetOptimizationService() const {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void ChangeFormSubmissionVerifier::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  CHECK(callback_);
  CHECK(web_contents_);

  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents_.get());

  if (!execution_result.response.has_value()) {
    LogSubmissionOutcome(SubmissionOutcome::kNoResponse,
                         client->GetUkmSourceId());
    std::move(callback_).Run(false);
    return;
  }
  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PasswordChangeResponse>(
          execution_result.response.value());
  if (!response) {
    LogSubmissionOutcome(SubmissionOutcome::kCouldNotParse,
                         client->GetUkmSourceId());
    std::move(callback_).Run(false);
    return;
  }

  if (logging_data) {
    // There is data to log, meaning this is a complete response.
    logs_uploader_->MergeData(response.value(), std::move(logging_data));
  }

  RecordOutcomeMetrics(response.value().outcome_data(),
                       client->GetUkmSourceId());
  PasswordChangeOutcome outcome =
      response.value().outcome_data().submission_outcome();
  if (outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME &&
      outcome !=
          PasswordChangeOutcome::
              PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    std::move(callback_).Run(false);
    return;
  }
  std::move(callback_).Run(true);
}
