// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/login_state_checker.h"

#include "base/functional/bind.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {
blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  return optimization_guide::DefaultAIPageContentOptions();
}

}  // namespace

LoginStateChecker::LoginStateChecker(content::WebContents* web_contents,
                                     LoginStateResultCallback callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {
  CheckLoginState();
}

LoginStateChecker::~LoginStateChecker() = default;

void LoginStateChecker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  capturer_.reset();
  CheckLoginState();
}

void LoginStateChecker::CheckLoginState() {
  CHECK(callback_);
  // Checks if the maximum number of attempts has been reached.
  if (state_checks_count_ >= LoginStateChecker::kMaxLoginChecks) {
    std::move(callback_).Run(false);
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
  CHECK(callback_);
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
  if (is_logged_in) {
    std::move(callback_).Run(true);
  }
}
