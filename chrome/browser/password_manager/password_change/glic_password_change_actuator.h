// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_GLIC_PASSWORD_CHANGE_ACTUATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_GLIC_PASSWORD_CHANGE_ACTUATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/password_manager/password_change/change_password_form_filler.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace glic {
class GlicInstance;
class GlicKeyedService;
}  // namespace glic

namespace password_manager {
class PasswordFormManager;
}  // namespace password_manager

class ChangePasswordFormWaiter;
class Profile;

// Actuator implementation that uses Gemini in Chrome (Glic) and Actor for
// password change navigation, form filling, and submission verification.
class GlicPasswordChangeActuator : public PasswordChangeActuator {
 public:
  GlicPasswordChangeActuator(password_manager::StoredCredential credential,
                             content::WebContents* originator,
                             Profile* profile,
                             GURL change_password_url = GURL());
  ~GlicPasswordChangeActuator() override;

  GlicPasswordChangeActuator(const GlicPasswordChangeActuator&) = delete;
  GlicPasswordChangeActuator& operator=(const GlicPasswordChangeActuator&) =
      delete;

  // PasswordChangeActuator:
  void Start() override;
  void Cancel() override;
  content::WebContents* GetExecutorWebContents() const override;
  void OpenPasswordChangeTab(content::WebContents* originator) override;
  std::u16string GetGeneratedPassword() const override;
  void AddObserver(PasswordChangeActuator::Observer* observer) override;
  void RemoveObserver(PasswordChangeActuator::Observer* observer) override;

 private:
  glic::GlicKeyedService* GetGlicService();

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnFindFormTaskStateChanged(actor::ActorTask& task);
  void OnChangePasswordFormManagerFound(
      password_manager::PasswordFormManager* form_manager);
  void OnChangePasswordFormFilled(
      ChangePasswordFormFiller::FillingResult result);
  void InvokeVerificationFlow(std::string post_submission_prompt);
  void OnVerificationTaskStateChanged(actor::ActorTask& task);
  void OnVerificationTimeout();
  void HandleMaybeSuccessfulPasswordChange();
  void CloseGlicSession();
  void ResetInternalState(actor::ActorTask::StoppedReason stop_reason);
  void NotifyStateChanged(PasswordChangeActuator::State new_state);

  const GURL change_password_url_;
  const password_manager::StoredCredential credential_;
  std::u16string generated_password_;

  base::WeakPtr<content::WebContents> originator_;
  const raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtr<content::WebContents> actuation_web_contents_;
  base::WeakPtr<glic::GlicInstance> glic_instance_;
  base::CallbackListSubscription tab_will_detach_subscription_;

  std::optional<actor::TaskId> find_form_task_id_;
  base::CallbackListSubscription actor_task_state_subscription_;

  std::unique_ptr<ChangePasswordFormFiller> form_filler_;
  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

  std::optional<actor::TaskId> verification_task_id_;
  std::unique_ptr<password_manager::PasswordFormManager> saved_form_manager_;
  bool verification_task_created_ = false;
  base::OneShotTimer verification_timer_;

  base::ObserverList<PasswordChangeActuator::Observer, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<GlicPasswordChangeActuator> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_GLIC_PASSWORD_CHANGE_ACTUATOR_H_
