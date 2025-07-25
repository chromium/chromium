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

// Helper object which waits for form password parsing, invokes callback on
// completion. Invokes callback with change password form immediately when
// detected. Login form or empty result is invoked after
// `kChangePasswordFormWaitingTimeout`. Timeout starts only after the page has
// finished loading.
class PasswordFormWaiter : public password_manager::PasswordFormManagerObserver,
                           public content::WebContentsObserver {
 public:
  // Timeout for change password form await time after the page is loaded.
  static constexpr base::TimeDelta kChangePasswordFormWaitingTimeout =
      base::Seconds(2);

  struct Result {
    raw_ptr<password_manager::PasswordFormManager>
        change_password_form_manager = nullptr;
    raw_ptr<password_manager::PasswordFormManager> login_form_manager = nullptr;

    bool operator==(const Result&) const = default;
  };

  using PasswordFormFoundCallback = base::OnceCallback<void(Result)>;

  PasswordFormWaiter(content::WebContents* web_contents,
                     password_manager::PasswordManagerClient* client,
                     PasswordFormFoundCallback callback);

  ~PasswordFormWaiter() override;

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  //  content::WebContentsObserver
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  void OnTimeout();

  base::OneShotTimer timeout_timer_;
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<password_manager::PasswordManagerClient> client_;
  PasswordFormFoundCallback callback_;

  raw_ptr<password_manager::PasswordFormManager> login_form_manager_;

  base::WeakPtrFactory<PasswordFormWaiter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CHANGE_PASSWORD_FORM_WAITER_H_
