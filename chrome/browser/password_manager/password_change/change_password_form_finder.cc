// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

constexpr optimization_guide::proto::PasswordChangeRequest::FlowStep
    kOpenFormFlowStep = optimization_guide::proto::PasswordChangeRequest::
        FlowStep::PasswordChangeRequest_FlowStep_OPEN_FORM_STEP;

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  auto options = optimization_guide::DefaultAIPageContentOptions();
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  options->on_critical_path = true;
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
    const GURL& change_password_url,
    ChangePasswordFormFoundCallback callback,
    LoginFormFoundCallback login_form_found_callback)
    : web_contents_(web_contents),
      client_(client),
      logs_uploader_(logs_uploader),
      change_password_url_(change_password_url),
      callback_(std::move(callback)),
      login_form_found_callback_(std::move(login_form_found_callback)) {
  CHECK(logs_uploader_);
  capture_annotated_page_content_ =
      base::BindOnce(&optimization_guide::GetAIPageContent, web_contents,
                     GetAIPageContentOptions());
  form_waiter_ = std::make_unique<PasswordFormWaiter>(
      web_contents, client_,
      base::BindOnce(&ChangePasswordFormFinder::OnInitialFormWaitingResult,
                     weak_ptr_factory_.GetWeakPtr()));

  timeout_timer_.Start(FROM_HERE, kFormWaitingTimeout,
                       base::BindOnce(&ChangePasswordFormFinder::OnFormNotFound,
                                      weak_ptr_factory_.GetWeakPtr()));
}

ChangePasswordFormFinder::ChangePasswordFormFinder(
    base::PassKey<class ChangePasswordFormFinderTest>,
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    ModelQualityLogsUploader* logs_uploader,
    const GURL& change_password_url,
    ChangePasswordFormFoundCallback callback,
    LoginFormFoundCallback login_form_found_callback,
    base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
        capture_annotated_page_content)
    : ChangePasswordFormFinder(web_contents,
                               client,
                               logs_uploader,
                               change_password_url,
                               std::move(callback),
                               std::move(login_form_found_callback)) {
  capture_annotated_page_content_ = std::move(capture_annotated_page_content);
}

ChangePasswordFormFinder::~ChangePasswordFormFinder() = default;

void ChangePasswordFormFinder::OnInitialFormWaitingResult(
    PasswordFormWaiter::Result result) {
  CHECK(web_contents_);
  CHECK(callback_);
  form_waiter_.reset();

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_PASSWORD_CHANGE_INITIAL_FORM_WAITING_RESULT,
        result.change_password_form_manager);
  }

  // Change password form found, invoke callback immediately.
  if (result.change_password_form_manager) {
    logs_uploader_->MarkStepSkipped(kOpenFormFlowStep);
    std::move(callback_).Run(result.change_password_form_manager);
    return;
  }

  // Login form detected, refresh page and wait again. User hasn't fully signed
  // in.
  if (result.login_form_manager) {
    timeout_timer_.Reset();
    if (login_form_found_callback_) {
      std::move(login_form_found_callback_).Run();
    }

    web_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(change_password_url_));

    form_waiter_ = std::make_unique<PasswordFormWaiter>(
        web_contents_, client_,
        base::BindOnce(&ChangePasswordFormFinder::OnInitialFormWaitingResult,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Neither change password nor login form is detected. It's potentially a
  // settings page.
  CHECK(capture_annotated_page_content_);
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(&ChangePasswordFormFinder::OnPageContentReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> content) {
  CHECK(web_contents_);
  CHECK(callback_);

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
    // Button to click is missing when the login page is displayed. Instead of
    // failing immediately continue refreshing the page until timeout.
    if (response.value().open_form_data().page_type() ==
        optimization_guide::proto::OpenFormResponseData_PageType_LOG_IN_PAGE) {
      ProcessPasswordFormManagerOrRefresh({});
    } else {
      std::move(callback_).Run(nullptr);
    }
    return;
  }

  click_helper_ = std::make_unique<ButtonClickHelper>(
      web_contents_, dom_node_id,
      base::BindOnce(&ChangePasswordFormFinder::OnButtonClicked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnButtonClicked(bool result) {
  CHECK(web_contents_);
  CHECK(callback_);

  click_helper_.reset();

  if (!result) {
    logs_uploader_->OpenFormTargetElementNotFound();
    std::move(callback_).Run(nullptr);
    return;
  }

  form_waiter_ = std::make_unique<PasswordFormWaiter>(
      web_contents_, client_,
      base::BindOnce(&ChangePasswordFormFinder::OnSubsequentFormWaitingResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnSubsequentFormWaitingResult(
    PasswordFormWaiter::Result result) {
  password_manager::PasswordFormManager* form_manager =
      result.change_password_form_manager;

  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogBoolean(
        Logger::STRING_PASSWORD_CHANGE_SUBSEQUENT_FORM_WAITING_RESULT,
        form_manager);
  }
  if (!form_manager) {
    logs_uploader_->FormNotDetectedAfterOpening();
  }
  CHECK(callback_);
  std::move(callback_).Run(form_manager);
}

void ChangePasswordFormFinder::ProcessPasswordFormManagerOrRefresh(
    PasswordFormWaiter::Result result) {
  password_manager::PasswordFormManager* form_manager =
      result.change_password_form_manager;

  if (form_manager) {
    CHECK(callback_);
    std::move(callback_).Run(form_manager);
    return;
  }
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(change_password_url_));

  form_waiter_ = std::make_unique<PasswordFormWaiter>(
      web_contents_, client_,
      base::BindOnce(
          &ChangePasswordFormFinder::ProcessPasswordFormManagerOrRefresh,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnFormNotFound() {
  CHECK(callback_);
  std::move(callback_).Run(nullptr);
}
