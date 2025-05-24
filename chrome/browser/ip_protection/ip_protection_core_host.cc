// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ip_protection/ip_protection_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_direct_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/variations/service/variations_service.h"
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

namespace {

bool IsLikelyDogfoodClient() {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  return variations_service && variations_service->IsLikelyDogfoodClient();
}

}  // namespace
IpProtectionCoreHost::IpProtectionCoreHost(
    signin::IdentityManager* identity_manager,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    PrefService* pref_service,
    Profile* profile)
    : identity_manager_(identity_manager),
      tracking_protection_settings_(tracking_protection_settings),
      pref_service_(pref_service),
      profile_(profile) {
  CHECK(identity_manager);
  identity_manager_->AddObserver(this);
  CHECK(tracking_protection_settings);
  CHECK(pref_service_);
  tracking_protection_settings_->AddObserver(this);
}

void IpProtectionCoreHost::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (!ip_protection_token_fetcher_ || !ip_protection_proxy_config_fetcher_ ||
      !ip_protection_prt_fetcher_) {
    CHECK(profile_);
    url_loader_factory = profile_->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess();
  }
  if (!ip_protection_prt_fetcher_) {
    ip_protection_prt_fetcher_ = std::make_unique<
        ip_protection::IpProtectionProbabilisticRevealTokenDirectFetcher>(
        url_loader_factory->Clone(), chrome::GetChannel());
  }
  if (!ip_protection_token_fetcher_) {
    ip_protection_token_fetcher_ =
        std::make_unique<ip_protection::IpProtectionTokenDirectFetcher>(
            this, url_loader_factory->Clone());
  }
  if (!ip_protection_proxy_config_fetcher_) {
    ip_protection_proxy_config_fetcher_ =
        std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
            std::move(url_loader_factory),
            ip_protection::IpProtectionTokenFetcherHelper::kChromeIpBlinding,
            this);
  }
}

void IpProtectionCoreHost::SetUpForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface> bsa) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for_testing_ = true;

  // Carefully destroy any existing values in the correct order.
  ip_protection_proxy_config_fetcher_.reset();
  ip_protection_token_fetcher_.reset();
  ip_protection_prt_fetcher_.reset();

  ip_protection_prt_fetcher_ = std::make_unique<
      ip_protection::IpProtectionProbabilisticRevealTokenDirectFetcher>(
      url_loader_factory->Clone(), chrome::GetChannel());
  ip_protection_token_fetcher_ =
      std::make_unique<ip_protection::IpProtectionTokenDirectFetcher>(
          this, url_loader_factory->Clone(), std::move(bsa));
  ip_protection_proxy_config_fetcher_ =
      std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
          std::move(url_loader_factory),
          ip_protection::IpProtectionTokenFetcherHelper::kChromeIpBlinding,
          this);
}

IpProtectionCoreHost::~IpProtectionCoreHost() = default;

void IpProtectionCoreHost::TryGetAuthTokens(
    uint32_t batch_size,
    ip_protection::ProxyLayer proxy_layer,
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

  // The mojo callback requires `std::optional<..>&`, while the fetcher callback
  // provides arguments by value. This seemingly-redundant lambda converts the
  // two.
  auto callback_with_refs = base::BindOnce(
      [](TryGetAuthTokensCallback callback,
         std::optional<std::vector<ip_protection::BlindSignedAuthToken>> tokens,
         std::optional<::base::Time> try_again_after) {
        std::move(callback).Run(tokens, try_again_after);
      },
      std::move(callback));

  ip_protection_token_fetcher_->TryGetAuthTokens(batch_size, proxy_layer,
                                                 std::move(callback_with_refs));
}

void IpProtectionCoreHost::GetProxyConfig(GetProxyConfigCallback callback) {
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

  ip_protection_proxy_config_fetcher_->GetProxyConfig(base::BindOnce(
      // Convert the mojo style callback, which takes `const
      // std::optional<..>&`, to the preferred style, passing `std::optional` by
      // value.
      [](GetProxyConfigCallback callback,
         std::optional<std::vector<::net::ProxyChain>> proxy_chain,
         std::optional<ip_protection::GeoHint> geo_hint) {
        std::move(callback).Run(proxy_chain, geo_hint);
      },
      std::move(callback)));
}

void IpProtectionCoreHost::TryGetProbabilisticRevealTokens(
    TryGetProbabilisticRevealTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // If non-Chrome-branded bail out unless tests are running.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!for_testing_) {
    std::move(callback).Run(
        {}, ip_protection::TryGetProbabilisticRevealTokensResult{
                ip_protection::TryGetProbabilisticRevealTokensStatus::
                    kNoGoogleChromeBranding,
                static_cast<int32_t>(net::OK)});
    return;
  }
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  ip_protection_prt_fetcher_->TryGetProbabilisticRevealTokens(base::BindOnce(
      // Convert the mojo style callback, which takes `const T&` to the
      // preferred style, passing `T` by value.
      [](TryGetProbabilisticRevealTokensCallback callback,
         std::optional<ip_protection::TryGetProbabilisticRevealTokensOutcome>
             outcome,
         ip_protection::TryGetProbabilisticRevealTokensResult result) {
        std::move(callback).Run(outcome, result);
      },
      std::move(callback)));
}

void IpProtectionCoreHost::AuthenticateRequest(
    std::unique_ptr<network::ResourceRequest> resource_request,
    ip_protection::IpProtectionProxyConfigDirectFetcher::Delegate::
        AuthenticateRequestCallback callback) {
  // Apply either an OAuth token (which must be fetched) or an API key to the
  // request.
  if (net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.Get()) {
    if (!CanRequestOAuthToken()) {
      std::move(callback).Run(false, std::move(resource_request));
      return;
    }
    RequestOAuthTokenInternal(base::BindOnce(
        [](std::unique_ptr<network::ResourceRequest> resource_request,
           ip_protection::IpProtectionProxyConfigDirectFetcher::Delegate::
               AuthenticateRequestCallback callback,
           GoogleServiceAuthError error,
           signin::AccessTokenInfo access_token_info) {
          if (error.state() != GoogleServiceAuthError::NONE) {
            std::move(callback).Run(false, std::move(resource_request));
            return;
          }
          resource_request->headers.SetHeader(
              net::HttpRequestHeaders::kAuthorization,
              base::StrCat({"Bearer ", access_token_info.token}));
          std::move(callback).Run(true, std::move(resource_request));
        },
        std::move(resource_request), std::move(callback)));
  } else {
    google_apis::AddAPIKeyToRequest(
        *resource_request, google_apis::GetAPIKey(chrome::GetChannel()));
    std::move(callback).Run(true, std::move(resource_request));
  }
}

void IpProtectionCoreHost::RequestOAuthToken(
    ip_protection::IpProtectionTokenDirectFetcher::Delegate::
        RequestOAuthTokenCallback callback) {
  if (!CanRequestOAuthToken()) {
    std::move(callback).Run(TryGetAuthTokensResult::kFailedNoAccount,
                            std::nullopt);
    return;
  }

  // If the user is not eligible and not a dogfooder, do not even try to fetch
  // tokens. If unknown, fall back to trying anyway.
  const CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  CHECK(!account_id.empty());
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  bool cannot_use_chrome_ip_protection =
      !account_info.IsEmpty() &&
      account_info.capabilities.can_use_chrome_ip_protection() ==
          signin::Tribool::kFalse;
  if (cannot_use_chrome_ip_protection && !IsLikelyDogfoodClient()) {
    VLOG(2) << "RequestOAuthToken failed: not eligible";
    std::move(callback).Run(TryGetAuthTokensResult::kFailedNotEligible,
                            std::nullopt);
    return;
  }

  RequestOAuthTokenInternal(base::BindOnce(
      [](ip_protection::IpProtectionTokenDirectFetcher::Delegate::
             RequestOAuthTokenCallback callback,
         GoogleServiceAuthError error,
         signin::AccessTokenInfo access_token_info) {
        if (error.state() != GoogleServiceAuthError::NONE) {
          VLOG(2) << "RequestOAuthToken got an error: "
                  << static_cast<int>(error.state());
          std::move(callback).Run(
              error.IsTransientError()
                  ? TryGetAuthTokensResult::kFailedOAuthTokenTransient
                  : TryGetAuthTokensResult::kFailedOAuthTokenPersistent,
              std::nullopt);
          return;
        }
        std::optional<std::string> access_token = access_token_info.token;
        std::move(callback).Run(TryGetAuthTokensResult::kSuccess, access_token);
      },
      std::move(callback)));
}

void IpProtectionCoreHost::RequestOAuthTokenInternal(
    RequestOAuthTokenInternalCallback callback) {
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
  oauth_token_fetcher_ptr->Start(base::BindOnce(
      [](std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
             oauth_token_fetcher,
         RequestOAuthTokenInternalCallback callback,
         GoogleServiceAuthError error,
         signin::AccessTokenInfo access_token_info) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
        std::move(callback).Run(error, access_token_info);
      },

      std::move(oauth_token_fetcher), std::move(callback)));
}

void IpProtectionCoreHost::InvalidateNetworkContextTryAgainAfterTime() {
  if (!profile_) {
    // `profile_` will be nullptr if `Shutdown()` was called or if this is
    // called in unit tests.
    return;
  }

  for (auto& ipp_control : remotes_) {
    ipp_control->AuthTokensMayBeAvailable();
  }
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
  ip_protection_token_fetcher_.reset();
  ip_protection_prt_fetcher_.reset();
  // If we are shutting down, we can't process messages anymore because we
  // rely on having `identity_manager_` to get the OAuth token. Thus, just
  // reset the receiver set.
  receivers_.Clear();
}

void IpProtectionCoreHost::AddNetworkService(
    mojo::PendingReceiver<ip_protection::mojom::CoreHost> pending_receiver,
    mojo::PendingRemote<ip_protection::mojom::CoreControl> pending_remote) {
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

void IpProtectionCoreHost::AccountStatusChanged(bool account_available) {
  if (ip_protection_prt_fetcher_) {
    ip_protection_prt_fetcher_->AccountStatusChanged(account_available);
  }

  if (ip_protection_proxy_config_fetcher_) {
    ip_protection_proxy_config_fetcher_->AccountStatusChanged(
        account_available);
  }

  if (ip_protection_token_fetcher_) {
    ip_protection_token_fetcher_->AccountStatusChanged(account_available);
  }

  // Tell the `IpProtectionConfigCache()` in the Network Service so that it
  // will begin making token requests.
  if (account_available) {
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
      AccountStatusChanged(true);
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // No need to tell the Network Service - it will find out the next time
      // it calls `TryGetAuthTokens()`.
      AccountStatusChanged(false);
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
    AccountStatusChanged(true);
    return;
  }
  if (error.IsPersistentError()) {
    AccountStatusChanged(false);
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

bool IpProtectionCoreHost::ShouldDisableIpProtectionForEnterpriseForTesting() {
  return ShouldDisableIpProtectionForEnterprise();
}

bool IpProtectionCoreHost::ShouldDisableIpProtectionForEnterprise() {
  return tracking_protection_settings_->IsIpProtectionDisabledForEnterprise();
}

bool IpProtectionCoreHost::IsIpProtectionEnabled() {
  if (is_shutting_down_) {
    return false;
  }

  if (ShouldDisableIpProtectionForEnterprise()) {
    return false;
  }

  // TODO(crbug.com/41494110): We should ultimately use
  // `tracking_protection_settings_->IsIpProtectionEnabled()` but we can't yet
  // because it would prevent us from being able to do experiments via Finch
  // without showing the user setting.
  if (!base::FeatureList::IsEnabled(privacy_sandbox::kIpProtectionUx)) {
    // If the preference isn't visible to users then IP Protection is enabled
    // via other means like via Finch experiment.
    return true;
  }
  return tracking_protection_settings_->IsIpProtectionEnabled();
}

bool IpProtectionCoreHost::IsProxyConfigFetchEnabled() {
  return IsIpProtectionEnabled();
}

bool IpProtectionCoreHost::IsTokenFetchEnabled() {
  return IsIpProtectionEnabled();
}

void IpProtectionCoreHost::OnIpProtectionEnabledChanged() {
  if (is_shutting_down_) {
    return;
  }

  if (IsIpProtectionEnabled()) {
    AccountStatusChanged(true);
  }

  for (auto& ipp_control : remotes_) {
    ipp_control->SetIpProtectionEnabled(IsIpProtectionEnabled());
  }
}
