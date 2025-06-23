// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
}  // namespace password_manager

class ChangePasswordFormFillingSubmissionHelper;
class ChangePasswordFormFinder;
class ModelQualityLogsUploader;
class PasswordChangeUIController;
class Profile;

// This class controls password change process including acceptance of privacy
// notice, opening of a new tab, navigation to the change password url, password
// generation and form submission.
class PasswordChangeDelegateImpl : public PasswordChangeDelegate {
 public:
  static constexpr char kFinalPasswordChangeStatusHistogram[] =
      "PasswordManager.FinalPasswordChangeStatus";

  PasswordChangeDelegateImpl(GURL change_password_url,
                             std::u16string username,
                             std::u16string password,
                             tabs::TabInterface* tab_interface);
  ~PasswordChangeDelegateImpl() override;

  PasswordChangeDelegateImpl(const PasswordChangeDelegateImpl&) = delete;
  PasswordChangeDelegateImpl& operator=(const PasswordChangeDelegateImpl&) =
      delete;

  base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() override;

#if defined(UNIT_TEST)
  ChangePasswordFormFinder* form_finder() { return form_finder_.get(); }
  content::WebContents* executor() { return executor_.get(); }
  PasswordChangeUIController* ui_controller() { return ui_controller_.get(); }
  void SetCustomUIController(
      std::unique_ptr<PasswordChangeUIController> controller) {
    ui_controller_ = std::move(controller);
  }
#endif

 private:
  // PasswordChangeDelegate Impl
  void StartPasswordChangeFlow() override;
  void CancelPasswordChangeFlow() override;
  bool IsPasswordChangeOngoing(content::WebContents* web_contents) override;
  State GetCurrentState() const override;
  void Stop() override;
  void OpenPasswordChangeTab() override;
  void OpenPasswordDetails() override;
  void OnPasswordFormSubmission(content::WebContents* web_contents) override;
  void OnOtpFieldDetected(content::WebContents* web_contents) override;
  void OnPrivacyNoticeAccepted() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::u16string GetDisplayOrigin() const override;
  const std::u16string& GetUsername() const override;
  const std::u16string& GetGeneratedPassword() const override;

  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);

  // Updates `current_state_` and notifies `observers_`.
  void UpdateState(State new_state);

  void OnPasswordChangeFormFound(
      password_manager::PasswordFormManager* form_manager);

  void OnChangeFormSubmissionVerified(bool result);

  bool IsPrivacyNoticeAcknowledged() const;

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;

  std::u16string generated_password_;

  raw_ptr<content::WebContents> originator_;
  std::unique_ptr<content::WebContents> executor_;

  const raw_ptr<Profile> profile_;

  // Helper class which uploads model quality logs.
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;

  State current_state_ = static_cast<State>(-1);

  // Helper class which looks for a change password form.
  std::unique_ptr<ChangePasswordFormFinder> form_finder_;

  // Helper class which submits a form and verifies submission.
  std::unique_ptr<ChangePasswordFormFillingSubmissionHelper>
      submission_verifier_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::Time flow_start_time_;

  // The controller for password change views.
  std::unique_ptr<PasswordChangeUIController> ui_controller_;

  // URL of the last committed page in `originator_` on the password change flow
  // startup.
  const GURL last_committed_url_;

  base::CallbackListSubscription tab_will_detach_subscription_;

  base::WeakPtrFactory<PasswordChangeDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
