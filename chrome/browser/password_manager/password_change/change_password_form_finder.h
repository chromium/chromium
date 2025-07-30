// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

namespace password_manager {
class PasswordFormManager;
class PasswordManagerClient;
}

namespace content {
class WebContents;
}

class ButtonClickHelper;
class ModelQualityLogsUploader;

// Helper class which searches for a change password form, performs actuation
// when necessary. Invokes a callback with a form when it's found, or nullptr
// otherwise.
class ChangePasswordFormFinder {
 public:
  // Maximum waiting time for a change password form to appear.
  static constexpr base::TimeDelta kFormWaitingTimeout = base::Seconds(30);
  using ChangePasswordFormFoundCallback =
      base::OnceCallback<void(password_manager::PasswordFormManager*)>;
  using LoginFormFoundCallback = base::OnceCallback<void()>;

  ChangePasswordFormFinder(content::WebContents* web_contents,
                           password_manager::PasswordManagerClient* client,
                           ModelQualityLogsUploader* logs_uploader,
                           const GURL& change_password_url,
                           ChangePasswordFormFoundCallback callback,
                           LoginFormFoundCallback login_form_found_callback);

  ChangePasswordFormFinder(
      base::PassKey<class ChangePasswordFormFinderTest>,
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      ModelQualityLogsUploader* logs_uploader,
      const GURL& change_password_url,
      ChangePasswordFormFoundCallback callback,
      LoginFormFoundCallback login_form_found_callback,
      base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
          capture_annotated_page_content);

  ~ChangePasswordFormFinder();

#if defined(UNIT_TEST)
  void RespondWithFormNotFound() { std::move(callback_).Run(nullptr); }

  PasswordFormWaiter* form_waiter() { return form_waiter_.get(); }
  ButtonClickHelper* click_helper() { return click_helper_.get(); }
#endif

 private:
  void OnInitialFormWaitingResult(PasswordFormWaiter::Result result);

  void OnPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> content);

  OptimizationGuideKeyedService* GetOptimizationService();

  void OnExecutionResponseCallback(
      base::Time request_time,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  void OnButtonClicked(bool result);

  void OnSubsequentFormWaitingResult(PasswordFormWaiter::Result result);

  // Invokes `callback_` if `form_manager` is present, navigates to the
  // `change_password_url_` and awaits for change password form again otherwise.
  void ProcessPasswordFormManagerOrRefresh(PasswordFormWaiter::Result result);
  void OnFormNotFound();

  const raw_ptr<content::WebContents> web_contents_ = nullptr;
  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;
  raw_ptr<ModelQualityLogsUploader> logs_uploader_ = nullptr;
  const GURL change_password_url_;

  ChangePasswordFormFoundCallback callback_;
  LoginFormFoundCallback login_form_found_callback_;

  base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
      capture_annotated_page_content_;

  std::unique_ptr<PasswordFormWaiter> form_waiter_;

  std::unique_ptr<ButtonClickHelper> click_helper_;

  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<ChangePasswordFormFinder> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_
