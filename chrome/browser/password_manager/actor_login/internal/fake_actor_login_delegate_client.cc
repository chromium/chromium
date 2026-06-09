// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/fake_actor_login_delegate_client.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_cleaning_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/actor_login/internal/fake_actor_login_siwg_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics_helper.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_siwg_controller_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_web_content_interface.h"
#include "components/prefs/pref_service.h"

namespace actor_login {

namespace {

// A fake implementation of `ActorLoginCredentialsFetcher` that executes the
// callback with fake data.
class FakeActorLoginFederatedCredentialFetcher
    : public ActorLoginCredentialsFetcher {
 public:
  explicit FakeActorLoginFederatedCredentialFetcher(
      std::vector<Credential> credentials)
      : credentials_(std::move(credentials)) {}
  ~FakeActorLoginFederatedCredentialFetcher() override = default;

  void Fetch(FetchResultCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), credentials_,
                       ActorLoginCredentialsFetcher::Status::kSuccess));
  }

 private:
  std::vector<Credential> credentials_;
};

}  // namespace

FakeActorLoginDelegateClient::FakeActorLoginDelegateClient(
    Profile* profile,
    const url::Origin& origin,
    password_manager::PasswordManagerDriver* driver,
    password_manager::PasswordManagerClient* client)
    : profile_(profile), origin_(origin), driver_(driver), client_(client) {}

FakeActorLoginDelegateClient::~FakeActorLoginDelegateClient() = default;

void FakeActorLoginDelegateClient::SetActorLoginWebContentInterface(
    ActorLoginWebContentInterface* web_interface) {
  web_interface_ = web_interface;
}

PrefService* FakeActorLoginDelegateClient::GetPrefs() {
  return profile_->GetPrefs();
}

password_manager::PasswordManagerClient*
FakeActorLoginDelegateClient::GetPasswordManagerClient() {
  return client_;
}

password_manager::PasswordManagerDriver*
FakeActorLoginDelegateClient::GetPasswordManagerDriverForMainFrame() {
  return driver_;
}

ukm::SourceId FakeActorLoginDelegateClient::GetPageUkmSourceIdForMainFrame() {
  return static_cast<ukm::SourceId>(123);
}

url::Origin FakeActorLoginDelegateClient::GetLastCommittedOriginForMainFrame() {
  return origin_;
}

translate::TranslateManager*
FakeActorLoginDelegateClient::GetTranslateManager() {
  return nullptr;
}

ActorLoginPermissionCleaningService*
FakeActorLoginDelegateClient::GetPermissionCleaningService() {
  return ActorLoginPermissionCleaningServiceFactory::GetForProfile(profile_);
}

std::unique_ptr<ActorLoginCredentialsFetcher>
FakeActorLoginDelegateClient::CreateFederatedCredentialsFetcher(
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    ActorLoginMetricsHelper* metrics_helper) {
  return std::make_unique<FakeActorLoginFederatedCredentialFetcher>(
      GetExpectedFederatedCredentials());
}

std::unique_ptr<ActorLoginSiwgControllerInterface>
FakeActorLoginDelegateClient::CreateSiwgController(
    const Credential& credential,
    bool should_store_permission,
    LoginStatusResultOrErrorReply on_finished_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    base::OnceCallback<void(bool)> post_button_click_login_result_callback) {
  auto siwg_controller = std::make_unique<FakeActorLoginSiwgController>(
      test_requires_federated_button_click_, should_store_permission,
      std::move(on_finished_callback),
      std::move(post_button_click_login_result_callback));
  if (test_requires_federated_button_click_) {
    siwg_controller_ = siwg_controller.get();
  }
  return siwg_controller;
}

bool FakeActorLoginDelegateClient::IsTaskInFocus() {
  return false;
}

bool FakeActorLoginDelegateClient::SupportsFedCmEmbedderInitiatedLogin() {
  return true;
}

base::WeakPtr<ActorLoginDelegateClient>
FakeActorLoginDelegateClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeActorLoginDelegateClient::RemoveFederatedEmbedderLoginRequest() {
  is_remove_federated_embedder_login_request_called_ = true;
}

void FakeActorLoginDelegateClient::ObserveControlStateForCurrentTask(
    base::OnceClosure on_released_callback) {
  on_released_callback_ = std::move(on_released_callback);
}

std::vector<Credential>
FakeActorLoginDelegateClient::GetExpectedFederatedCredentials() const {
  Credential credential;
  credential.username = u"federated_user";
  credential.type = CredentialType::kFederated;

  FederationDetail detail;
  detail.idp_origin = origin_;
  credential.federation_detail = detail;

  credential.source_site_or_app = base::ASCIIToUTF16(origin_.host());
  return {credential};
}

void FakeActorLoginDelegateClient::TriggerControlStateReleasedCallback() {
  if (on_released_callback_) {
    std::move(on_released_callback_).Run();
  }
}

void FakeActorLoginDelegateClient::ClickSiwgButton(bool will_succeed) {
  CHECK(siwg_controller_);
  siwg_controller_->OnSiwgButtonClicked(will_succeed);
  siwg_controller_ = nullptr;
}

void FakeActorLoginDelegateClient::PrimaryPageChanged() {
  CHECK(web_interface_);
  web_interface_->OnPrimaryPageChanged();
}

void FakeActorLoginDelegateClient::WebContentsDestroyed() {
  CHECK(web_interface_);
  web_interface_->OnContextDestroyed();
}

}  // namespace actor_login
