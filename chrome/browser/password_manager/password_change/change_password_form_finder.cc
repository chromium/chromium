// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"

#include "base/functional/bind.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/proto/features/password_change_submission.pb.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/password_change/button_click_helper.h"
#endif

namespace {

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  auto options = blink::mojom::AIPageContentOptions::New();
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  options->on_critical_path = true;
  options->enable_experimental_actionable_data = true;
  return options;
}

}  // namespace

ChangePasswordFormFinder::ChangePasswordFormFinder(
    content::WebContents* web_contents,
    ChangePasswordFormWaiter::PasswordFormFoundCallback callback)
    : web_contents_(web_contents->GetWeakPtr()),
      callback_(std::move(callback)) {
  capture_annotated_page_content_ =
      base::BindOnce(&optimization_guide::GetAIPageContent, web_contents,
                     GetAIPageContentOptions());
  form_waiter_ = std::make_unique<ChangePasswordFormWaiter>(
      web_contents,
      base::BindOnce(&ChangePasswordFormFinder::OnInitialFormWaitingResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

ChangePasswordFormFinder::ChangePasswordFormFinder(
    base::PassKey<class ChangePasswordFormFinderTest>,
    content::WebContents* web_contents,
    ChangePasswordFormWaiter::PasswordFormFoundCallback callback,
    base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
        capture_annotated_page_content)
    : ChangePasswordFormFinder(web_contents, std::move(callback)) {
  capture_annotated_page_content_ = std::move(capture_annotated_page_content);
}

ChangePasswordFormFinder::~ChangePasswordFormFinder() = default;

void ChangePasswordFormFinder::OnInitialFormWaitingResult(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();
  if (form_manager) {
    std::move(callback_).Run(form_manager);
    return;
  }

  // The tab closed, fail immediately.
  if (!web_contents_) {
    std::move(callback_).Run(nullptr);
    return;
  }

  CHECK(capture_annotated_page_content_);
  std::move(capture_annotated_page_content_)
      .Run(base::BindOnce(&ChangePasswordFormFinder::OnPageContentReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> content) {
  if (!content || !web_contents_) {
    std::move(callback_).Run(nullptr);
    return;
  }
  // TODO(crbug.com/407486413): Check if it's a settings page and try to find a
  // button which opens a change-pwd form.
  optimization_guide::proto::PasswordChangeRequest request;
  request.set_step(optimization_guide::proto::PasswordChangeRequest::FlowStep::
                       PasswordChangeRequest_FlowStep_OPEN_FORM_STEP);
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(content->proto);
  optimization_guide::ExecuteModelWithLogging(
      GetOptimizationService(),
      optimization_guide::ModelBasedCapabilityKey::kPasswordChangeSubmission,
      request, /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ChangePasswordFormFinder::OnExecutionResponseCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

OptimizationGuideKeyedService*
ChangePasswordFormFinder::GetOptimizationService() {
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void ChangePasswordFormFinder::OnExecutionResponseCallback(
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<
        optimization_guide::proto::PasswordChangeSubmissionLoggingData>
        logging_data) {
  if (!web_contents_ || !execution_result.response.has_value()) {
    // TODO(crbug.com/407503334): Record metrics here.
    std::move(callback_).Run(nullptr);
    return;
  }
  std::optional<optimization_guide::proto::PasswordChangeResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::PasswordChangeResponse>(
          execution_result.response.value());
  if (!response) {
    // TODO(crbug.com/407503334): Record metrics here.
    std::move(callback_).Run(nullptr);
    return;
  }

  int dom_node_id = response.value().open_form_data().dom_node_id_to_click();
  if (!dom_node_id) {
    // TODO(crbug.com/407503334): Record metrics here.
    std::move(callback_).Run(nullptr);
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  click_helper_ = std::make_unique<ButtonClickHelper>(
      web_contents_.get(), dom_node_id,
      base::BindOnce(&ChangePasswordFormFinder::OnButtonClicked,
                     weak_ptr_factory_.GetWeakPtr()));
#else
  std::move(callback_).Run(nullptr);
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ChangePasswordFormFinder::OnButtonClicked(bool result) {
  click_helper_.reset();

  if (!result || !web_contents_) {
    // TODO(crbug.com/407503334): Record metrics here.
    std::move(callback_).Run(nullptr);
    return;
  }

  form_waiter_ = std::make_unique<ChangePasswordFormWaiter>(
      web_contents_.get(),
      base::BindOnce(&ChangePasswordFormFinder::OnSubsequentFormWaitingResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChangePasswordFormFinder::OnSubsequentFormWaitingResult(
    password_manager::PasswordFormManager* form_manager) {
  // TODO(crbug.com/407503334): Record metrics here.
  std::move(callback_).Run(form_manager);
}
#endif
