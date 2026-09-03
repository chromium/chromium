// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/password_manager/password_change/password_change_actuator.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace content {
class WebContents;
}

namespace password_manager {
struct StoredCredential;
}  // namespace password_manager

// Handles a password change flow leveraging Gemini in Chrome.
// This flow is initiated for a specific credential from the Password Checkup
// page.
class PasswordChangeFromCheckupDelegate
    : public PasswordChangeActuator::Observer {
 public:
  // Enumerates possible states the automatic change flow could be in
  enum class PasswordAutomaticChangeState {
    kInactive,
    kAttemptingSignIn,
    kChangingPassword,
    kConfirmingChangedPassword,
    kPasswordChangedSuccessfully,
    kError
  };

  using StateChangeCallback =
      base::RepeatingCallback<void(PasswordAutomaticChangeState)>;

  PasswordChangeFromCheckupDelegate();
  ~PasswordChangeFromCheckupDelegate() override;

  void StartPasswordChangeFlow(
      password_manager::StoredCredential credential,
      base::WeakPtr<content::WebContents> web_contents,
      StateChangeCallback callback = base::NullCallback());

  void Stop(actor::ActorTask::StoppedReason stop_reason);

  // PasswordChangeActuator::Observer:
  void OnActuationStateChanged(
      PasswordChangeActuator::State new_state) override;

  std::u16string generated_password() const;

  void OpenActuationTab();

  base::WeakPtr<PasswordChangeFromCheckupDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

#if defined(UNIT_TEST)
  void set_actuator_for_testing(
      std::unique_ptr<PasswordChangeActuator> actuator) {
    actuator_ = std::move(actuator);
    actuator_->AddObserver(this);
  }
#endif

  PasswordChangeActuator* get_actuator_for_testing() { return actuator_.get(); }

 private:
  std::unique_ptr<PasswordChangeActuator> actuator_;
  StateChangeCallback state_change_callback_;
  base::WeakPtr<content::WebContents> web_contents_;
  base::WeakPtrFactory<PasswordChangeFromCheckupDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
