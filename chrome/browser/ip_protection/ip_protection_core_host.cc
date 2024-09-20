// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ip_protection/ip_protection_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_direct_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

using ::ip_protection::TryGetAuthTokensResult;

IpProtectionCoreHost::IpProtectionCoreHost(
    signin::IdentityManager* identity_manager,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    PrefService* pref_service,
    Profile* profile)
    : identity_manager_(identity_manager),
      tracking_protection_settings_(tracking_protection_settings),
      pref_service_(pref_service),
      profile_(profile),
      token_fetcher_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  CHECK(identity_manager);
  identity_manager_->AddObserver(this);
  CHECK(tracking_protection_settings);
  CHECK(pref_service_);
  tracking_protection_settings_->AddObserver(this);
}

void IpProtectionCoreHost::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ip_protection_token_direct_fetcher_) {
    if (!url_loader_factory_) {
      CHECK(profile_);
      url_loader_factory_ = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
    }
    ip_protection_token_direct_fetcher_ =
        base::SequenceBound<ip_protection::IpProtectionTokenDirectFetcher>(
            token_fetcher_task_runner_, url_loader_factory_->Clone());
  }
  if (!ip_protection_proxy_config_fetcher_) {
    ip_protection_proxy_config_fetcher_ =
        std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
            url_loader_factory_.get(),
            ip_protection::IpProtectionCoreHostHelper::kChromeIpBlinding,
            base::BindRepeating(&IpProtectionCoreHost::AuthenticateCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void IpProtectionCoreHost::SetUpForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface> bsa) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for_testing_ = true;

  // Carefully destroy any existing values in the correct order.
  ip_protection_proxy_config_fetcher_ = nullptr;
  ip_protection_token_direct_fetcher_.Reset();
  url_loader_factory_ = nullptr;

  ip_protection_token_direct_fetcher_ =
      base::SequenceBound<ip_protection::IpProtectionTokenDirectFetcher>(
          token_fetcher_task_runner_, url_loader_factory->Clone(),
          std::move(bsa));
  ip_protection_proxy_config_fetcher_ =
      std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
          std::move(url_loader_factory),
          ip_protection::IpProtectionCoreHostHelper::kChromeIpBlinding,
          base::BindRepeating(&IpProtectionCoreHost::AuthenticateCallback,
                              weak_ptr_factory_.GetWeakPtr()));
}

IpProtectionCoreHost::~IpProtectionCoreHost() = default;

void IpProtectionCoreHost::TryGetAuthTokens(
    uint32_t batch_size,
    network::mojom::IpProtectionProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // The `batch_size` is cast to an `int` for use by BlindSignAuth, so check
  // for overflow here.
  if (batch_size == 0 || batch_size > INT_MAX) {
    mojo::ReportBadMessage("Invalid batch_size");
    return;
  }

  // If IP Protection is disabled via user settings then don't attempt to fetch
  // tokens.
  if (!IsIpProtectionEnabled()) {
    TryGetAuthTokensComplete(std::nullopt, std::move(callback),
                             TryGetAuthTokensResult::kFailedDisabledByUser);
    return;
  }

  // If we are in a state where the OAuth token has persistent errors then don't
  // try to request tokens.
  if (last_try_get_auth_tokens_backoff_ &&
      *last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    TryGetAuthTokensComplete(std::nullopt, std::move(callback),
                             TryGetAuthTokensResult::kFailedNoAccount);
    return;
  }

  if (!CanRequestOAuthToken()) {
    TryGetAuthTokensComplete(std::nullopt, std::move(callback),
                             TryGetAuthTokensResult::kFailedNoAccount);
    return;
  }

  auto oauth_token_fetch_start_time = base::TimeTicks::Now();
  auto quiche_proxy_layer =
      proxy_layer == network::mojom::IpProtectionProxyLayer::kProxyA
          ? quiche::ProxyLayer::kProxyA
          : quiche::ProxyLayer::kProxyB;
  auto request_token_callback = base::BindOnce(
      &IpProtectionCoreHost::OnRequestOAuthTokenCompletedForTryGetAuthTokens,
      weak_ptr_factory_.GetWeakPtr(), batch_size, quiche_proxy_layer,
      std::move(callback), oauth_token_fetch_start_time);

  RequestOAuthToken(std::move(request_token_callback));
}

void IpProtectionCoreHost::GetProxyList(GetProxyListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // Neither the API key nor the OAuth token will be available to
  // non-Chrome-branded builds, so unless we are testing, bail out.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!for_testing_) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // If IP Protection is disabled via user settings then don't attempt to get a
  // proxy list.
  // TODO(crbug.com/41494110): We don't currently prevent GetProxyList
  // calls from being made from the network service once the user has disabled
  // the feature, so for now we will fail all of these requests here (and rely
  // on rate-limiting by the network service to prevent the browser process from
  // being flooded with messages). We are currently planning to move the
  // GetProxyList calls to be made in the network service directly, so once that
  // happens it should obviate the need for a long-term solution here. If that
  // plan changes, though, we should implement a way for these requests to stop
  // being made.
  if (!IsIpProtectionEnabled()) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  // If we are not able to call `GetProxyConfig` yet, return early.
  if (ip_protection_proxy_config_fetcher_->GetNoGetProxyConfigUntilTime() >
      base::Time::Now()) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  ip_protection_proxy_config_fetcher_->GetProxyConfig(std::move(callback));
}

void IpProtectionCoreHost::AuthenticateCallback(
    std::unique_ptr<network::ResourceRequest> resource_request,
    ip_protection::IpProtectionProxyConfigDirectFetcher::
        AuthenticateDoneCallback callback) {
  // Apply either an OAuth token (which must be fetched) or an API key to the
  // request.
  if (net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.Get()) {
    if (!CanRequestOAuthToken()) {
      std::move(callback).Run(false, std::move(resource_request));
      return;
    }
    auto request_token_callback = base::BindOnce(
        &IpProtectionCoreHost::OnRequestOAuthTokenCompletedForGetProxyConfig,
        weak_ptr_factory_.GetWeakPtr(), std::move(resource_request),
        std::move(callback));

    RequestOAuthToken(std::move(request_token_callback));
  } else {
    google_apis::AddAPIKeyToRequest(
        *resource_request, google_apis::GetAPIKey(chrome::GetChannel()));
    std::move(callback).Run(true, std::move(resource_request));
  }
}

void IpProtectionCoreHost::RequestOAuthToken(
    RequestOAuthTokenCallback callback) {
  // TODO(crbug.com/40267788): Add a client side account capabilities
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
  auto oauth_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"IpProtectionService", identity_manager_, scopes,
          mode, signin::ConsentLevel::kSignin);
  auto* oauth_token_fetcher_ptr = oauth_token_fetcher.get();
  oauth_token_fetcher_ptr->Start(
      base::BindOnce(&IpProtectionCoreHost::OnRequestOAuthTokenCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(oauth_token_fetcher), std::move(callback)));
}

void IpProtectionCoreHost::OnRequestOAuthTokenCompleted(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
        oauth_token_fetcher,
    RequestOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }

  std::move(callback).Run(error, access_token_info);
}

void IpProtectionCoreHost::OnRequestOAuthTokenCompletedForTryGetAuthTokens(
    uint32_t batch_size,
    quiche::ProxyLayer quiche_proxy_layer,
    TryGetAuthTokensCallback callback,
    base::TimeTicks oauth_token_fetch_start_time,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (error.state() != GoogleServiceAuthError::NONE) {
    VLOG(2) << "IPATP::OnRequestOAuthTokenCompletedForTryGetAuthTokens got an "
               "error: "
            << static_cast<int>(error.state());
    TryGetAuthTokensComplete(
        std::nullopt, std::move(callback),
        error.IsTransientError()
            ? TryGetAuthTokensResult::kFailedOAuthTokenTransient
            : TryGetAuthTokensResult::kFailedOAuthTokenPersistent);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  ip_protection::Telemetry().OAuthTokenFetchComplete(
      current_time - oauth_token_fetch_start_time);
  FetchBlindSignedToken(access_token_info, batch_size, quiche_proxy_layer,
                        std::move(callback));
}

void IpProtectionCoreHost::OnRequestOAuthTokenCompletedForGetProxyConfig(
    std::unique_ptr<network::ResourceRequest> resource_request,
    ip_protection::IpProtectionProxyConfigDirectFetcher::
        AuthenticateDoneCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    VLOG(2) << "IPATP::OnRequestOAuthTokenCompletedForGetProxyConfig failed: "
            << static_cast<int>(error.state());
    std::move(callback).Run(false, std::move(resource_request));
    return;
  }
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_info.token}));
  std::move(callback).Run(true, std::move(resource_request));
}

void IpProtectionCoreHost::FetchBlindSignedToken(
    std::optional<signin::AccessTokenInfo> access_token_info,
    uint32_t batch_size,
    quiche::ProxyLayer quiche_proxy_layer,
    TryGetAuthTokensCallback callback) {
  std::optional<std::string> access_token = access_token_info.value().token;
  auto bsa_get_tokens_start_time = base::TimeTicks::Now();
  ip_protection_token_direct_fetcher_
      .AsyncCall(
          &ip_protection::IpProtectionTokenDirectFetcher::FetchBlindSignedToken)
      .WithArgs(std::move(access_token), batch_size, quiche_proxy_layer,
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &IpProtectionCoreHost::OnFetchBlindSignedTokenCompleted,
                    weak_ptr_factory_.GetWeakPtr(), bsa_get_tokens_start_time,
                    std::move(callback))));
}

void IpProtectionCoreHost::OnFetchBlindSignedTokenCompleted(
    base::TimeTicks bsa_get_tokens_start_time,
    TryGetAuthTokensCallback callback,
    absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens) {
  using enum TryGetAuthTokensResult;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  if (!tokens.ok()) {
    // Apply the canonical mapping from abseil status to HTTP status.
    TryGetAuthTokensResult result;
    switch (tokens.status().code()) {
      case absl::StatusCode::kInvalidArgument:
        result = kFailedBSA400;
        break;
      case absl::StatusCode::kUnauthenticated:
        result = kFailedBSA401;
        break;
      case absl::StatusCode::kPermissionDenied:
        result = kFailedBSA403;
        break;
      default:
        result = kFailedBSAOther;
        break;
    }
    base::UmaHistogramSparse(
        "NetworkService.IpProtection.TryGetAuthTokensErrors",
        base::PersistentHash(tokens.status().ToString()));
    VLOG(2) << "IPATP::OnFetchBlindSignedTokenCompleted got an error: "
            << static_cast<int>(result);
    TryGetAuthTokensComplete(std::nullopt, std::move(callback), result);
    return;
  }

  if (tokens.value().size() == 0) {
    VLOG(2) << "IPATP::OnFetchBlindSignedTokenCompleted called with no tokens";
    TryGetAuthTokensComplete(std::nullopt, std::move(callback),
                             kFailedBSAOther);
    return;
  }

  std::vector<ip_protection::BlindSignedAuthToken> bsa_tokens;
  for (const quiche::BlindSignToken& token : tokens.value()) {
    std::optional<ip_protection::BlindSignedAuthToken> converted_token =
        ip_protection::IpProtectionCoreHostHelper::CreateBlindSignedAuthToken(
            token);
    if (!converted_token.has_value() || converted_token->token.empty()) {
      TryGetAuthTokensComplete(std::nullopt, std::move(callback),
                               kFailedBSAOther);
      return;
    } else {
      bsa_tokens.push_back(std::move(converted_token).value());
    }
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  TryGetAuthTokensComplete(std::make_optional(std::move(bsa_tokens)),
                           std::move(callback), kSuccess,
                           current_time - bsa_get_tokens_start_time);
}

void IpProtectionCoreHost::TryGetAuthTokensComplete(
    std::optional<std::vector<ip_protection::BlindSignedAuthToken>> bsa_tokens,
    TryGetAuthTokensCallback callback,
    TryGetAuthTokensResult result,
    std::optional<base::TimeDelta> duration) {
  ip_protection::Telemetry().TokenBatchFetchComplete(result, duration);

  std::optional<base::TimeDelta> backoff = CalculateBackoff(result);
  std::optional<base::Time> try_again_after;
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

void IpProtectionCoreHost::InvalidateNetworkContextTryAgainAfterTime() {
  if (!profile_) {
    // `profile_` will be nullptr if `Shutdown()` was called or if this is
    // called in unit tests.
    return;
  }

  for (auto& ipp_control : remotes_) {
    ipp_control->InvalidateIpProtectionConfigCacheTryAgainAfterTime();
  }
}

std::optional<base::TimeDelta> IpProtectionCoreHost::CalculateBackoff(
    TryGetAuthTokensResult result) {
  using enum TryGetAuthTokensResult;
  std::optional<base::TimeDelta> backoff;
  bool exponential = false;
  switch (result) {
    case kSuccess:
      break;
    case kFailedNoAccount:
    case kFailedOAuthTokenPersistent:
    case kFailedDisabledByUser:
      backoff = base::TimeDelta::Max();
      break;
    case kFailedNotEligible:
      // TODO(crbug.com/40267788): When we add a client side account
      // capabilities check, if this capability/eligibility is something that
      // can change and be detected via callbacks to an overridden
      // `IdentityManager::Observer::OnExtendedAccountInfoUpdated()` method,
      // then update this failure so that we wait indefinitely as well (like
      // the cases above).
    case kFailedBSA403:
      // Eligibility, whether determined locally or on the server, is unlikely
      // to change quickly.
      backoff = ip_protection::IpProtectionCoreHostHelper::kNotEligibleBackoff;
      break;
    case kFailedOAuthTokenTransient:
    case kFailedBSAOther:
      // Transient failure to fetch an OAuth token, or some other error from
      // BSA that is probably transient.
      backoff = ip_protection::IpProtectionCoreHostHelper::kTransientBackoff;
      exponential = true;
      break;
    case kFailedBSA400:
    case kFailedBSA401:
      // Both 400 and 401 suggest a bug, so do not retry aggressively.
      backoff = ip_protection::IpProtectionCoreHostHelper::kBugBackoff;
      exponential = true;
      break;
    case kFailedOAuthTokenDeprecated:
      NOTREACHED_NORETURN();
  }

  // Note that we calculate the backoff assuming that we've waited for
  // `last_try_get_auth_tokens_backoff_` time already, but this may not be the
  // case when:
  //  - Concurrent calls to `TryGetAuthTokens` from two network contexts are
  //  made and both fail in the same way
  //
  //  - A new incognito window is opened (the new network context won't know
  //  to backoff until after the first request)
  //
  //  - The network service restarts (the new network context(s) won't know to
  //  backoff until after the first request(s))
  //
  // We can't do much about the first case, but for the others we could track
  // the backoff time here and not request tokens again until afterward.
  //
  // TODO(crbug.com/40280126): Track the backoff time in the browser
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

void IpProtectionCoreHost::Shutdown() {
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  CHECK(identity_manager_);
  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
  CHECK(tracking_protection_settings_);
  tracking_protection_settings_->RemoveObserver(this);
  tracking_protection_settings_ = nullptr;
  pref_service_ = nullptr;
  profile_ = nullptr;
  ip_protection_token_direct_fetcher_.Reset();
  // If we are shutting down, we can't process messages anymore because we
  // rely on having `identity_manager_` to get the OAuth token. Thus, just
  // reset the receiver set.
  receivers_.Clear();
}

/*static*/
IpProtectionCoreHost* IpProtectionCoreHost::Get(Profile* profile) {
  return IpProtectionCoreHostFactory::GetForProfile(profile);
}

void IpProtectionCoreHost::AddNetworkService(
    mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
        pending_receiver,
    mojo::PendingRemote<network::mojom::IpProtectionControl> pending_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  receiver_id_for_testing_ = receivers_.Add(this, std::move(pending_receiver));
  remote_id_for_testing_ = remotes_.Add(std::move(pending_remote));

  // We only expect two concurrent receivers, one corresponding to the main
  // profile network context and one for an associated incognito mode profile
  // (if an incognito window is open). However, if the network service crashes
  // and is restarted, there might be lingering receivers that are bound until
  // they are eventually cleaned up.
}

void IpProtectionCoreHost::ClearOAuthTokenProblemBackoff() {
  // End the backoff period if it was caused by account-related issues. Also,
  // tell the `IpProtectionConfigCache()` in the Network Service so that it
  // will begin making token requests.
  if (last_try_get_auth_tokens_backoff_ == base::TimeDelta::Max()) {
    last_try_get_auth_tokens_backoff_.reset();
    InvalidateNetworkContextTryAgainAfterTime();
  }
}

void IpProtectionCoreHost::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  auto signin_event_type = event.GetEventTypeFor(signin::ConsentLevel::kSignin);
  VLOG(2) << "IPATP::OnPrimaryAccountChanged kSignin event type: "
          << static_cast<int>(signin_event_type);
  switch (signin_event_type) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // Account information is now available, so resume making requests for
      // the OAuth token.
      ClearOAuthTokenProblemBackoff();
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      last_try_get_auth_tokens_backoff_ = base::TimeDelta::Max();
      // No need to tell the Network Service - it will find out the next time
      // it calls `TryGetAuthTokens()`.
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void IpProtectionCoreHost::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  VLOG(2) << "IPATP::OnErrorStateOfRefreshTokenUpdatedForAccount: "
          << error.ToString();
  // Workspace user accounts can have account credential expirations that
  // cause persistent OAuth token errors until the user logs in to Chrome
  // again. To handle this, watch for these error events and treat them the
  // same way we do login/logout events.
  if (error.state() == GoogleServiceAuthError::State::NONE) {
    ClearOAuthTokenProblemBackoff();
    return;
  }
  if (error.IsPersistentError()) {
    last_try_get_auth_tokens_backoff_ = base::TimeDelta::Max();
    return;
  }
}

bool IpProtectionCoreHost::CanRequestOAuthToken() {
  if (is_shutting_down_) {
    return false;
  }

  return identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

// static
bool IpProtectionCoreHost::CanIpProtectionBeEnabled() {
  return base::FeatureList::IsEnabled(
             net::features::kEnableIpProtectionProxy) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableIpProtectionProxy);
}

bool IpProtectionCoreHost::IsIpProtectionEnabled() {
  if (is_shutting_down_) {
    return false;
  }

  // If the user's enterprise has a policy for IP, use this regardless of user
  // UX feature status. Enterprises should have the ability to enable or
  // disable IPP even when users do not have UX access to the feature.
  if (pref_service_->IsManagedPreference(prefs::kIpProtectionEnabled)) {
    return pref_service_->GetBoolean(prefs::kIpProtectionEnabled);
  }

  // TODO(crbug.com/41494110): We should ultimately use
  // `tracking_protection_settings_->IsIpProtectionEnabled()` but we can't yet
  // because it would prevent us from being able to do experiments via Finch
  // without showing the user setting.
  if (!base::FeatureList::IsEnabled(privacy_sandbox::kIpProtectionV1)) {
    // If the preference isn't visible to users then IP Protection is enabled
    // via other means like via Finch experiment.
    return true;
  }
  return tracking_protection_settings_->IsIpProtectionEnabled();
}

void IpProtectionCoreHost::OnIpProtectionEnabledChanged() {
  if (is_shutting_down_) {
    return;
  }

  ClearOAuthTokenProblemBackoff();

  for (auto& ipp_control : remotes_) {
    ipp_control->SetIpProtectionEnabled(IsIpProtectionEnabled());
  }
}
