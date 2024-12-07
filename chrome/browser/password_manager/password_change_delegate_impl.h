// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// This class controls password change process. Password change process starts
// immediately after creating the object.
class PasswordChangeDelegateImpl
    : public password_manager::PasswordFormManagerObserver,
      public PasswordChangeDelegate,
      public content::WebContentsObserver {
 public:
  PasswordChangeDelegateImpl(
      GURL change_password_url,
      std::u16string username,
      std::u16string password,
      content::WebContents* originator,
      base::RepeatingCallback<
          content::WebContents*(const GURL&, content::WebContents*)> callback);
  ~PasswordChangeDelegateImpl() override;

  PasswordChangeDelegateImpl(const PasswordChangeDelegateImpl&) = delete;
  PasswordChangeDelegateImpl& operator=(const PasswordChangeDelegateImpl&) =
      delete;

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  // PasswordChangeDelegate Impl
  bool IsPasswordChangeOngoing(content::WebContents* web_contents) override;
  State GetCurrentState() const override;
  void Stop() override;
  void SuccessfulSubmissionDetected(
      content::WebContents* web_contents) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // content::WebContentsObserver Impl
  void WebContentsDestroyed() override;

  // Updates `current_state_` and notifies `observers_`.
  void UpdateState(State new_state);

  void FillChangePasswordForm(
      password_manager::PasswordForm form,
      base::WeakPtr<password_manager::PasswordManagerDriver> driver);
  void ChangePasswordFormFilled(const autofill::FormData& submitted_form);

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;

  std::u16string generated_password_;

  base::WeakPtr<content::WebContents> originator_;
  base::WeakPtr<content::WebContents> executor_;

  State current_state_ = State::kWaitingForChangePasswordForm;

  // Form manager for displayed change password form.
  std::unique_ptr<password_manager::PasswordFormManager> form_manager_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<PasswordChangeDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_IMPL_H_
