// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  auto options = blink::mojom::AIPageContentOptions::New();
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  options->on_critical_path = true;
  return options;
}

std::unique_ptr<Logger> GetLoggerIfAvailable(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  password_manager::PasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents);
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<Logger>(log_manager);
  }

  return nullptr;
}

}  // namespace

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        content::WebContents* web_contents,
        ModelQualityLogsUploader* logs_uploader,
        base::OnceCallback<void(bool)> callback)
    : web_contents_(web_contents),
      callback_(std::move(callback)),
      logs_uploader_(logs_uploader) {
  capture_annotated_page_content_ =
      base::BindOnce(&optimization_guide::GetAIPageContent, web_contents,
                     GetAIPageContentOptions());
}

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        base::PassKey<class ChangePasswordFormFillingSubmissionHelperTest>,
        content::WebContents* web_contents,
        ModelQualityLogsUploader* logs_uploader,
        base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
            capture_annotated_page_content,
        base::OnceCallback<void(bool)> result_callback)
    : web_contents_(web_contents),
      callback_(std::move(result_callback)),
      logs_uploader_(logs_uploader),
      capture_annotated_page_content_(
          std::move(capture_annotated_page_content)) {}

ChangePasswordFormFillingSubmissionHelper::
    ~ChangePasswordFormFillingSubmissionHelper() = default;

void ChangePasswordFormFillingSubmissionHelper::FillChangePasswordForm(
    password_manager::PasswordFormManager* form_manager,
    const std::u16string& username,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  CHECK(form_manager);
  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  // TODO(crbug.com/422125487): Fix metrics duplication.
  form_manager_ = form_manager->Clone();
  // PostTask is required because if the form is filled immediately the fields
  // might be cleared by PasswordAutofillAgent if there were no credentials to
  // fill during SendFillInformationToRenderer call.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChangePasswordFormFillingSubmissionHelper::TriggerFilling,
                     weak_ptr_factory_.GetWeakPtr(),
                     *form_manager->GetParsedObservedForm(),
                     form_manager->GetDriver(), username, old_password,
                     new_password));

  // Proceed with verifying password on timeout, in case submission was not
  // captured.
  timeout_timer_.Start(
      FROM_HERE,
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout,
      this,
      &ChangePasswordFormFillingSubmissionHelper::
          OnSubmissionDetectedOrTimeout);
}

void ChangePasswordFormFillingSubmissionHelper::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (!submission_verifier_) {
    return;
  }
  if (web_contents != web_contents_) {
    return;
  }
  if (std::exchange(submission_detected_, true)) {
    return;
  }
  if (!timeout_timer_.IsRunning()) {
    return;
  }
  timeout_timer_.Reset();
  OnSubmissionDetectedOrTimeout();
}

void ChangePasswordFormFillingSubmissionHelper::SavePassword(
    const std::u16string& username) {
  CHECK(!callback_);
  CHECK(form_manager_);
  form_manager_->OnUpdateUsernameFromPrompt(username);
  form_manager_->Save();
}

GURL ChangePasswordFormFillingSubmissionHelper::GetURL() const {
  CHECK(form_manager_);
  return form_manager_->GetURL();
}

void ChangePasswordFormFillingSubmissionHelper::TriggerFilling(
    const password_manager::PasswordForm& form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    const std::u16string& username,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  CHECK(form_manager_);
  if (!driver) {
    // Fail immediately as something went terribly wrong (e.g. page crashed).
    std::move(callback_).Run(false);
    return;
  }

  driver->FillChangePasswordForm(
      form.password_element_renderer_id, form.new_password_element_renderer_id,
      form.confirmation_password_element_renderer_id, old_password,
      new_password,
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::ChangePasswordFormFilled,
          weak_ptr_factory_.GetWeakPtr(), driver,
          form.new_password_element_renderer_id, old_password));

  password_manager::PasswordForm form_to_save(form);
  form_to_save.username_value = username;
  form_to_save.password_value = old_password;
  password_manager::PasswordFormManager::PresaveGeneratedPasswordAsBackup(
      *form_manager_, form_to_save, new_password);
  // Fetch newly saved password so that it's included in the matches when we
  // save the submitted form.
  form_manager_->GetFormFetcher()->Fetch();
}

void ChangePasswordFormFillingSubmissionHelper::ChangePasswordFormFilled(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    autofill::FieldRendererId field_id,
    const std::u16string& backup_password,
    const std::optional<autofill::FormData>& submitted_form) {
  if (!driver) {
    // Fail immediately as something went terribly wrong (e.g. page crashed).
    std::move(callback_).Run(false);
    return;
  }

  if (auto logger = GetLoggerIfAvailable(web_contents_)) {
    logger->LogBoolean(Logger::STRING_PASSWORD_CHANGE_FORM_FILLING_RESULT,
                       submitted_form.has_value());
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
  form_manager_->UpdateBackupPassword(backup_password);
  driver->SubmitFormWithEnter(
      field_id,
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnSubmitWithEnterResult,
          weak_ptr_factory_.GetWeakPtr(), driver));
}

void ChangePasswordFormFillingSubmissionHelper::OnSubmitWithEnterResult(
    base::WeakPtr<password_manager::PasswordManagerDriver> driver,
    bool success) {
  if (auto logger = GetLoggerIfAvailable(web_contents_)) {
    logger->LogBoolean(Logger::STRING_PASSWORD_CHANGE_SUBMIT_WITH_ENTER_RESULT,
                       success);
  }

  if (success) {
    OnFormSubmitted();
    return;
  }

  // Fallback to submission using optimization_guide.
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnPageContentReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFillingSubmissionHelper::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> content) {
  if (!content) {
    // Fail immediately as submit element can't be identified without `content`.
    std::move(callback_).Run(false);
    return;
  }

  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(optimization_guide::proto::PasswordChangeRequest::FlowStep::
                       PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP);
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(content->proto);
  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request, /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ChangePasswordFormFillingSubmissionHelper::
                         OnExecutionResponseCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

OptimizationGuideKeyedService*
ChangePasswordFormFillingSubmissionHelper::GetOptimizationService() {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void ChangePasswordFormFillingSubmissionHelper::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  CHECK(web_contents_);
  if (!execution_result.response.has_value()) {
    std::move(callback_).Run(false);
    return;
  }
  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PasswordChangeResponse>(
          execution_result.response.value());
  if (!response) {
    std::move(callback_).Run(false);
    return;
  }

  int dom_node_id = response.value().submit_form_data().dom_node_id_to_click();

  if (!dom_node_id) {
    // Fail immediately as model didn't provide a submit element to click.
    std::move(callback_).Run(false);
    return;
  }

  click_helper_ = std::make_unique<ButtonClickHelper>(
      web_contents_.get(), dom_node_id,
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnButtonClicked,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFillingSubmissionHelper::OnFormSubmitted() {
  submission_verifier_ = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents_, logs_uploader_);
}

void ChangePasswordFormFillingSubmissionHelper::OnButtonClicked(bool result) {
  CHECK(web_contents_);
  click_helper_.reset();

  if (auto logger = GetLoggerIfAvailable(web_contents_)) {
    logger->LogBoolean(Logger::STRING_PASSWORD_CHANGE_SUBMIT_WITH_MODEL_RESULT,
                       result);
  }

  if (!result) {
    // Fail immediately as click failed.
    std::move(callback_).Run(false);
    return;
  }

  OnFormSubmitted();
}

void ChangePasswordFormFillingSubmissionHelper::
    OnSubmissionDetectedOrTimeout() {
  if (!submission_verifier_) {
    CHECK(callback_);
    std::move(callback_).Run(false);
    return;
  }

  base::UmaHistogramBoolean(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically",
      submission_detected_);

  submission_verifier_->CheckSubmissionOutcome(std::move(callback_));
}
