// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

const int kMaxRetries = 3;
const char kAccessTokenFetchId[] = "token_handle_fetcher";

class TokenHandleFetcherShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static TokenHandleFetcherShutdownNotifierFactory* GetInstance() {
    return base::Singleton<TokenHandleFetcherShutdownNotifierFactory>::get();
  }

  TokenHandleFetcherShutdownNotifierFactory(
      const TokenHandleFetcherShutdownNotifierFactory&) = delete;
  TokenHandleFetcherShutdownNotifierFactory& operator=(
      const TokenHandleFetcherShutdownNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      TokenHandleFetcherShutdownNotifierFactory>;

  TokenHandleFetcherShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "TokenHandleFetcher") {
    DependsOn(IdentityManagerFactory::GetInstance());
  }
  ~TokenHandleFetcherShutdownNotifierFactory() override {}
};

}  // namespace

TokenHandleFetcher::TokenHandleFetcher(TokenHandleUtil* util,
                                       const AccountId& account_id)
    : token_handle_util_(util), account_id_(account_id) {}

TokenHandleFetcher::~TokenHandleFetcher() {}

void TokenHandleFetcher::BackfillToken(Profile* profile,
                                       TokenFetchingCallback callback) {
  profile_ = profile;
  callback_ = std::move(callback);

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  // This class doesn't care about browser sync consent.
  if (!identity_manager_->HasAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin))) {
    profile_shutdown_subscription_ =
        TokenHandleFetcherShutdownNotifierFactory::GetInstance()
            ->Get(profile)
            ->Subscribe(
                base::BindRepeating(&TokenHandleFetcher::OnProfileDestroyed,
                                    base::Unretained(this)));
  }

  // Now we can request the token, knowing that it will be immediately requested
  // if the refresh token is available, or that it will be requested once the
  // refresh token is available for the primary account.
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);

  // We can use base::Unretained(this) below because `access_token_fetcher_` is
  // owned by this object (thus destroyed when this object is destroyed) and
  // PrimaryAccountAccessTokenFetcher guarantees that it doesn't invoke its
  // callback after it is destroyed.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kAccessTokenFetchId, identity_manager_, scopes,
          base::BindOnce(&TokenHandleFetcher::OnAccessTokenFetchComplete,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void TokenHandleFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Could not get access token to backfill token handler"
               << error.ToString();
    std::move(callback_).Run(account_id_, false);
    return;
  }

  FillForAccessToken(token_info.token);
}

void TokenHandleFetcher::FillForNewUser(const std::string& access_token,
                                        TokenFetchingCallback callback) {
  profile_ = ProfileHelper::Get()->GetSigninProfile();
  callback_ = std::move(callback);
  FillForAccessToken(access_token);
}

void TokenHandleFetcher::FillForAccessToken(const std::string& access_token) {
  if (!gaia_client_.get())
    gaia_client_ = std::make_unique<gaia::GaiaOAuthClient>(
        profile_->GetURLLoaderFactory());
  tokeninfo_response_start_time_ = base::TimeTicks::Now();
  gaia_client_->GetTokenInfo(access_token, kMaxRetries, this);
}

void TokenHandleFetcher::OnOAuthError() {
  std::move(callback_).Run(account_id_, false);
}

void TokenHandleFetcher::OnNetworkError(int response_code) {
  std::move(callback_).Run(account_id_, false);
}

void TokenHandleFetcher::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  bool success = false;
  if (!token_info.Find("error")) {
    const std::string* handle = token_info.FindString("token_handle");
    if (handle) {
      success = true;
      token_handle_util_->StoreTokenHandle(account_id_, *handle);
    }
  }
  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  UMA_HISTOGRAM_TIMES("Login.TokenObtainResponseTime", duration);
  std::move(callback_).Run(account_id_, success);
}

void TokenHandleFetcher::OnProfileDestroyed() {
  std::move(callback_).Run(account_id_, false);
}

// static
void TokenHandleFetcher::EnsureFactoryBuilt() {
  TokenHandleFetcherShutdownNotifierFactory::GetInstance();
}

}  // namespace ash
