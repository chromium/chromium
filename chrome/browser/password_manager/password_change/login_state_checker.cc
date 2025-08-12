// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "content/public/browser/web_contents.h"

LoginStateChecker::LoginStateChecker(content::WebContents* web_contents,
                                     LoginStateResultCallback callback)
    : LoginStateChecker(
          web_contents,
          base::BindOnce(&optimization_guide::GetAIPageContent,
                         web_contents,
                         optimization_guide::DefaultAIPageContentOptions()),
          std::move(callback)) {}

LoginStateChecker::LoginStateChecker(
    content::WebContents* web_contents,
    base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
        capture_annotated_page_content,
    LoginStateResultCallback callback)
    : web_contents_(web_contents),
      capture_annotated_page_content_(
          std::move(capture_annotated_page_content)),
      callback_(std::move(callback)) {
  CheckLoginState();
}

LoginStateChecker::~LoginStateChecker() = default;

void LoginStateChecker::CheckLoginState() {
  // TODO(crbug.com/436537301): Retry the login check after a navigation.
  CHECK(web_contents_);
  CHECK(callback_);
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(&LoginStateChecker::OnPageContentReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

OptimizationGuideKeyedService* LoginStateChecker::GetOptimizationService() {
  CHECK(web_contents_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
}

void LoginStateChecker::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> content) {
  if (!content || !web_contents_) {
    std::move(callback_).Run(false);
    return;
  }

  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(optimization_guide::proto::PasswordChangeRequest::FlowStep::
                       PasswordChangeRequest_FlowStep_IS_LOGGED_IN_STEP);
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(content->proto);

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

  bool is_logged_in = response->is_logged_in_data().is_logged_in();
  std::move(callback_).Run(is_logged_in);
}
