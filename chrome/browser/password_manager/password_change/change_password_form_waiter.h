// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_

#include "base/timer/timer.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "content/public/browser/web_contents_observer.h"

namespace password_manager {
class PasswordFormManager;
}
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
  static constexpr base::TimeDelta kChangePasswordFormWaitingTimeout =
      base::Seconds(2);
  using PasswordFormFoundCallback =
      base::OnceCallback<void(password_manager::PasswordFormManager*)>;

  ChangePasswordFormWaiter(content::WebContents* web_contents,
                           PasswordFormFoundCallback callback);

  ~ChangePasswordFormWaiter() override;

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  //  content::WebContentsObserver
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  void OnTimeout();

  base::OneShotTimer timeout_timer_;
  base::WeakPtr<content::WebContents> web_contents_;
  PasswordFormFoundCallback callback_;

  base::WeakPtrFactory<ChangePasswordFormWaiter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
