// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {
blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  return optimization_guide::DefaultAIPageContentOptions(
      /* on_critical_path =*/false);
}

using autofill::SavePasswordProgressLogger;
using password_manager::BrowserSavePasswordProgressLogger;
using QualityStatus = optimization_guide::proto::
    PasswordChangeQuality_StepQuality_SubmissionStatus;

void LogMessage(password_manager::PasswordManagerClient* client,
                autofill::SavePasswordProgressLogger::StringID message_id) {
  if (client && client->GetCurrentLogManager() &&
      client->GetCurrentLogManager()->IsLoggingActive()) {
    BrowserSavePasswordProgressLogger(client->GetCurrentLogManager())
        .LogMessage(message_id);
  }
}

void LogBoolean(password_manager::PasswordManagerClient* client,
                autofill::SavePasswordProgressLogger::StringID message_id,
                bool value) {
  if (client && client->GetCurrentLogManager() &&
      client->GetCurrentLogManager()->IsLoggingActive()) {
    BrowserSavePasswordProgressLogger(client->GetCurrentLogManager())
        .LogBoolean(message_id, value);
  }
}

void LogNumber(password_manager::PasswordManagerClient* client,
               autofill::SavePasswordProgressLogger::StringID message_id,
               int error_enum) {
  if (client && client->GetCurrentLogManager() &&
      client->GetCurrentLogManager()->IsLoggingActive()) {
    BrowserSavePasswordProgressLogger(client->GetCurrentLogManager())
        .LogNumber(message_id, error_enum);
  }
}

}  // namespace

LoginStateChecker::LoginStateChecker(
    content::WebContents* web_contents,
    ModelQualityLogsUploader* logs_uploader,
    password_manager::PasswordManagerClient* client,
    LoginStateResultCallback callback)
    : content::WebContentsObserver(web_contents),
      logs_uploader_(CHECK_DEREF(logs_uploader)),
      client_(client),
      result_check_callback_(std::move(callback)) {
  CheckLoginState();
}

LoginStateChecker::~LoginStateChecker() = default;

bool LoginStateChecker::ReachedAttemptsLimit() const {
  return state_checks_count_ >= kMaxLoginChecks;
}

void LoginStateChecker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  capturer_.reset();
  CheckLoginState();
}

void LoginStateChecker::TerminateLoginChecks() {
  SetLoginCheckQuality(IsLoggedIn(false));
  state_checks_count_ = kMaxLoginChecks;
  result_check_callback_.Run(false);
}

void LoginStateChecker::SetLoginCheckQuality(IsLoggedIn is_logged_in) {
  if (is_logged_in.value()) {
    logs_uploader_->SetLoggedInCheckQuality(
        state_checks_count_,
        QualityStatus::
            PasswordChangeQuality_StepQuality_SubmissionStatus_ACTION_SUCCESS);
    return;
  }

  QualityStatus quality_status;
  if (ReachedAttemptsLimit()) {
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_FAILURE_STATUS;
  } else {
    // If the login check terminated before the maximum attempts were reached,
    // it indicates an unexpected state and a lack of a model response.
    quality_status = QualityStatus::
        PasswordChangeQuality_StepQuality_SubmissionStatus_UNEXPECTED_STATE;
  }
  logs_uploader_->SetLoggedInCheckQuality(state_checks_count_, quality_status);
}

void LoginStateChecker::CheckLoginState() {
  LogMessage(client_,
             SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_STARTED);
  // Checks if the maximum number of attempts has been reached.
  if (ReachedAttemptsLimit()) {
    LogMessage(client_, SavePasswordProgressLogger::
                            STRING_LOGIN_STATE_CHECK_MAX_ATTEMPTS_REACHED);
    TerminateLoginChecks();
    return;
  }

  // Clear previously captured page content.
  cached_page_content_ = std::nullopt;

  capturer_ = std::make_unique<AnnotatedPageContentCapturer>(
      web_contents(), GetAIPageContentOptions(),
      base::BindRepeating(&LoginStateChecker::OnPageContentReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

OptimizationGuideKeyedService* LoginStateChecker::GetOptimizationService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
}

void LoginStateChecker::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> content) {
  CHECK(content);
  if (is_request_in_flight_) {
    cached_page_content_ = std::move(content);
    return;
  }

  is_request_in_flight_ = true;
  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(optimization_guide::proto::PasswordChangeRequest::FlowStep::
                       PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP);
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(content->proto);

  LogMessage(client_,
             SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_REQUEST_SENT);
  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request, /*execution_timeout=*/std::nullopt,
      base::BindOnce(&LoginStateChecker::OnExecutionResponseCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LoginStateChecker::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  is_request_in_flight_ = false;
  // Increase the count of login checks.
  state_checks_count_++;

  LogMessage(
      client_,
      SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_RESPONSE_RECEIVED);
  if (!execution_result.response.has_value()) {
    LogNumber(client_,
              SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_SERVER_ERROR,
              static_cast<int>(execution_result.response.error().error()));
    TerminateLoginChecks();
    return;
  }

  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PasswordChangeResponse>(
          execution_result.response.value());
  if (!response) {
    LogMessage(client_,
               SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_FAILURE);
    TerminateLoginChecks();
    return;
  }

  bool is_logged_in = response->is_logged_in_data().is_logged_in();
  // If the login state is false, a subsequent retry will override the
  // quality state with either an unexpected or failure status.
  SetLoginCheckQuality(IsLoggedIn(true));

  LogBoolean(client_,
             SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_RESULT,
             is_logged_in);

  if (cached_page_content_.has_value() && !is_logged_in &&
      !ReachedAttemptsLimit()) {
    OnPageContentReceived(std::move(cached_page_content_));
    // Clear the page content to ensure that this check doesn't pass next time,
    // which would lead to a request with empty page content.
    cached_page_content_ = std::nullopt;
  }

  result_check_callback_.Run(is_logged_in);
}
