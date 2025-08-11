// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "content/public/browser/web_contents_observer.h"

namespace password_manager {
class PasswordFormManager;
class PasswordManagerClient;
}  // namespace password_manager

namespace content {
class WebContents;
}

// Helper object which waits for change password parsing, invokes callback on
// completion. If form isn't found withing
// `kChangePasswordFormWaitingTimeout` after WebContents finished loading
// callback is invoked with nullptr.
class ChangePasswordFormWaiter
    : public password_manager::PasswordFormManagerObserver,
      public content::WebContentsObserver {
 public:
  // Timeout for change password form await time after the page is loaded.
  static constexpr base::TimeDelta kChangePasswordFormWaitingTimeout =
      base::Seconds(2);
  using PasswordFormFoundCallback =
      base::OnceCallback<void(password_manager::PasswordFormManager*)>;

  ChangePasswordFormWaiter(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      PasswordFormFoundCallback callback,
      base::TimeDelta timeout = kChangePasswordFormWaitingTimeout,
      const std::vector<autofill::FieldRendererId>& fields_to_ignore = {});

  ~ChangePasswordFormWaiter() override;

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  //  content::WebContentsObserver
  void DidStartLoading() override;
  void DidStopLoading() override;

  void OnTimeout();

  const base::TimeDelta timeout_;
  base::OneShotTimer timeout_timer_;
  const raw_ptr<password_manager::PasswordManagerClient> client_;
  PasswordFormFoundCallback callback_;

  // new_password_element_renderer_ids which ChangePasswordFormWaiter should
  // ignore. This helps avoid detecting the same change password form over and
  // over again.
  std::vector<autofill::FieldRendererId> fields_to_ignore_;

  base::WeakPtrFactory<ChangePasswordFormWaiter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
