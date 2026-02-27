// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_service.h"

#include "base/sequence_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/token_fetcher_impl.h"
#include "components/private_ai/phosphor/token_manager_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace private_ai {

// static
bool PrivateAiService::CanPrivateAiBeEnabled() {
  return base::FeatureList::IsEnabled(kPrivateAi);
}

PrivateAiService::PrivateAiService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    Profile* profile,
    std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory)
    : profile_(profile),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      bsa_factory_(std::move(bsa_factory)) {
  identity_manager_->AddObserver(this);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto logger = std::make_unique<PrivateAiLogger>();
  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto bsa = bsa_factory_->CreateBlindSignAuth(url_loader_factory->Clone());
  auto token_fetcher =
      std::make_unique<phosphor::TokenFetcherImpl>(this, std::move(bsa));
  token_fetcher_ = token_fetcher.get();
  token_manager_ =
      std::make_unique<phosphor::TokenManagerImpl>(std::move(token_fetcher));

  client_ = Client::Create(
      kPrivateAiUrl.Get(), kPrivateAiApiKey.Get(),
      kPrivateAiProxyServerUrl.Get(),
      base::FeatureList::IsEnabled(kPrivateAiUseTokenAttestation),
      profile_->GetDefaultStoragePartition()->GetNetworkContext(),
      token_manager_.get(), content::GetNetworkService(), std::move(logger));
}

PrivateAiService::~PrivateAiService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivateAiService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_shutting_down_ = true;
  identity_manager_->RemoveObserver(this);
}

phosphor::TokenManager* PrivateAiService::GetTokenManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return token_manager_.get();
}

Client* PrivateAiService::GetClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(client_);
  return client_.get();
}

void PrivateAiService::SetClientForTesting(std::unique_ptr<Client> client) {
  client_ = std::move(client);
}

bool PrivateAiService::IsTokenFetchEnabled() {
  CHECK(identity_manager_);
  if (is_shutting_down_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  return true;
}

void PrivateAiService::RequestOAuthToken(RequestOAuthTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsTokenFetchEnabled()) {
    std::move(callback).Run(phosphor::GetAuthnTokensResult::kFailedNoAccount,
                            std::nullopt);
    return;
  }

  auto oauth_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kPrivateAiService, identity_manager_,
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);

  auto* fetcher_ptr = oauth_token_fetcher.get();
  fetcher_ptr->Start(
      base::BindOnce(&PrivateAiService::OnRequestOAuthTokenCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(oauth_token_fetcher), std::move(callback)));
}

void PrivateAiService::OnRequestOAuthTokenCompleted(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
    RequestOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "OAuth token fetch failed with error: " << error.ToString();
    phosphor::GetAuthnTokensResult result =
        error.IsTransientError()
            ? phosphor::GetAuthnTokensResult::kFailedOAuthTokenTransient
            : phosphor::GetAuthnTokensResult::kFailedOAuthTokenPersistent;
    std::move(callback).Run(result, std::nullopt);
    return;
  }
  std::move(callback).Run(phosphor::GetAuthnTokensResult::kSuccess,
                          access_token_info.token);
}

void PrivateAiService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (token_fetcher_) {
    bool account_available =
        event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kCleared;
    token_fetcher_->AccountStatusChanged(account_available);
  }
}

}  // namespace private_ai
