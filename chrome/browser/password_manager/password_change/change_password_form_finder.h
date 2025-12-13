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
#include "chrome/common/chrome_render_frame.mojom.h"
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

  ChangePasswordFormFinder(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      ModelQualityLogsUploader* logs_uploader,
      ChangePasswordFormWaiter::PasswordFormFoundCallback callback);

  ChangePasswordFormFinder(
      base::PassKey<class ChangePasswordFormFinderTest>,
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      ModelQualityLogsUploader* logs_uploader,
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
  void OnFormNotFoundInitially();
  void OnFormFoundInitially(
      password_manager::PasswordFormManager* form_manager);

  void OnPageContentReceived(
      optimization_guide::AIPageContentResultOrError content);

  OptimizationGuideKeyedService* GetOptimizationService();

  void OnExecutionResponseCallback(
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::PasswordChangeSubmissionLoggingData>
          logging_data);

  void OnButtonClicked(actor::mojom::ActionResultCode result);

  void OnChangePasswordFormFoundAfterClick(
      password_manager::PasswordFormManager* form_manager);
  void OnFormNotFound();

  const base::Time creation_time_;
  const raw_ptr<content::WebContents> web_contents_ = nullptr;
  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;
  raw_ptr<ModelQualityLogsUploader> logs_uploader_ = nullptr;

  ChangePasswordFormWaiter::PasswordFormFoundCallback callback_;

  base::OnceCallback<void(optimization_guide::OnAIPageContentDone)>
      capture_annotated_page_content_;

  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

  std::unique_ptr<ButtonClickHelper> click_helper_;

  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<ChangePasswordFormFinder> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_FINDER_H_
