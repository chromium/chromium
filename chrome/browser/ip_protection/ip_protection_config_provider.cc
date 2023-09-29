// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_config_provider.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ip_protection/get_proxy_config.pb.h"
#include "chrome/browser/ip_protection/ip_protection_config_http.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"

IpProtectionConfigProvider::IpProtectionConfigProvider(
    signin::IdentityManager* identity_manager,
    Profile* profile)
    : identity_manager_(identity_manager), profile_(profile) {
  CHECK(identity_manager);
  identity_manager_->AddObserver(this);
}

void IpProtectionConfigProvider::SetUp() {
  if (!ip_protection_config_http_) {
    if (!url_loader_factory_) {
      CHECK(profile_);
      url_loader_factory_ = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
    }
    ip_protection_config_http_ =
        std::make_unique<IpProtectionConfigHttp>(url_loader_factory_.get());
  }
  if (!bsa_) {
    if (!blind_sign_auth_) {
      blind_sign_auth_ = std::make_unique<quiche::BlindSignAuth>(
          ip_protection_config_http_.get(),
          privacy::ppn::BlindSignAuthOptions());
    }
    bsa_ = blind_sign_auth_.get();
  }
}

void IpProtectionConfigProvider::SetUpForTesting(
    std::unique_ptr<IpProtectionConfigHttp> ip_protection_config_http,
    quiche::BlindSignAuthInterface* bsa) {
  // Carefully destroy any existing values in the correct order.
  ip_protection_config_http_ = nullptr;
  url_loader_factory_ = nullptr;
  ip_protection_config_http_ = std::move(ip_protection_config_http);

  bsa_ = nullptr;
  blind_sign_auth_ = nullptr;
  bsa_ = bsa;
}

IpProtectionConfigProvider::~IpProtectionConfigProvider() = default;

void IpProtectionConfigProvider::TryGetAuthTokens(
    uint32_t batch_size,
    TryGetAuthTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // The `batch_size` is cast to an `int` for use by BlindSignAuth, so check for
  // overflow here.
  if (batch_size == 0 || batch_size > INT_MAX) {
    mojo::ReportBadMessage("Invalid batch_size");
    return;
  }

  if (last_try_get_auth_tokens_backoff_ &&
      *last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    TryGetAuthTokensComplete(
        absl::nullopt, std::move(callback),
        IpProtectionTryGetAuthTokensResult::kFailedNoAccount);
    return;
  }

  RequestOAuthToken(batch_size, std::move(callback));
}

void IpProtectionConfigProvider::GetProxyList(GetProxyListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  ip_protection_config_http_->GetProxyConfig(base::BindOnce(
      [](GetProxyListCallback callback,
         absl::StatusOr<ip_protection::GetProxyConfigResponse> response) {
        if (!response.ok()) {
          VLOG(2) << "IPATP::GetProxyList failed: " << response.status();
          std::move(callback).Run(absl::nullopt);
          return;
        }
        std::vector<std::string> proxy_list(
            response->first_hop_hostnames().begin(),
            response->first_hop_hostnames().end());
        VLOG(2) << "IPATP::GetProxyList got proxy list of length "
                << proxy_list.size();
        std::move(callback).Run(std::move(proxy_list));
      },
      std::move(callback)));
}

void IpProtectionConfigProvider::RequestOAuthToken(
    uint32_t batch_size,
    TryGetAuthTokensCallback callback) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    TryGetAuthTokensComplete(
        absl::nullopt, std::move(callback),
        IpProtectionTryGetAuthTokensResult::kFailedNoAccount);
    return;
  }

  // TODO(https://crbug.com/1444621): Add a client side account capabilities
  // check to compliment the server-side checks.

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kIpProtectionAuthScope);

  // Waits for the account to have a refresh token before making the request.
  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  // Create the OAuth token fetcher and call `OnRequestOAuthTokenCompleted()`
  // when complete. The callback will own the
  // `signin::PrimaryAccountAccessTokenFetcher()` object to ensure it stays
  // alive long enough for the callback to occur, and we will pass a weak
  // pointer to ensure that the callback won't be called if this object gets
  // destroyed.
  auto oauth_token_fetch_start_time = base::TimeTicks::Now();
  auto oauth_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"IpProtectionService", identity_manager_, scopes,
          mode, signin::ConsentLevel::kSignin);
  auto* oauth_token_fetcher_ptr = oauth_token_fetcher.get();
  oauth_token_fetcher_ptr->Start(base::BindOnce(
      &IpProtectionConfigProvider::OnRequestOAuthTokenCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(oauth_token_fetcher),
      oauth_token_fetch_start_time, batch_size, std::move(callback)));
}

void IpProtectionConfigProvider::OnRequestOAuthTokenCompleted(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
        oauth_token_fetcher,
    base::TimeTicks oauth_token_fetch_start_time,
    uint32_t batch_size,
    TryGetAuthTokensCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }

  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (error.state() != GoogleServiceAuthError::NONE) {
    VLOG(2) << "IPATP::OnRequestOAuthTokenCompleted got an error: "
            << static_cast<int>(error.state());
    TryGetAuthTokensComplete(
        absl::nullopt, std::move(callback),
        error.IsTransientError()
            ? IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenTransient
            : IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenPersistent);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  base::UmaHistogramTimes("NetworkService.IpProtection.OAuthTokenFetchTime",
                          current_time - oauth_token_fetch_start_time);
  FetchBlindSignedToken(access_token_info, batch_size, std::move(callback));
}

void IpProtectionConfigProvider::FetchBlindSignedToken(
    signin::AccessTokenInfo access_token_info,
    uint32_t batch_size,
    TryGetAuthTokensCallback callback) {
  auto bsa_get_tokens_start_time = base::TimeTicks::Now();
  bsa_->GetTokens(
      access_token_info.token, batch_size,
      [weak_ptr = weak_ptr_factory_.GetWeakPtr(), bsa_get_tokens_start_time,
       callback = std::move(callback)](
          absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) mutable {
        if (weak_ptr) {
          weak_ptr->OnFetchBlindSignedTokenCompleted(
              bsa_get_tokens_start_time, std::move(callback), tokens);
        }
      });
}

void IpProtectionConfigProvider::OnFetchBlindSignedTokenCompleted(
    base::TimeTicks bsa_get_tokens_start_time,
    TryGetAuthTokensCallback callback,
    absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  if (!tokens.ok()) {
    // Apply the canonical mapping from abseil status to HTTP status.
    IpProtectionTryGetAuthTokensResult result;
    switch (tokens.status().code()) {
      case absl::StatusCode::kInvalidArgument:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSA400;
        break;
      case absl::StatusCode::kUnauthenticated:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSA401;
        break;
      case absl::StatusCode::kPermissionDenied:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSA403;
        break;
      default:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSAOther;
        break;
    }
    VLOG(2) << "IPATP::OnFetchBlindSignedTokenCompleted got an error: "
            << static_cast<int>(result);
    TryGetAuthTokensComplete(absl::nullopt, std::move(callback), result);
    return;
  }

  if (tokens.value().size() == 0) {
    VLOG(2) << "IPATP::OnFetchBlindSignedTokenCompleted called with no tokens";
    TryGetAuthTokensComplete(
        absl::nullopt, std::move(callback),
        IpProtectionTryGetAuthTokensResult::kFailedBSAOther);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  base::UmaHistogramTimes("NetworkService.IpProtection.TokenBatchRequestTime",
                          current_time - bsa_get_tokens_start_time);

  std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens;
  base::ranges::transform(tokens.value(), std::back_inserter(bsa_tokens),
                          [](quiche::BlindSignToken bsa_token) {
                            base::Time expiration = base::Time::FromTimeT(
                                absl::ToTimeT(bsa_token.expiration));
                            return network::mojom::BlindSignedAuthToken::New(
                                bsa_token.token, expiration);
                          });

  TryGetAuthTokensComplete(absl::make_optional(std::move(bsa_tokens)),
                           std::move(callback),
                           IpProtectionTryGetAuthTokensResult::kSuccess);
}

void IpProtectionConfigProvider::TryGetAuthTokensComplete(
    absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
        bsa_tokens,
    TryGetAuthTokensCallback callback,
    IpProtectionTryGetAuthTokensResult result) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.TryGetAuthTokensResult", result);

  absl::optional<base::TimeDelta> backoff = CalculateBackoff(result);
  absl::optional<base::Time> try_again_after;
  if (backoff) {
    if (*backoff == base::TimeDelta::Max()) {
      try_again_after = base::Time::Max();
    } else {
      try_again_after = base::Time::Now() + *backoff;
    }
  }
  DCHECK(bsa_tokens.has_value() || try_again_after.has_value());
  std::move(callback).Run(std::move(bsa_tokens), try_again_after);
}

void IpProtectionConfigProvider::InvalidateNetworkContextTryAgainAfterTime() {
  if (!profile_) {
    // `profile_` will be nullptr if `Shutdown()` was called or if this is
    // called in unit tests.
    return;
  }
  // Notify the main profile network context to invalidate its stored backoff
  // time.
  profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->InvalidateIpProtectionConfigCacheTryAgainAfterTime();

  // If an associated incognito profile exists, tell it to invalidate its
  // backoff time as well.
  if (profile_->HasPrimaryOTRProfile()) {
    profile_->GetPrimaryOTRProfile(/*create_if_needed=*/false)
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->InvalidateIpProtectionConfigCacheTryAgainAfterTime();
  }
}

absl::optional<base::TimeDelta> IpProtectionConfigProvider::CalculateBackoff(
    IpProtectionTryGetAuthTokensResult result) {
  absl::optional<base::TimeDelta> backoff;
  bool exponential = false;
  switch (result) {
    case IpProtectionTryGetAuthTokensResult::kSuccess:
      break;
    case IpProtectionTryGetAuthTokensResult::kFailedNoAccount:
    case IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenPersistent:
      backoff = base::TimeDelta::Max();
      break;
    case IpProtectionTryGetAuthTokensResult::kFailedNotEligible:
      // TODO(https://crbug.com/1444621): When we add a client side account
      // capabilities check, if this capability/eligibility is something that
      // can change and be detected via callbacks to an overridden
      // `IdentityManager::Observer::OnExtendedAccountInfoUpdated()` method,
      // then update this failure so that we wait indefinitely as well (like the
      // cases above).
    case IpProtectionTryGetAuthTokensResult::kFailedBSA403:
      // Eligibility, whether determined locally or on the server, is unlikely
      // to change quickly.
      backoff = kNotEligibleBackoff;
      break;
    case IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenTransient:
    case IpProtectionTryGetAuthTokensResult::kFailedBSAOther:
      // Transient failure to fetch an OAuth token, or some other error from
      // BSA that is probably transient.
      backoff = kTransientBackoff;
      exponential = true;
      break;
    case IpProtectionTryGetAuthTokensResult::kFailedBSA400:
    case IpProtectionTryGetAuthTokensResult::kFailedBSA401:
      // Both 400 and 401 suggest a bug, so do not retry aggressively.
      backoff = kBugBackoff;
      exponential = true;
      break;
    case IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenDeprecated:
      NOTREACHED_NORETURN();
  }

  // Note that we calculate the backoff assuming that we've waited for
  // `last_try_get_auth_tokens_backoff_` time already, but this may not be the
  // case when:
  //  - Concurrent calls to `TryGetAuthTokens` from two network contexts are
  //  made and both fail in the same way
  //
  //  - A new incognito window is opened (the new network context won't know to
  //  backoff until after the first request)
  //
  //  - The network service restarts (the new network context(s) won't know to
  //  backoff until after the first request(s))
  //
  // We can't do much about the first case, but for the others we could track
  // the backoff time here and not request tokens again until afterward.
  //
  // TODO(https://crbug.com/1476891): Track the backoff time in the browser
  // process and don't make new requests if we are in a backoff period.
  if (exponential) {
    if (last_try_get_auth_tokens_backoff_ &&
        last_try_get_auth_tokens_result_ == result) {
      backoff = *last_try_get_auth_tokens_backoff_ * 2;
    }
  }

  // If the backoff is due to a user account issue, then only update the
  // backoff time based on account status changes (via the login observer) and
  // not based on the result of any `TryGetAuthTokens()` calls.
  if (last_try_get_auth_tokens_backoff_ &&
      *last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    return *last_try_get_auth_tokens_backoff_;
  }

  last_try_get_auth_tokens_result_ = result;
  last_try_get_auth_tokens_backoff_ = backoff;

  return backoff;
}

void IpProtectionConfigProvider::Shutdown() {
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  CHECK(identity_manager_);
  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
  profile_ = nullptr;
  // If we are shutting down, we can't process messages anymore because we rely
  // on having `identity_manager_` to get the OAuth token. Thus, just reset the
  // receiver set.
  receivers_.Clear();
  bsa_ = nullptr;
}

/*static*/
IpProtectionConfigProvider* IpProtectionConfigProvider::Get(Profile* profile) {
  return IpProtectionConfigProviderFactory::GetForProfile(profile);
}

void IpProtectionConfigProvider::AddReceiver(
    mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  receiver_id_for_testing_ = receivers_.Add(this, std::move(pending_receiver));

  // We only expect two concurrent receivers, one corresponding to the main
  // profile network context and one for an associated incognito mode profile
  // (if an incognito window is open). However, if the network service crashes
  // and is restarted, there might be lingering receivers that are bound until
  // they are eventually cleaned up.
}

void IpProtectionConfigProvider::ClearOAuthTokenProblemBackoff() {
  // End the backoff period if it was caused by account-related issues. Also,
  // tell the `IpProtectionConfigCache()` in the Network Service so that it
  // will begin making token requests.
  if (last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    last_try_get_auth_tokens_backoff_.reset();
    InvalidateNetworkContextTryAgainAfterTime();
  }
}

void IpProtectionConfigProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  auto signin_event_type = event.GetEventTypeFor(signin::ConsentLevel::kSignin);
  VLOG(2) << "IPATP::OnPrimaryAccountChanged kSignin event type: "
          << static_cast<int>(signin_event_type);
  switch (signin_event_type) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // Account information is now available, so resume making requests for the
      // OAuth token.
      ClearOAuthTokenProblemBackoff();
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      last_try_get_auth_tokens_backoff_ = base::TimeDelta::Max();
      // No need to tell the Network Service - it will find out the next time it
      // calls `TryGetAuthTokens()`.
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void IpProtectionConfigProvider::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  VLOG(2) << "IPATP::OnErrorStateOfRefreshTokenUpdatedForAccount: "
          << error.ToString();
  // Workspace user accounts can have account credential expirations that cause
  // persistent OAuth token errors until the user logs in to Chrome again. To
  // handle this, watch for these error events and treat them the same way we do
  // login/logout events.
  if (error.state() == GoogleServiceAuthError::State::NONE) {
    ClearOAuthTokenProblemBackoff();
    return;
  }
  if (error.IsPersistentError()) {
    last_try_get_auth_tokens_backoff_ = base::TimeDelta::Max();
    return;
  }
}
