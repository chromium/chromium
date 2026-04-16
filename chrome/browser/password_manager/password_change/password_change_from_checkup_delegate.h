// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filling_submission_helper.h"
#include "url/gurl.h"

class ChangePasswordFormWaiter;

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialUIEntry;
class PasswordFormManager;
class PasswordManagerClient;
}  // namespace password_manager

namespace glic {
class GlicKeyedService;
}

// Handles a password change flow leveraging Gemini in Chrome.
// This flow is initiated for a specific credential from the Password Checkup
// page.
class PasswordChangeFromCheckupDelegate {
 public:
  explicit PasswordChangeFromCheckupDelegate(
      password_manager::PasswordManagerClient* client);
  ~PasswordChangeFromCheckupDelegate();

  void StartPasswordChangeFlow(
      const password_manager::CredentialUIEntry& credential,
      base::WeakPtr<content::WebContents> web_contents);

#if defined(UNIT_TEST)
  std::optional<actor::ActorTask::State> GetFindFormTaskState() const {
    return find_form_task_state_;
  }

#endif

 private:
  void AutoSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      actor::ToolDelegate::CredentialSelectedCallback callback);

  glic::GlicKeyedService* GetGlicService();

  void OnFindFormTaskStateChanged(actor::ActorTask& task);

  void OnChangePasswordFormManagerFound(
      password_manager::PasswordFormManager* form_manager);
  void OnChangePasswordFormSubmitted(
      ChangePasswordFormFillingSubmissionHelper::SubmissionResult result);

  void OnVerificationTaskStateChanged(actor::ActorTask& task);
  void OnVerificationTimeout();
  void HandleMaybeSuccessfulPasswordChange();
  void RegisterAutoSelectCredential(actor::ActorTask& task);
  void InvokeVerificationFlow(std::string post_submission_prompt);

  base::WeakPtr<content::WebContents> originator_;
  raw_ptr<password_manager::PasswordManagerClient> client_;
  base::WeakPtr<content::WebContents> actuation_web_contents_;

  std::u16string username_;
  std::u16string current_password_;
  GURL credential_url_;

  std::optional<actor::TaskId> find_form_task_id_;

  base::CallbackListSubscription actor_task_state_subscription_;

  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper> submission_helper_;
  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

  std::optional<actor::ActorTask::State> find_form_task_state_ = std::nullopt;

  std::optional<actor::TaskId> verification_task_id_;
  std::unique_ptr<password_manager::PasswordFormManager> saved_form_manager_;
  bool verification_task_created_ = false;
  base::OneShotTimer verification_timer_;

  base::WeakPtrFactory<PasswordChangeFromCheckupDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
