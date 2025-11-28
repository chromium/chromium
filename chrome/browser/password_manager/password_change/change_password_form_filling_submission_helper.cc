// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/form_filling_helper.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

constexpr optimization_guide::proto::PasswordChangeRequest::FlowStep
    kSubmitFormFlowStep = optimization_guide::proto::PasswordChangeRequest::
        FlowStep::PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP;

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUseActionablesForImprovedPasswordChange)) {
    return optimization_guide::ActionableAIPageContentOptions(
        /*on_critical_path =*/true);
  }
  return optimization_guide::DefaultAIPageContentOptions(
      /*on_critical_path =*/true);
}

std::unique_ptr<Logger> GetLoggerIfAvailable(
    password_manager::PasswordManagerClient* client) {
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<Logger>(log_manager);
  }

  return nullptr;
}

FormFillingHelper::FillingTasks PrepareFormForFilling(
    const password_manager::PasswordForm& form,
    const std::u16string& old_password,
    const std::u16string& new_password) {
  FormFillingHelper::FillingTasks filling_tasks;

  CHECK(form.new_password_element_renderer_id);
  filling_tasks[{form.form_data.host_frame(),
                 form.new_password_element_renderer_id}] = new_password;

  if (form.password_element_renderer_id) {
    filling_tasks[{form.form_data.host_frame(),
                   form.password_element_renderer_id}] = old_password;
  }

  if (form.confirmation_password_element_renderer_id) {
    filling_tasks[{form.form_data.host_frame(),
                   form.confirmation_password_element_renderer_id}] =
        new_password;
  }

  return filling_tasks;
}

}  // namespace

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        content::WebContents* web_contents,
        password_manager::PasswordManagerClient* client,
        ModelQualityLogsUploader* logs_uploader,
        base::OnceCallback<void(bool)> callback)
    : creation_time_(base::Time::Now()),
      web_contents_(web_contents),
      client_(client),
      logs_uploader_(logs_uploader),
      callback_(std::move(callback)) {
  CHECK(logs_uploader_);
  capture_annotated_page_content_ =
      base::BindOnce(&optimization_guide::GetAIPageContent, web_contents,
                     GetAIPageContentOptions());
}

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        base::PassKey<class ChangePasswordFormFillingSubmissionHelperTest>,
        content::WebContents* web_contents,
        password_manager::PasswordManagerClient* client,
        ModelQualityLogsUploader* logs_uploader,
        base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
            capture_annotated_page_content,
        base::OnceCallback<void(bool)> result_callback)
    : ChangePasswordFormFillingSubmissionHelper(web_contents,
                                                client,
                                                logs_uploader,
                                                std::move(result_callback)) {
  capture_annotated_page_content_ = std::move(capture_annotated_page_content);
}

ChangePasswordFormFillingSubmissionHelper::
    ~ChangePasswordFormFillingSubmissionHelper() {
  // Record duration in case the something went wrong before the helper reached
  // Submit click.
  if (creation_time_) {
    base::TimeDelta time_delta = base::Time::Now() - creation_time_.value();
    base::UmaHistogramMediumTimes("PasswordManager.TimeSpentChangingPassword",
                                  time_delta);
    logs_uploader_->SetStepDuration(kSubmitFormFlowStep, time_delta);
  }
}

void ChangePasswordFormFillingSubmissionHelper::FillChangePasswordForm(
    password_manager::PasswordFormManager* form_manager,
    const std::u16string& username,
    const std::u16string& login_password,
    const std::u16string& generated_password) {
  CHECK(form_manager);
  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  username_ = username;
  login_password_ = login_password;
  generated_password_ = generated_password;

  // TODO(crbug.com/422125487): Fix metrics duplication.
  form_manager_ = form_manager->Clone();
  logs_uploader_->SetChangePasswordFormData(
      *form_manager->GetParsedObservedForm());

  const password_manager::PasswordForm* best_match =
      password_manager_util::FindFormByUsername(form_manager_->GetBestMatches(),
                                                username_);
  // If we already have a password for the website but it doesn't match the one
  // used to log in, we don't want to overwrite the existing password without
  // user consent even if the password is wrong.
  stored_password_ = best_match ? best_match->password_value : login_password_;

  TriggerFilling(*form_manager->GetParsedObservedForm(),
                 form_manager->GetDriver());

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
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogMessage(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_FORM_SUBMISSION);
  }
  if (!timeout_timer_.IsRunning()) {
    return;
  }
  timeout_timer_.Stop();
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
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  CHECK(form_manager_);
  if (!driver) {
    // Fail immediately as something went terribly wrong (e.g. page crashed).
    std::move(callback_).Run(false);
    return;
  }

  observed_fields_.push_back(form.new_password_element_renderer_id);

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogString(
        Logger::STRING_PASSWORD_CHANGE_CURRENT_PASSWORD_RENDERER_ID,
        base::NumberToString(form.password_element_renderer_id.value()));
    logger->LogString(
        Logger::STRING_PASSWORD_CHANGE_NEW_PASSWORD_RENDERER_ID,
        base::NumberToString(form.new_password_element_renderer_id.value()));
    logger->LogString(
        Logger::STRING_PASSWORD_CHANGE_CONFIRMATION_PASSWORD_RENDERER_ID,
        base::NumberToString(
            form.confirmation_password_element_renderer_id.value()));
  }

  auto filling_callback = base::BindOnce(
      &ChangePasswordFormFillingSubmissionHelper::ChangePasswordFormFilled,
      weak_ptr_factory_.GetWeakPtr());

  if (base::FeatureList::IsEnabled(
          password_manager::features::kFillChangePasswordFormByTyping)) {
    form_filler_ = std::make_unique<FormFillingHelper>(
        web_contents_, driver,
        PrepareFormForFilling(form, login_password_, generated_password_),
        std::move(filling_callback));
  } else {
    // PostTask is required because if the form is filled immediately the fields
    // might be cleared by PasswordAutofillAgent if there were no credentials to
    // fill during SendFillInformationToRenderer call.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &password_manager::PasswordManagerDriver::FillChangePasswordForm,
            driver, form.password_element_renderer_id,
            form.new_password_element_renderer_id,
            form.confirmation_password_element_renderer_id, login_password_,
            generated_password_, std::move(filling_callback)));
  }

  password_manager::PasswordForm form_to_save(form);
  form_to_save.username_value = username_;
  form_to_save.password_value = stored_password_;
  password_manager::PasswordFormManager::PresaveGeneratedPasswordAsBackup(
      *form_manager_, form_to_save, generated_password_);
  // Fetch newly saved password so that it's included in the matches when we
  // save the submitted form.
  form_manager_->GetFormFetcher()->Fetch();
}

void ChangePasswordFormFillingSubmissionHelper::ChangePasswordFormFilled(
    const std::optional<autofill::FormData>& submitted_form) {
  form_filler_.reset();

  bool provisionally_saved = false;
  if (submitted_form) {
    provisionally_saved = form_manager_->ProvisionallySave(
        submitted_form.value(), form_manager_->GetDriver().get(),
        base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                       password_manager::PossibleUsernameData>(
            password_manager::kMaxSingleUsernameFieldsToStore));
  }

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(Logger::STRING_PASSWORD_CHANGE_FORM_FILLING_RESULT,
                       provisionally_saved);
  }

  if (!provisionally_saved) {
    logs_uploader_->SetFlowInterrupted(
        kSubmitFormFlowStep,
        ModelQualityLogsUploader::QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_FILLING_FAILED);
    // Change password form disappeared, some websites practice updating form
    // dynamically which resets the form. Try to find a new change-pwd form.
    form_waiter_ =
        ChangePasswordFormWaiter::Builder(
            web_contents_, client_,
            base::BindOnce(&ChangePasswordFormFillingSubmissionHelper::
                               OnChangePasswordFormFound,
                           weak_ptr_factory_.GetWeakPtr()))
            .SetTimeoutCallback(
                base::BindOnce(&ChangePasswordFormFillingSubmissionHelper::
                                   OnSubmissionOutcomeChecked,
                               weak_ptr_factory_.GetWeakPtr(), false))
            .SetFieldsToIgnore(observed_fields_)
            .Build();
    return;
  }

  // Sanity check: the generated password is provisionally saved as the primary
  // one.
  CHECK_EQ(form_manager_->GetPendingCredentials().password_value,
           generated_password_);
  form_manager_->UpdateBackupPassword(stored_password_);

  CHECK(capture_annotated_page_content_);
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnPageContentReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFillingSubmissionHelper::OnPageContentReceived(
    optimization_guide::AIPageContentResultOrError content) {
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_PAGE_CONTENT_RECEIVED,
        content.has_value());
  }
  if (!content.has_value()) {
    LogPageContentCaptureFailure(password_manager::metrics_util::
                                     PasswordChangeFlowStep::kSubmitFormStep);
    std::move(callback_).Run(false);
    return;
  }
  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(kSubmitFormFlowStep);
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
  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      std::nullopt;
  if (execution_result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::PasswordChangeResponse>(
        execution_result.response.value());
  }
  logs_uploader_->SetSubmitFormQuality(response, std::move(logging_data));

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

  CHECK(creation_time_);
  base::TimeDelta time_delta = base::Time::Now() - creation_time_.value();
  base::UmaHistogramMediumTimes("PasswordManager.TimeSpentChangingPassword",
                                time_delta);
  logs_uploader_->SetStepDuration(kSubmitFormFlowStep, time_delta);

  // Reset creation_time_ to avoid recording duration the second time in
  // destructor.
  creation_time_ = std::nullopt;

  submission_verifier_ = std::make_unique<PasswordChangeSubmissionVerifier>(
      web_contents_, logs_uploader_);
  click_helper_ = std::make_unique<ButtonClickHelper>(
      web_contents_.get(), client_, dom_node_id,
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnButtonClicked,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFillingSubmissionHelper::OnButtonClicked(
    actor::mojom::ActionResultCode result) {
  CHECK(web_contents_);
  click_helper_.reset();

  if (result != actor::mojom::ActionResultCode::kOk && !submission_detected_) {
    // Fail immediately as click failed and no form submission was detected.
    logs_uploader_->RecordButtonClickFailure(kSubmitFormFlowStep, result);
    std::move(callback_).Run(false);
    return;
  }
}

void ChangePasswordFormFillingSubmissionHelper::
    OnSubmissionDetectedOrTimeout() {
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogMessage(
        Logger::
            STRING_AUTOMATED_PASSWORD_CHANGE_SUBMISSION_DETECTED_OR_TIMEOUT);
  }
  if (!submission_verifier_) {
    CHECK(callback_);
    std::move(callback_).Run(false);
    return;
  }

  base::UmaHistogramBoolean(
      "PasswordManager.PasswordChangeVerificationTriggeredAutomatically",
      submission_detected_);

  submission_verifier_->CheckSubmissionOutcome(base::BindOnce(
      &ChangePasswordFormFillingSubmissionHelper::OnSubmissionOutcomeChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFillingSubmissionHelper::OnSubmissionOutcomeChecked(
    bool success) {
  CHECK(callback_);
  std::move(callback_).Run(success);
}

void ChangePasswordFormFillingSubmissionHelper::OnChangePasswordFormFound(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();
  CHECK(form_manager);

  CHECK(form_manager->GetParsedObservedForm());
  CHECK(form_manager->GetDriver());

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogMessage(Logger::STRING_AUTOMATED_PASSWORD_CHANGE_FORM_FOUND);
  }

  form_manager_ = form_manager->Clone();
  TriggerFilling(*form_manager->GetParsedObservedForm(),
                 form_manager->GetDriver());
}
