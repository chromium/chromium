// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/legion/private_ai_service.h"

#include "base/sequence_checker.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/legion/client.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/token_fetcher_impl.h"
#include "components/legion/phosphor/token_manager_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"

namespace legion {

// static
bool PrivateAiService::CanLegionBeEnabled() {
  return base::FeatureList::IsEnabled(kLegion);
}

PrivateAiService::PrivateAiService(signin::IdentityManager* identity_manager,
                                   PrefService* pref_service,
                                   Profile* profile)
    : profile_(profile),
      identity_manager_(identity_manager),
      pref_service_(pref_service) {
  identity_manager_->AddObserver(this);
}

PrivateAiService::~PrivateAiService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivateAiService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_shutting_down_ = true;
  identity_manager_->RemoveObserver(this);
}

// TODO(b:469400476): Move into ctor. Currently some tests will fail because
// vtable is not matching the expectation that
// `TestPrivateAiService::CreateBlindSignAuth()` will be called.
void PrivateAiService::InitializeServicesIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_shutting_down_) {
    return;
  }
  if (token_manager_ && client_) {
    return;
  }

  auto url_loader_factory = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto token_fetcher = std::make_unique<phosphor::TokenFetcherImpl>(
      this, url_loader_factory->Clone());
  token_fetcher_ = token_fetcher.get();
  token_manager_ =
      std::make_unique<phosphor::TokenManagerImpl>(std::move(token_fetcher));
  client_ = legion::Client::Create(
      token_manager_.get(),
      profile_->GetDefaultStoragePartition()->GetNetworkContext());
}

phosphor::TokenManager* PrivateAiService::GetTokenManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitializeServicesIfNeeded();

  return token_manager_.get();
}

Client* PrivateAiService::GetClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitializeServicesIfNeeded();

  return client_.get();
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
          signin::OAuthConsumerId::kLegionService, identity_manager_,
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

  InitializeServicesIfNeeded();

  if (token_fetcher_) {
    bool account_available =
        event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kCleared;
    token_fetcher_->AccountStatusChanged(account_available);
  }
}

}  // namespace legion
