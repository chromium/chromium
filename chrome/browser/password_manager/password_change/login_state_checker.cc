// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "base/functional/bind.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
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
  return optimization_guide::DefaultAIPageContentOptions();
}

using autofill::SavePasswordProgressLogger;
using password_manager::BrowserSavePasswordProgressLogger;

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

}  // namespace

LoginStateChecker::LoginStateChecker(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    InitialLoginCheckFailedCallback first_check_callback,
    LoginStateResultCallback final_check_callback)
    : content::WebContentsObserver(web_contents),
      client_(client),
      first_check_callback_(std::move(first_check_callback)),
      final_check_callback_(std::move(final_check_callback)) {
  CheckLoginState();
}

LoginStateChecker::~LoginStateChecker() = default;

void LoginStateChecker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  capturer_.reset();
  CheckLoginState();
}

void LoginStateChecker::TerminateLoginChecks() {
  std::move(final_check_callback_).Run(false);
}

void LoginStateChecker::CheckLoginState() {
  CHECK(final_check_callback_);

  LogMessage(client_,
             SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_STARTED);
  // Checks if the maximum number of attempts has been reached.
  if (state_checks_count_ >= LoginStateChecker::kMaxLoginChecks) {
    LogMessage(client_, SavePasswordProgressLogger::
                            STRING_LOGIN_STATE_CHECK_MAX_ATTEMPTS_REACHED);
    TerminateLoginChecks();
    return;
  }

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
  // Increase the count of login checks.
  state_checks_count_++;
  if (!content) {
    LogMessage(client_,
               SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_NO_CONTENT);
    TerminateLoginChecks();
    return;
  }

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
  CHECK(final_check_callback_);
  LogMessage(
      client_,
      SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_RESPONSE_RECEIVED);
  if (!execution_result.response.has_value()) {
    LogMessage(client_,
               SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_FAILURE);
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
  LogBoolean(client_,
             SavePasswordProgressLogger::STRING_LOGIN_STATE_CHECK_RESULT,
             is_logged_in);
  if (is_logged_in) {
    std::move(final_check_callback_).Run(true);
  } else if (first_check_callback_) {
    std::move(first_check_callback_).Run();
  }
}
