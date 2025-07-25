// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace password_manager {
class FormFetcherImpl;
class PasswordManagerClient;
}  // namespace password_manager

namespace actor_login {

// Delegate implementation, scoped to `WebContents` as its functionality is
// intrinsically tied to a specific browser tab.
class ActorLoginDelegateImpl
    : public ActorLoginDelegate,
      public content::WebContentsUserData<ActorLoginDelegateImpl>,
      public password_manager::FormFetcher::Consumer {
 public:
  ~ActorLoginDelegateImpl() override;

  ActorLoginDelegateImpl(const ActorLoginDelegateImpl&) = delete;
  ActorLoginDelegateImpl& operator=(const ActorLoginDelegateImpl&) = delete;

  // `ActorLoginDelegate` implementation:
  void GetCredentials(CredentialsOrErrorReply callback) override;
  void AttemptLogin(const Credential& credential,
                    LoginStatusResultOrErrorReply callback) override;

 private:
  friend class content::WebContentsUserData<ActorLoginDelegateImpl>;

  // Private constructor for `WebContentsUserData`.
  // This is the constructor that `WebContentsUserData::FromWebContents` will
  // call when no instance exists and it needs to create one.
  explicit ActorLoginDelegateImpl(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client);

  // Private helper methods for handling asynchronous task completion.
  void OnAttemptLoginCompleted();

  // `FormFetcher::Consumer`:
  void OnFetchCompleted() override;

  // Store the pending callbacks. A non-null callback indicates an active
  // request.
  CredentialsOrErrorReply pending_get_credentials_callback_;
  LoginStatusResultOrErrorReply pending_attempt_login_callback_;

  std::unique_ptr<password_manager::FormFetcherImpl> form_fetcher_;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  base::WeakPtrFactory<ActorLoginDelegateImpl> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
