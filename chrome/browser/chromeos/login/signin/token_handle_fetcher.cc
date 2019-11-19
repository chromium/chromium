// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/token_handle_fetcher.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/login/signin/token_handle_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
const int kMaxRetries = 3;
const char kAccessTokenFetchId[] = "token_handle_fetcher";

class TokenHandleFetcherShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static TokenHandleFetcherShutdownNotifierFactory* GetInstance() {
    return base::Singleton<TokenHandleFetcherShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      TokenHandleFetcherShutdownNotifierFactory>;

  TokenHandleFetcherShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "TokenHandleFetcher") {
    DependsOn(IdentityManagerFactory::GetInstance());
  }
  ~TokenHandleFetcherShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(TokenHandleFetcherShutdownNotifierFactory);
};

}  // namespace

TokenHandleFetcher::TokenHandleFetcher(TokenHandleUtil* util,
                                       const AccountId& account_id)
    : token_handle_util_(util), account_id_(account_id) {}

TokenHandleFetcher::~TokenHandleFetcher() {}

void TokenHandleFetcher::BackfillToken(Profile* profile,
                                       const TokenFetchingCallback& callback) {
  profile_ = profile;
  callback_ = callback;

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager_->HasPrimaryAccountWithRefreshToken()) {
    profile_shutdown_notification_ =
        TokenHandleFetcherShutdownNotifierFactory::GetInstance()
            ->Get(profile)
            ->Subscribe(base::Bind(&TokenHandleFetcher::OnProfileDestroyed,
                                   base::Unretained(this)));
  }

  // Now we can request the token, knowing that it will be immediately requested
  // if the refresh token is available, or that it will be requested once the
  // refresh token is available for the primary account.
  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);

  // We can use base::Unretained(this) below because |access_token_fetcher_| is
  // owned by this object (thus destroyed when this object is destroyed) and
  // PrimaryAccountAccessTokenFetcher guarantees that it doesn't invoke its
  // callback after it is destroyed.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kAccessTokenFetchId, identity_manager_, scopes,
          base::BindOnce(&TokenHandleFetcher::OnAccessTokenFetchComplete,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void TokenHandleFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Could not get access token to backfill token handler"
               << error.ToString();
    callback_.Run(account_id_, false);
    return;
  }

  FillForAccessToken(token_info.token);
}

void TokenHandleFetcher::FillForNewUser(const std::string& access_token,
                                        const TokenFetchingCallback& callback) {
  profile_ = chromeos::ProfileHelper::Get()->GetSigninProfile();
  callback_ = callback;
  FillForAccessToken(access_token);
}

void TokenHandleFetcher::FillForAccessToken(const std::string& access_token) {
  if (!gaia_client_.get())
    gaia_client_.reset(
        new gaia::GaiaOAuthClient(profile_->GetURLLoaderFactory()));
  tokeninfo_response_start_time_ = base::TimeTicks::Now();
  gaia_client_->GetTokenInfo(access_token, kMaxRetries, this);
}

void TokenHandleFetcher::OnOAuthError() {
  callback_.Run(account_id_, false);
}

void TokenHandleFetcher::OnNetworkError(int response_code) {
  callback_.Run(account_id_, false);
}

void TokenHandleFetcher::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  bool success = false;
  if (!token_info->HasKey("error")) {
    std::string handle;
    if (token_info->GetString("token_handle", &handle)) {
      success = true;
      token_handle_util_->StoreTokenHandle(account_id_, handle);
    }
  }
  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  UMA_HISTOGRAM_TIMES("Login.TokenObtainResponseTime", duration);
  callback_.Run(account_id_, success);
}

void TokenHandleFetcher::OnProfileDestroyed() {
  callback_.Run(account_id_, false);
}
