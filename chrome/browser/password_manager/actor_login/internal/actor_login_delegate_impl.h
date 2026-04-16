// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_siwg_controller.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "content/public/browser/web_contents_user_data.h"

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace actor_login {

class ActorLoginCredentialFiller;
class ActorLoginGetCredentialsHelper;
class ActorLoginMetricsHelper;

// Delegate implementation, scoped to `WebContents` as its functionality is
// intrinsically tied to a specific browser tab.
class ActorLoginDelegateImpl
    : public ActorLoginDelegate,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ActorLoginDelegateImpl>,
      public password_manager::PasswordManagerInterface::Observer {
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
      bool has_sign_in_with_google_button,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) override;
  void AttemptLogin(
      const Credential& credential,
      bool should_store_permission,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) override;

  // password_manager::PasswordManagerInterface::Observer implementation:
  void OnLoginSuccessful(
      const password_manager::PasswordForm& pending_form) override;

 private:
  friend class content::WebContentsUserData<ActorLoginDelegateImpl>;

  // Private constructor for `WebContentsUserData`.
  // This is the constructor that `WebContentsUserData::CreateForWebContents`
  // will call when no instance exists and it needs to create one.
  ActorLoginDelegateImpl(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      PasswordDriverSupplierForPrimaryMainFrame driver_supplier);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Checks whether the currently ongoing task is in focus, either in
  // the tab or in its corresponding Glic UI instance.
  bool IsTaskInFocus();

  // Private helper methods for handling task completion. They should be
  // invoked asynchronously.
  void OnGetCredentialsCompleted(CredentialsOrErrorReply callback,
                                 CredentialsOrError result,
                                 bool conflicting_permissions);
  void OnAttemptLoginCompleted(
      base::expected<LoginStatusResult, ActorLoginError> result);

  // Called when `OnAttemptLoginCompleted` is invoked with a result for
  // a federated credential login.
  void ProcessFederatedResult(
      base::expected<LoginStatusResult, ActorLoginError> result);

  // Called when `OnAttemptLoginCompleted` is invoked with a filling
  // result for a password credential login.
  void ProcessPasswordResult(
      base::expected<LoginStatusResult, ActorLoginError> result);

  void OnActorTaskStateChanged(actor::ActorTask& task);

  void OnActionSequenceEnded(bool success);

  bool ShouldCleanUpConflictingPermissions(
      const password_manager::PasswordForm& form) const;

  // Calls the permissions cleaning service to clean up conflicting permissions.
  // If the login attempt was performed with a password credential,
  // `signon_realm`, is used to identify it, so that we don't clean the
  // permission granted after disambiguation.
  void ClearConflictingPermissions(std::optional<std::string> signon_realm);

  // Reset any pending state from a previous invocation. Most fields are reset
  // when the corresponding request finishes, or the login succeeds or failed.
  // However, the password manager cannot be fully relied upon to call the
  // delegate back with the login result, so just in case, reset the fields
  // which depend on it when a new `GetCredentials` request comes in.
  void ResetState();

  // Helper methods for recording metrics.
  void RecordGetCredentialsMetricsAndResetHelper(
      const CredentialsOrError& result);
  void RecordAttemptLoginMetrics(const Credential& credential);

  // Store the pending callback. A non-null callback indicates an active
  // request.
  LoginStatusResultOrErrorReply pending_attempt_login_done_callback_;

  base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate_;
  base::CallbackListSubscription action_sequence_subscription_;

  // Helper for `GetCredentials`. Scoped to one `GetCredentials` request.
  std::unique_ptr<ActorLoginGetCredentialsHelper> get_credentials_helper_;

  // Callback that returns a `PasswordManagerDriver` corresponding to the
  // primary main frame of the passed-in `WebContents`.
  PasswordDriverSupplierForPrimaryMainFrame driver_supplier_;

  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  // Helper for recording Actor.Login metrics. The helper is created at the
  // beginning of a `GetCredentials` or `AttemptLogin` request, and it's
  // reset (recording metrics) when the request is completed. If credentials
  // are found, it's kept alive until an `AttemptLogin` request is made or
  // until the flow is considered finished.
  std::unique_ptr<ActorLoginMetricsHelper> metrics_helper_;

  // Fills credentials into a form. Scoped to one `AttemptLogin` request.
  std::unique_ptr<ActorLoginCredentialFiller> credential_filler_;

  // Handles FedCM login. For prototyping purposes this uses heuristics to find
  // and click the SiwG button. After the prototype, the click will be done
  // through `ExecutionEngine`.
  // Scoped to one `AttemptLogin` request.
  std::unique_ptr<ActorLoginSiwgController> siwg_controller_;

  // Track the currently acting task to know when we can remove the
  // FederatedEmbedderLoginRequest from the `WebContents`. This is needed to
  // ensure that the request is removed in cases such as the task being stopped
  // by user action, which can happen before the login flow completes.
  actor::TaskId acting_task_id_;
  base::CallbackListSubscription actor_task_state_subscription_;

  // Stores the credential with which the latest `AttemptLogin` request was
  // made. This is used to clean up the permission after the login attempt.
  std::unique_ptr<Credential> last_attempted_credential_;

  // Set to true whenever we find conflicting permissions in the
  // `GetCredentials` step. Reset when the login process completes. If the login
  // is successful the conflicting permissions will be cleaned up.
  // TODO(crbug.com/486089293): Reset on federated login completion as well.
  bool found_conflicting_permissions_ = false;

  // Used to listen to whether the password login was successful.
  base::ScopedObservation<password_manager::PasswordManagerInterface,
                          password_manager::PasswordManagerInterface::Observer>
      password_manager_observation_{this};

  base::WeakPtrFactory<ActorLoginDelegateImpl> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
