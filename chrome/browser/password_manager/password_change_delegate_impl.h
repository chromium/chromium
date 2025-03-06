// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace content {
enum class Visibility;
class PageNavigator;
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
}  // namespace password_manager

class ChangeFormSubmissionVerifier;
class ChangePasswordFormWaiter;

// This class controls password change process including acceptance of privacy
// notice, opening of a new tab, navigation to the change password url, password
// generation and form submission.
class PasswordChangeDelegateImpl : public PasswordChangeDelegate,
                                   public content::WebContentsObserver {
 public:
  static constexpr char kFinalPasswordChangeStatusHistogram[] =
      "PasswordManager.FinalPasswordChangeStatus";
  static constexpr char kWasPasswordChangeNewTabFocused[] =
      "PasswordManager.WasPasswordChangeNewTabFocused";

  PasswordChangeDelegateImpl(GURL change_password_url,
                             std::u16string username,
                             std::u16string password,
                             content::WebContents* originator);
  ~PasswordChangeDelegateImpl() override;

  PasswordChangeDelegateImpl(const PasswordChangeDelegateImpl&) = delete;
  PasswordChangeDelegateImpl& operator=(const PasswordChangeDelegateImpl&) =
      delete;

  // Sets `kOfferingPasswordChange` state and triggers the leak check bubble.
  void OfferPasswordChangeUi();

  base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() override;

#if defined(UNIT_TEST)
  void SetNavigator(content::PageNavigator* navigator) {
    test_navigator_ = navigator;
  }
#endif

 private:
  // PasswordChangeDelegate Impl
  void StartPasswordChangeFlow() override;
  bool IsPasswordChangeOngoing(content::WebContents* web_contents) override;
  State GetCurrentState() const override;
  void Stop() override;
  void Restart() override;
#if !BUILDFLAG(IS_ANDROID)
  void OpenPasswordChangeTab() override;
#endif
  void OnPasswordFormSubmission(content::WebContents* web_contents) override;
  void OnPrivacyNoticeAccepted() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::u16string GetDisplayOrigin() const override;
  const std::u16string& GetUsername() const override;
  const std::u16string& GetGeneratedPassword() const override;

  // content::WebContentsObserver Impl
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Opens the tab for password change and start looking for change password
  // form.
  void StartPasswordChange();

  // Updates `current_state_` and notifies `observers_`.
  void UpdateState(State new_state);

  void OnPasswordChangeFormParsed(
      password_manager::PasswordFormManager* form_manager);

  void OnChangeFormSubmissionVerified(bool result);

  bool IsPrivacyNoticeAcknowledged() const;

  content::PageNavigator* GetNavigator();

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;

  std::u16string generated_password_;

  base::WeakPtr<content::WebContents> originator_;
  base::WeakPtr<content::WebContents> executor_;

  State current_state_ = static_cast<State>(-1);

  // Class which awaits for change password form to appear.
  std::unique_ptr<ChangePasswordFormWaiter> form_waiter_;

  // Helper class which submits a form and verifies submission.
  std::unique_ptr<ChangeFormSubmissionVerifier> submission_verifier_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::Time flow_start_time_;
  bool was_password_change_tab_focused_ = false;

  // Allows mocking opening of a URL in tests.
  raw_ptr<content::PageNavigator> test_navigator_;

  base::WeakPtrFactory<PasswordChangeDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
