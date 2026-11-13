// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/net/test/test_auth_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace enterprise::test {

namespace {

// TODO(crbug.com/532559920): Migrate Desktop from
// `IdentityTestEnvironmentProfileAdaptor` to real
// `MutableProfileOAuth2TokenServiceDelegate` + `FakeGaia` once Desktop OAuth2
// token fetches bypass the dynamic routing network pause.
class DesktopTestAuthHelper
    : public TestAuthHelper,
      public signin::IdentityManager::DiagnosticsObserver {
 public:
  DesktopTestAuthHelper() = default;
  ~DesktopTestAuthHelper() override = default;

  void SetUpInProcessBrowserTestFixture(
      std::string_view default_access_token) override {
    access_token_ = std::string(default_access_token);
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);
                }));
  }

  void SetUpOnMainThread(Profile* profile) override {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile);
    diagnostics_observation_.Observe(
        identity_test_env_adaptor_->identity_test_env()->identity_manager());
  }

  void TearDownOnMainThread() override {
    diagnostics_observation_.Reset();
    identity_test_env_adaptor_.reset();
  }

  void SignInPrimaryAccount(signin::IdentityManager* /*identity_manager*/,
                            const std::string& email,
                            const std::string& hosted_domain,
                            const std::string& access_token) override {
    access_token_ = access_token;
    simulate_invalid_credentials_ = false;
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_env_adaptor_->identity_test_env();
    AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    identity_test_env->SimulateSuccessfulFetchOfAccountInfo(
        account_info.GetAccountId(), account_info.GetEmail(),
        account_info.GetGaiaId(), hosted_domain, "Full Name", "Given Name",
        "en-US", /*picture_url=*/"");
  }

  void SimulateInvalidCredentials(const GaiaId& /*gaia_id*/) override {
    simulate_invalid_credentials_ = true;
  }

  size_t GetTokenFetchRequestCount() const override {
    return proxy_token_request_count_;
  }

  // signin::IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& /*consumer_id*/,
                              const signin::ScopeSet& scopes) override {
    const bool is_proxy_scope =
        scopes.contains(GaiaConstants::kSecureGatewayOAuth2Scope);
    if (is_proxy_scope) {
      ++proxy_token_request_count_;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopTestAuthHelper::RespondToAccessTokenRequest,
                       weak_ptr_factory_.GetWeakPtr(), account_id, scopes,
                       is_proxy_scope && simulate_invalid_credentials_));
  }

 private:
  void RespondToAccessTokenRequest(CoreAccountId account_id,
                                   signin::ScopeSet scopes,
                                   bool fail_with_invalid_credentials) {
    if (!identity_test_env_adaptor_) {
      return;
    }
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_env_adaptor_->identity_test_env();
    if (fail_with_invalid_credentials) {
      identity_test_env
          ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
              account_id,
              GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      UNKNOWN));
      return;
    }
    identity_test_env
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            access_token_, base::Time::Max(), /*id_token=*/std::string(),
            scopes);
  }

  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::DiagnosticsObserver>
      diagnostics_observation_{this};
  std::string access_token_;
  bool simulate_invalid_credentials_ = false;
  size_t proxy_token_request_count_ = 0;
  base::WeakPtrFactory<DesktopTestAuthHelper> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TestAuthHelper> TestAuthHelper::Create() {
  return std::make_unique<DesktopTestAuthHelper>();
}

}  // namespace enterprise::test
