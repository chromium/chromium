// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

namespace password_manager {
class PasswordFormManager;
}

namespace content {
class WebContents;
}

class ButtonClickHelper;

// Helper class which searches for a change password form, performs actuation
// when necessary. Invokes a callback with a form when it's found, or nullptr
// otherwise.
class ChangePasswordFormFinder {
 public:
  ChangePasswordFormFinder(
      content::WebContents* web_contents,
      ChangePasswordFormWaiter::PasswordFormFoundCallback callback);

  ChangePasswordFormFinder(
      base::PassKey<class ChangePasswordFormFinderTest>,
      content::WebContents* web_contents,
      ChangePasswordFormWaiter::PasswordFormFoundCallback callback,
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
          capture_annotated_page_content);

  ~ChangePasswordFormFinder();

#if defined(UNIT_TEST)
  void RespondWithFormNotFound() { std::move(callback_).Run(nullptr); }

  ChangePasswordFormWaiter* form_waiter() { return form_waiter_.get(); }
  ButtonClickHelper* click_helper() { return click_helper_.get(); }
#endif

 private:
  void OnInitialFormWaitingResult(
      password_manager::PasswordFormManager* form_manager);

  void OnPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> content);

  OptimizationGuideKeyedService* GetOptimizationService();

  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

#if !BUILDFLAG(IS_ANDROID)
  void OnButtonClicked(bool result);

  void OnSubsequentFormWaitingResult(
      password_manager::PasswordFormManager* form_manager);
#endif

  base::WeakPtr<content::WebContents> web_contents_;
  ChangePasswordFormWaiter::PasswordFormFoundCallback callback_;
  base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
      capture_annotated_page_content_;

  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ButtonClickHelper> click_helper_;
#endif

  base::WeakPtrFactory<ChangePasswordFormFinder> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_
