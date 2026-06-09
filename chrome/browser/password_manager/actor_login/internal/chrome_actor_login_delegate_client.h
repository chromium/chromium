// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_CHROME_ACTOR_LOGIN_DELEGATE_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_CHROME_ACTOR_LOGIN_DELEGATE_CLIENT_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_task.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate_client.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_web_content_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class PrefService;

namespace content {
class Page;
class WebContents;
}

namespace translate {
class TranslateManager;
}

namespace actor_login {

// Chrome-specific implementation of `ActorLoginDelegateClient`.
//
// Note: For any critical implementation changes, update its test double
// `FakeActorLoginDelegateClient`.
class ChromeActorLoginDelegateClient
    : public ActorLoginDelegateClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeActorLoginDelegateClient> {
 public:
  ~ChromeActorLoginDelegateClient() override;

  // Not copyable or movable.
  ChromeActorLoginDelegateClient(const ChromeActorLoginDelegateClient&) =
      delete;
  ChromeActorLoginDelegateClient& operator=(
      const ChromeActorLoginDelegateClient&) = delete;

  // ActorLoginDelegateClient:
  void SetActorLoginWebContentInterface(
      ActorLoginWebContentInterface* web_interface) override;
  PrefService* GetPrefs() override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient() override;
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
  void RemoveFederatedEmbedderLoginRequest() override;
  void ObserveControlStateForCurrentTask(
      base::OnceClosure on_released_callback) override;
  base::WeakPtr<ActorLoginDelegateClient> AsWeakPtr() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<ChromeActorLoginDelegateClient>;

  explicit ChromeActorLoginDelegateClient(content::WebContents* web_contents);

  // Notifies the client that the actor task's state has changed.
  void OnActorTaskStateChanged(actor::ActorTask& task);

  // Track the currently acting task to know when we should execute
  // `OnActorTaskStateChanged`.
  actor::TaskId acting_task_id_;
  base::CallbackListSubscription actor_task_state_subscription_;

  // Callback sent into `ObserveControlStateForCurrentTask` to be invoked the
  // the task is no longer controlled by the actor.
  base::OnceClosure on_task_control_released_callback_;

  // Weak reference to the registered delegate interface, used to dispatch
  // WebContents-scoped events (such as navigations or tab destruction)
  // to the active login flow without holding strong ownership or exposing
  // implementation-specific methods.
  raw_ptr<ActorLoginWebContentInterface> web_interface_ = nullptr;

  base::WeakPtrFactory<ChromeActorLoginDelegateClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_CHROME_ACTOR_LOGIN_DELEGATE_CLIENT_H_
