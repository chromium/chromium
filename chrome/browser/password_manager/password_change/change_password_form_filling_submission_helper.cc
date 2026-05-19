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
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filler.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/password_manager/password_change/password_change_logging_util.h"
#include "chrome/browser/password_manager/password_change/password_change_submission_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
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
using SubmissionResult =
    ChangePasswordFormFillingSubmissionHelper::SubmissionResult;
using password_change::LogBoolean;
using password_change::LogMessage;
using password_change::LogResponse;
using password_change::LogString;

constexpr optimization_guide::proto::PasswordChangeRequest::FlowStep
    kSubmitFormFlowStep = optimization_guide::proto::PasswordChangeRequest::
        FlowStep::PasswordChangeRequest_FlowStep_SUBMIT_FORM_STEP;

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  blink::mojom::AIPageContentOptionsPtr options =
      optimization_guide::ActionableAIPageContentOptions(
          /*on_critical_path =*/true);
  options->include_same_site_only = true;
  return options;
}



ChangePasswordFormFillingSubmissionHelper::SubmissionResult LogError(
    ChangePasswordFormFillingSubmissionHelper::SubmissionResult result) {
  if (result.has_value()) {
    return result;
  }
  base::UmaHistogramEnumeration(
      "PasswordManager.ChangePasswordFormSubmissionError", result.error());
  return result;
}

}  // namespace

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        content::WebContents* web_contents,
        password_manager::PasswordManagerClient* client,
        base::OnceCallback<void(SubmissionResult)> callback)
    : creation_time_(base::Time::Now()),
      web_contents_(web_contents),
      client_(client),
      callback_(base::BindOnce(&LogError).Then(std::move(callback))) {}

ChangePasswordFormFillingSubmissionHelper::
    ChangePasswordFormFillingSubmissionHelper(
        content::WebContents* web_contents,
        password_manager::PasswordManagerClient* client,
        ModelQualityLogsUploader* logs_uploader,
        base::OnceCallback<void(SubmissionResult)> callback)
    : ChangePasswordFormFillingSubmissionHelper(web_contents,
                                                client,
                                                std::move(callback)) {
  CHECK(logs_uploader);
  logs_uploader_ = logs_uploader;
}

ChangePasswordFormFillingSubmissionHelper::
    ~ChangePasswordFormFillingSubmissionHelper() {
  base::TimeDelta time_delta = base::Time::Now() - creation_time_;
  base::UmaHistogramMediumTimes("PasswordManager.TimeSpentChangingPassword",
                                time_delta);
  if (logs_uploader_) {
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

  filler_ = std::make_unique<ChangePasswordFormFiller>(web_contents_, client_,
                                                       logs_uploader_);
  filler_->FillForm(
      form_manager, username, login_password, generated_password,
      base::BindOnce(&ChangePasswordFormFillingSubmissionHelper::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr()));

  // Proceed with verifying password on timeout, in case submission was not
  // captured.
  timeout_timer_.Start(
      FROM_HERE,
      ChangePasswordFormFillingSubmissionHelper::kSubmissionWaitingTimeout,
      this, &ChangePasswordFormFillingSubmissionHelper::OnTimeout);
}

void ChangePasswordFormFillingSubmissionHelper::OnFormFilled(
    base::expected<std::unique_ptr<password_manager::PasswordFormManager>,
                   SubmissionError> result) {
  if (!result.has_value()) {
    std::move(callback_).Run(base::unexpected(result.error()));
    return;
  }

  form_manager_ = std::move(result.value());

  capturer_ = AnnotatedPageContentCapturer::Create(
      web_contents_, client_, GetAIPageContentOptions(),
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnPageContentReceived,
          weak_ptr_factory_.GetWeakPtr()));
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

void ChangePasswordFormFillingSubmissionHelper::OnPageContentReceived(
    optimization_guide::AIPageContentResultOrError content) {
  LogBoolean(client_,
             Logger::STRING_AUTOMATED_PASSWORD_CHANGE_PAGE_CONTENT_RECEIVED,
             content.has_value());
  if (!content.has_value()) {
    LogPageContentCaptureFailure(password_manager::metrics_util::
                                     PasswordChangeFlowStep::kSubmitFormStep);
    std::move(callback_).Run(
        base::unexpected(SubmissionError::kFailedToCaptureContent));
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
    if (response) {
      LogResponse(client_, Logger::STRING_MESSAGE, *response);
    }
  }
  if (logs_uploader_) {
    logs_uploader_->SetSubmitFormQuality(response, std::move(logging_data));
  }

  if (!response) {
    std::move(callback_).Run(
        base::unexpected(SubmissionError::kFailedToParseResponse));
    return;
  }

  if (response->submit_form_data().is_user_intervention_needed()) {
    std::move(callback_).Run(
        base::unexpected(SubmissionError::kInterventionDetected));
    return;
  }

  int dom_node_id = response.value().submit_form_data().dom_node_id_to_click();

  if (!dom_node_id) {
    // Fail immediately as model didn't provide a submit element to click.
    std::move(callback_).Run(
        base::unexpected(SubmissionError::kSubmitButtonNotFound));
    return;
  }

  // Once button is clicked timeout can cause password on a website and inside
  // Password Manager to diverge. Better to wait for click result to avoid false
  // negatives.
  timeout_timer_.Stop();

  click_helper_ = std::make_unique<ButtonClickHelper>(
      web_contents_.get(), client_, dom_node_id,
      base::BindOnce(
          &ChangePasswordFormFillingSubmissionHelper::OnButtonClicked,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFillingSubmissionHelper::OnButtonClicked(
    actor::mojom::ActionResultCode result) {
  CHECK(web_contents_);

  if (result == actor::mojom::ActionResultCode::kOk) {
    std::move(callback_).Run(std::move(form_manager_));
    return;
  }

  if (logs_uploader_) {
    logs_uploader_->RecordButtonClickFailure(kSubmitFormFlowStep, result);
  }
  std::move(callback_).Run(
      base::unexpected(SubmissionError::kFailedToClickSubmit));
}

void ChangePasswordFormFillingSubmissionHelper::OnTimeout() {
  LogMessage(client_, Logger::STRING_AUTOMATED_PASSWORD_CHANGE_TIMEOUT);
  if (logs_uploader_) {
    logs_uploader_->SetFlowInterrupted(
        kSubmitFormFlowStep,
        ModelQualityLogsUploader::QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_TIME_OUT);
  }
  std::move(callback_).Run(base::unexpected(SubmissionError::kTimeout));
}

