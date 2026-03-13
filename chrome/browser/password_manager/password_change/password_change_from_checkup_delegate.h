// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "url/gurl.h"

class ChangePasswordFormWaiter;

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialUIEntry;
class PasswordFormManager;
}  // namespace password_manager

namespace glic {
class GlicKeyedService;
}

// Handles a password change flow leveraging Gemini in Chrome.
// This flow is initiated for a specific credential from the Password Checkup
// page.
class PasswordChangeFromCheckupDelegate {
 public:
  PasswordChangeFromCheckupDelegate();
  ~PasswordChangeFromCheckupDelegate();

  void StartPasswordChangeFlow(
      const password_manager::CredentialUIEntry& credential,
      base::WeakPtr<content::WebContents> web_contents);

#if defined(UNIT_TEST)
  bool HasActorTaskSubscriptionForTesting() const {
    return !!actor_task_state_subscription_;
  }
  // TODO(crbug.com/485620841): Check a different state when the form is
  // submitted.
  bool IsCleanedUpAfterTaskFinishedForTesting() const {
    return !submission_helper_;
  }

#endif

 private:
  void OnPromptReady(GURL credential_url, std::string prompt);

  glic::GlicKeyedService* GetGlicService();

  void OnActorTaskStateChanged(actor::TaskId task_id,
                               actor::ActorTask::State state);

  void OnChangePasswordFormManagerFound(
      password_manager::PasswordFormManager* form_manager);
  void OnChangePasswordFormSubmitted(
      ChangePasswordFormFillingSubmissionHelper::SubmissionResult result);

  base::WeakPtr<content::WebContents> originator_;
  base::WeakPtr<content::WebContents> actuation_web_contents_;

  std::u16string username_;
  std::u16string current_password_;
  std::optional<actor::TaskId> actor_task_id_;

  base::CallbackListSubscription actor_task_state_subscription_;

  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper> submission_helper_;
  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

  base::WeakPtrFactory<PasswordChangeFromCheckupDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
