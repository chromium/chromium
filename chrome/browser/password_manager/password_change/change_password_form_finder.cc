// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_finder.h"

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents.h"

namespace {

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  auto options = blink::mojom::AIPageContentOptions::New();
  // WebContents where password change is happening is hidden, and renderer
  // won't capture a snapshot unless it becomes visible again or
  // on_critical_path is set to true.
  options->on_critical_path = true;
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
  std::move(callback_).Run(nullptr);
}
