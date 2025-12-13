// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace actor_login {

class ActorLoginCredentialFiller;
class ActorLoginGetCredentialsHelper;

// Delegate implementation, scoped to `WebContents` as its functionality is
// intrinsically tied to a specific browser tab.
class ActorLoginDelegateImpl
    : public ActorLoginDelegate,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ActorLoginDelegateImpl> {
 public:
  using PasswordDriverSupplierForPrimaryMainFrame =
      base::RepeatingCallback<password_manager::PasswordManagerDriver*(
          content::WebContents*)>;

  static ActorLoginDelegate* GetOrCreate(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client);

  static ActorLoginDelegate* GetOrCreateForTesting(
      content::WebContents* web_contents,
      ::password_manager::PasswordManagerClient* client,
      PasswordDriverSupplierForPrimaryMainFrame driver_supplier);

  ~ActorLoginDelegateImpl() override;

  // Not copyable or movable.
  ActorLoginDelegateImpl(const ActorLoginDelegateImpl&) = delete;
  ActorLoginDelegateImpl& operator=(const ActorLoginDelegateImpl&) = delete;

  // `ActorLoginDelegate` implementation:
  void GetCredentials(
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) override;
  void AttemptLogin(const Credential& credential,
                    bool should_store_permission,
                    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
                    LoginStatusResultOrErrorReply callback) override;

 private:
  friend class content::WebContentsUserData<ActorLoginDelegateImpl>;

  // Private constructor for `WebContentsUserData`.
  // This is the constructor that `WebContentsUserData::CreateForWebContents`
  // will call when no instance exists and it needs to create one.
  ActorLoginDelegateImpl(
      content::WebContents* web_contents,
      ::password_manager::PasswordManagerClient* client,
      PasswordDriverSupplierForPrimaryMainFrame driver_supplier);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Checks whether the currently ongoing task is in focus, either in
  // the tab or in its corresponding Glic UI instance.
  bool IsTaskInFocus();

  // Private helper methods for handling task completion. They should be
  // invoked asynchronously.
  void OnGetCredentialsCompleted(CredentialsOrErrorReply callback,
                                 CredentialsOrError result);
  void OnAttemptLoginCompleted(
      base::expected<LoginStatusResult, ActorLoginError> result);

  // Store the pending callback. A non-null callback indicates an active
  // request.
  LoginStatusResultOrErrorReply pending_attempt_login_callback_;

  // Helper for `GetCredentials`. Scoped to one `GetCredentials` request.
  std::unique_ptr<ActorLoginGetCredentialsHelper> get_credentials_helper_;

  // Callback that returns a `PasswordManagerDriver` corresponding to the
  // primary main frame of the passed-in `WebContents`.
  PasswordDriverSupplierForPrimaryMainFrame driver_supplier_;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // Fills credentials into a form. Scoped to one `AttemptLogin` request.
  std::unique_ptr<ActorLoginCredentialFiller> credential_filler_;

  base::WeakPtrFactory<ActorLoginDelegateImpl> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
