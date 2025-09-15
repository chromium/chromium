// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

constexpr optimization_guide::proto::PasswordChangeRequest::FlowStep
    kOpenFormFlowStep = optimization_guide::proto::PasswordChangeRequest::
        FlowStep::PasswordChangeRequest_FlowStep_OPEN_FORM_STEP;

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  auto options = optimization_guide::DefaultAIPageContentOptions(
      /*on_critical_path =*/true);
  return options;
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

}  // namespace

ChangePasswordFormFinder::ChangePasswordFormFinder(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    ModelQualityLogsUploader* logs_uploader,
    ChangePasswordFormWaiter::PasswordFormFoundCallback callback)
    : web_contents_(web_contents),
      client_(client),
      logs_uploader_(logs_uploader),
      callback_(std::move(callback)) {
  CHECK(logs_uploader_);
  capture_annotated_page_content_ =
      base::BindOnce(&optimization_guide::GetAIPageContent, web_contents,
                     GetAIPageContentOptions());
  form_waiter_ =
      ChangePasswordFormWaiter::Builder(
          web_contents_, client_,
          base::BindOnce(&ChangePasswordFormFinder::OnFormFoundInitially,
                         weak_ptr_factory_.GetWeakPtr()))
          .SetTimeoutCallback(
              base::BindOnce(&ChangePasswordFormFinder::OnFormNotFoundInitially,
                             weak_ptr_factory_.GetWeakPtr()))
          .IgnoreHiddenForms()
          .Build();

  timeout_timer_.Start(FROM_HERE, kFormWaitingTimeout,
                       base::BindOnce(&ChangePasswordFormFinder::OnFormNotFound,
                                      weak_ptr_factory_.GetWeakPtr()));
}

ChangePasswordFormFinder::ChangePasswordFormFinder(
    base::PassKey<class ChangePasswordFormFinderTest>,
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    ModelQualityLogsUploader* logs_uploader,
    ChangePasswordFormWaiter::PasswordFormFoundCallback callback,
    base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
        capture_annotated_page_content)
    : ChangePasswordFormFinder(web_contents,
                               client,
                               logs_uploader,
                               std::move(callback)) {
  capture_annotated_page_content_ = std::move(capture_annotated_page_content);
}

ChangePasswordFormFinder::~ChangePasswordFormFinder() = default;

void ChangePasswordFormFinder::OnFormNotFoundInitially() {
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_PASSWORD_CHANGE_INITIAL_FORM_WAITING_RESULT, false);
  }

  CHECK(capture_annotated_page_content_);
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(&ChangePasswordFormFinder::OnPageContentReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnFormFoundInitially(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();
  CHECK(callback_);
  CHECK(form_manager);

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogMessage(Logger::STRING_AUTOMATED_PASSWORD_CHANGE_FORM_FOUND);
  }

  logs_uploader_->MarkStepSkipped(kOpenFormFlowStep);
  std::move(callback_).Run(form_manager);
}

void ChangePasswordFormFinder::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> content) {
  CHECK(web_contents_);
  CHECK(callback_);

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_PAGE_CONTENT_RECEIVED,
        content.has_value());
  }

  if (!content) {
    LogPageContentCaptureFailure(
        password_manager::metrics_util::PasswordChangeFlowStep::kOpenFormStep);
    std::move(callback_).Run(nullptr);
    return;
  }

  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(kOpenFormFlowStep);
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(content->proto);
  *request.mutable_page_context()->mutable_title() =
      base::UTF16ToUTF8(web_contents_->GetTitle());
  *request.mutable_page_context()->mutable_url() =
      web_contents_->GetLastCommittedURL().spec();
  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request, /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ChangePasswordFormFinder::OnExecutionResponseCallback,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
}

OptimizationGuideKeyedService*
ChangePasswordFormFinder::GetOptimizationService() {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void ChangePasswordFormFinder::OnExecutionResponseCallback(
    base::Time request_time,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  CHECK(web_contents_);
  CHECK(callback_);

  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      std::nullopt;
  if (execution_result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::PasswordChangeResponse>(
        execution_result.response.value());
  }

  logs_uploader_->SetOpenFormQuality(response, std::move(logging_data),
                                     request_time);

  if (!response) {
    std::move(callback_).Run(nullptr);
    return;
  }

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogNumber(Logger::STRING_PASSWORD_CHANGE_MODEL_PAGE_PREDICTION_TYPE,
                      response.value().open_form_data().page_type());
  }
  int dom_node_id = response.value().open_form_data().dom_node_id_to_click();
  if (!dom_node_id) {
    std::move(callback_).Run(nullptr);
    return;
  }

  form_waiter_.reset();
  click_helper_ = std::make_unique<ButtonClickHelper>(
      web_contents_, client_, dom_node_id,
      base::BindOnce(&ChangePasswordFormFinder::OnButtonClicked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnButtonClicked(bool result) {
  CHECK(web_contents_);
  CHECK(callback_);

  click_helper_.reset();

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_ON_BUTTON_CLICKED, result);
  }

  if (!result) {
    logs_uploader_->OpenFormTargetElementNotFound();
    std::move(callback_).Run(nullptr);
    return;
  }

  form_waiter_ =
      ChangePasswordFormWaiter::Builder(
          web_contents_, client_,
          base::BindOnce(
              &ChangePasswordFormFinder::OnChangePasswordFormFoundAfterClick,
              weak_ptr_factory_.GetWeakPtr()))
          .Build();
}

void ChangePasswordFormFinder::OnChangePasswordFormFoundAfterClick(
    password_manager::PasswordFormManager* form_manager) {
  CHECK(form_manager);

  form_waiter_.reset();
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_PASSWORD_CHANGE_SUBSEQUENT_FORM_WAITING_RESULT,
        form_manager);
  }
  std::move(callback_).Run(form_manager);
}

void ChangePasswordFormFinder::OnFormNotFound() {
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogMessage(Logger::STRING_AUTOMATED_PASSWORD_CHANGE_FORM_NOT_FOUND);
  }
  logs_uploader_->FormNotDetectedAfterOpening();

  CHECK(callback_);
  std::move(callback_).Run(nullptr);
}
