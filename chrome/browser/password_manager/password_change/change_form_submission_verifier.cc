// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_form_submission_verifier.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

using PasswordChangeOutcome = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeOutcome;
using PasswordChangeErrorCase = optimization_guide::proto ::
    PasswordChangeSubmissionData_PasswordChangeErrorCase;
using ProtoTreeUpdate = optimization_guide::proto::AXTreeUpdate;
using SubmissionOutcome = ChangeFormSubmissionVerifier::SubmissionOutcome;

// Max numbers of nodes for the AX Tree Update Snapshot.
constexpr int kMaxNodesInAXTreeSnapshot = 5000;

constexpr char kSubmissionOutcomeHistogramName[] =
    "PasswordManager.PasswordChangeSubmissionOutcome";

void LogSubmissionOutcome(SubmissionOutcome outcome) {
  base::UmaHistogramEnumeration(kSubmissionOutcomeHistogramName, outcome);
}

void RecordOutcomeMetrics(
    optimization_guide::proto ::PasswordChangeSubmissionData submission_data) {
  PasswordChangeOutcome outcome = submission_data.submission_outcome();
  if (outcome ==
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_SUCCESSFUL_OUTCOME) {
    LogSubmissionOutcome(SubmissionOutcome::kSuccess);
    return;
  }
  if (outcome ==
      PasswordChangeOutcome::
          PasswordChangeSubmissionData_PasswordChangeOutcome_UNKNOWN_OUTCOME) {
    LogSubmissionOutcome(SubmissionOutcome::kUnknown);
    return;
  }
  for (auto error_case_enum : submission_data.error_case()) {
    PasswordChangeErrorCase error_case = optimization_guide::proto ::
        PasswordChangeSubmissionData_PasswordChangeErrorCase(error_case_enum);
    switch (error_case) {
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_OLD_PASSWORD_INCORRECT:
        LogSubmissionOutcome(SubmissionOutcome::kErrorOldPasswordIncorrect);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_PASSWORDS_DO_NOT_MATCH:
        LogSubmissionOutcome(SubmissionOutcome::kErrorOldPasswordDoNotMatch);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_NEW_PASSWORD_INCORRECT:
        LogSubmissionOutcome(SubmissionOutcome::kErrorNewPasswordIncorrect);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_PAGE_ERROR:
        LogSubmissionOutcome(SubmissionOutcome::kPageError);
        break;
      case optimization_guide::proto::
          PasswordChangeSubmissionData_PasswordChangeErrorCase_UNKNOWN_CASE:
      default:
        LogSubmissionOutcome(SubmissionOutcome::kUncategorizedError);
        break;
    }
  }

  if (!submission_data.error_case_size()) {
    LogSubmissionOutcome(SubmissionOutcome::kUncategorizedError);
  }
}

}  // namespace

ChangeFormSubmissionVerifier::ChangeFormSubmissionVerifier(
    content::WebContents* web_contents,
    FormSubmissionResultCallback callback)
    : web_contents_(web_contents->GetWeakPtr()),
      callback_(std::move(callback)) {}

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
                       this, &ChangeFormSubmissionVerifier::RequestAXTree);
}

void ChangeFormSubmissionVerifier::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (!web_contents_) {
    return;
  }
  if (web_contents != web_contents_.get()) {
    return;
  }
  if (std::exchange(submission_detected_, true)) {
    return;
  }
  RequestAXTree();
  // Stop timer since submission has been detected.
  timeout_timer_.Reset();
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

  driver->SubmitChangePasswordForm(
      form.password_element_renderer_id, form.new_password_element_renderer_id,
      form.confirmation_password_element_renderer_id, old_password,
      new_password,
      base::BindOnce(&ChangeFormSubmissionVerifier::ChangePasswordFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), driver));

  form_manager_->PresaveGeneratedPassword(form.form_data, new_password);
}

void ChangeFormSubmissionVerifier::ChangePasswordFormFilled(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    const autofill::FormData& submitted_form) {
  if (!driver) {
    return;
  }

  form_manager_->ProvisionallySave(
      submitted_form, form_manager_->GetDriver().get(),
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          password_manager::kMaxSingleUsernameFieldsToStore));
}

void ChangeFormSubmissionVerifier::RequestAXTree() {
  if (!web_contents_) {
    return;
  }
  web_contents_->RequestAXTreeSnapshot(
      base::BindOnce(&ChangeFormSubmissionVerifier::ProcessTree,
                     weak_ptr_factory_.GetWeakPtr()),
      ui::AXMode::kWebContents, kMaxNodesInAXTreeSnapshot,
      /* timeout= */ {}, content::WebContents::AXTreeSnapshotPolicy::kAll);
}

void ChangeFormSubmissionVerifier::ProcessTree(
    ui::AXTreeUpdate& ax_tree_update) {
  ProtoTreeUpdate ax_tree_proto;
  optimization_guide::PopulateAXTreeUpdateProto(ax_tree_update, &ax_tree_proto);
  // Construct request.
  optimization_guide::proto::PasswordChangeRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_proto);

  OptimizationGuideKeyedService* optimization_executor =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));

  optimization_executor->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ChangeFormSubmissionVerifier::OnExecutionResponseCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChangeFormSubmissionVerifier::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  CHECK(callback_);

  if (!execution_result.response.has_value()) {
    LogSubmissionOutcome(SubmissionOutcome::kNoResponse);
    std::move(callback_).Run(false);
    return;
  }
  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PasswordChangeResponse>(
          execution_result.response.value());
  if (!response) {
    LogSubmissionOutcome(SubmissionOutcome::kCouldNotParse);
    std::move(callback_).Run(false);
    return;
  }

  RecordOutcomeMetrics(response.value().outcome_data());
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
