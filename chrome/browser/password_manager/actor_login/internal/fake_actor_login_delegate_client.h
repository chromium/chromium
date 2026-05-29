// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_FAKE_ACTOR_LOGIN_DELEGATE_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_FAKE_ACTOR_LOGIN_DELEGATE_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credentials_fetcher.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate_client.h"
#include "url/origin.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

namespace actor_login {

// A test double of `ActorLoginDelegateClient` used in unit tests to inspect
// delegate state and trigger test events.
class FakeActorLoginDelegateClient : public ActorLoginDelegateClient {
 public:
  FakeActorLoginDelegateClient(Profile* profile,
                               content::WebContents* web_contents,
                               const url::Origin& origin,
                               password_manager::PasswordManagerDriver* driver);
  ~FakeActorLoginDelegateClient() override;

  // ActorLoginDelegateClient implementation:
  PrefService* GetPrefs() override;
  password_manager::PasswordManagerDriver*
  GetPasswordManagerDriverForMainFrame() override;
  ukm::SourceId GetPageUkmSourceIdForMainFrame() override;
  url::Origin GetLastCommittedOriginForMainFrame() override;
  translate::TranslateManager* GetTranslateManager() override;
  ActorLoginPermissionCleaningService* GetPermissionCleaningService() override;
  std::unique_ptr<ActorLoginCredentialsFetcher>
  CreateFederatedCredentialsFetcher(
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      ActorLoginMetricsHelper* metrics_helper) override;
  std::unique_ptr<ActorLoginSiwgControllerInterface> CreateSiwgController(
      const Credential& credential,
      bool should_store_permission,
      LoginStatusResultOrErrorReply on_finished_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      base::OnceCallback<void(bool)> post_button_click_login_result_callback)
      override;
  bool IsTaskInFocus() override;
  bool SupportsFedCmEmbedderInitiatedLogin() override;
  base::WeakPtr<ActorLoginDelegateClient> AsWeakPtr() override;
  void RemoveFederatedEmbedderLoginRequest() override;
  void ObserveControlStateForCurrentTask(
      base::OnceClosure on_released_callback) override;

  // Convenience methods for testing purposes:

  // Returns true if `RemoveFederatedEmbedderLoginRequest` was called.
  bool remove_federated_embedder_login_request_called() const {
    return remove_federated_embedder_login_request_called_;
  }

  // Returns true if `ObserveControlStateForCurrentTask` was called with a valid
  // callback.
  bool has_on_released_callback() const {
    return !on_released_callback_.is_null();
  }

  // Returns the expected federated credentials that this client serves.
  std::vector<Credential> GetExpectedFederatedCredentials() const;

  // Triggers the control state released callback registered by the observer.
  void TriggerControlStateReleasedCallback();

 private:
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  url::Origin origin_;
  raw_ptr<password_manager::PasswordManagerDriver> driver_;
  bool remove_federated_embedder_login_request_called_ = false;
  base::OnceClosure on_released_callback_;
  base::WeakPtrFactory<FakeActorLoginDelegateClient> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_FAKE_ACTOR_LOGIN_DELEGATE_CLIENT_H_
