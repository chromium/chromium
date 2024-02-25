// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/ash/signin_helper.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
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

// A dictionary pref that stores the mapping from (access) token handles to a
// hash of the OAuth refresh token from which the token handle was derived.
constexpr char kTokenHandleMap[] = "ash.token_handle_map";

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

account_manager::AccountManager* GetAccountManager(Profile* profile) {
  return g_browser_process->platform_part()
      ->GetAccountManagerFactory()
      ->GetAccountManager(profile->GetPath().value());
}

}  // namespace

TokenHandleFetcher::TokenHandleFetcher(Profile* profile,
                                       TokenHandleUtil* util,
                                       const AccountId& account_id)
    : profile_(profile), token_handle_util_(util), account_id_(account_id) {
  CHECK(profile_.get());
  CHECK(token_handle_util_.get());
}

TokenHandleFetcher::~TokenHandleFetcher() {}

void TokenHandleFetcher::BackfillToken(TokenFetchingCallback callback) {
  callback_ = std::move(callback);

  if (account_id_.GetAccountType() != AccountType::GOOGLE) {
    return;
  }

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  // This class doesn't care about browser sync consent.
  if (!identity_manager_->HasAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin))) {
    profile_shutdown_subscription_ =
        TokenHandleFetcherShutdownNotifierFactory::GetInstance()
            ->Get(profile_)
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

  GetAccountManager(profile_)->GetTokenHash(
      account_manager::AccountKey(account_id_.GetGaiaId(),
                                  account_manager::AccountType::kGaia),
      base::BindOnce(&TokenHandleFetcher::FillForAccessToken,
                     weak_factory_.GetWeakPtr(),
                     /*access_token=*/token_info.token));
}

// static
void TokenHandleFetcher::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(/*path=*/kTokenHandleMap);
}

void TokenHandleFetcher::FillForNewUser(const std::string& access_token,
                                        const std::string& refresh_token_hash,
                                        TokenFetchingCallback callback) {
  callback_ = std::move(callback);
  FillForAccessToken(access_token, refresh_token_hash);
}

void TokenHandleFetcher::FillForAccessToken(
    const std::string& access_token,
    const std::string& refresh_token_hash) {
  refresh_token_hash_ = refresh_token_hash;
  if (!gaia_client_.get()) {
    gaia_client_ = std::make_unique<gaia::GaiaOAuthClient>(
        profile_->GetURLLoaderFactory());
  }
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
      StoreTokenHandleMapping(*handle);
    }
  }
  const base::TimeDelta duration =
      base::TimeTicks::Now() - tokeninfo_response_start_time_;
  base::UmaHistogramTimes("Login.TokenObtainResponseTime", duration);
  std::move(callback_).Run(account_id_, success);
}

void TokenHandleFetcher::StoreTokenHandleMapping(
    const std::string& token_handle) {
  PrefService* prefs = profile_->GetPrefs();
  ScopedDictPrefUpdate update(prefs, kTokenHandleMap);
  CHECK(!refresh_token_hash_.empty());
  update->Set(token_handle, refresh_token_hash_);
}

void TokenHandleFetcher::DiagnoseTokenHandleMapping(const AccountId& account_id,
                                                    const std::string& token) {
  GetAccountManager(profile_)->GetTokenHash(
      account_manager::AccountKey(account_id.GetGaiaId(),
                                  account_manager::AccountType::kGaia),
      base::BindOnce(&TokenHandleFetcher::OnGetTokenHash,
                     weak_factory_.GetWeakPtr(), token));
}

void TokenHandleFetcher::OnGetTokenHash(
    const std::string& token,
    const std::string& account_manager_stored_hash) {
  PrefService* prefs = profile_->GetPrefs();
  const base::Value::Dict& token_handle_map = prefs->GetDict(kTokenHandleMap);
  const std::string* pref_stored_hash_val = token_handle_map.FindString(token);
  if (!pref_stored_hash_val) {
    return;
  }

  const bool hashes_match =
      (*pref_stored_hash_val == account_manager_stored_hash);
  if (!hashes_match) {
    LOG(ERROR) << "Token handle check was performed against an older token";
  }
  base::UmaHistogramBoolean("Login.IsTokenHandleInSyncWithRefreshToken",
                            hashes_match);
}

void TokenHandleFetcher::OnProfileDestroyed() {
  std::move(callback_).Run(account_id_, false);
}

// static
void TokenHandleFetcher::EnsureFactoryBuilt() {
  TokenHandleFetcherShutdownNotifierFactory::GetInstance();
}

}  // namespace ash
