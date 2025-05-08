// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CORE_HOST_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CORE_HOST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/ip_protection_core_host_factory.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_direct_fetcher.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings_observer.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/status/status.h"

class Profile;

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
}  // namespace quiche

// Fetches IP protection tokens on demand for the network service.
//
// This class handles both requesting OAuth2 tokens for the signed-in user, and
// fetching blind-signed auth tokens for that user. It may only be used on the
// UI thread.
class IpProtectionCoreHost
    : public KeyedService,
      public ip_protection::mojom::CoreHost,
      public signin::IdentityManager::Observer,
      public privacy_sandbox::TrackingProtectionSettingsObserver,
      public ip_protection::IpProtectionProxyConfigDirectFetcher::Delegate,
      public ip_protection::IpProtectionTokenDirectFetcher::Delegate {
 public:
  IpProtectionCoreHost(
      signin::IdentityManager* identity_manager,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      PrefService* pref_service,
      Profile* profile);

  ~IpProtectionCoreHost() override;

  // CoreHost implementation:

  // Get a batch of blind-signed auth tokens. It is forbidden for two calls to
  // this method for the same proxy layer to be outstanding at the same time.
  void TryGetAuthTokens(uint32_t batch_size,
                        ip_protection::ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;
  void GetProxyConfig(GetProxyConfigCallback callback) override;
  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override;

  static bool CanIpProtectionBeEnabled();
  bool IsIpProtectionEnabled();
  bool CanRequestOAuthToken();

  // IpProtectionTokenDirectFetcher::Delegate implementation.
  bool IsTokenFetchEnabled() override;
  void RequestOAuthToken(
      ip_protection::IpProtectionTokenDirectFetcher::Delegate::
          RequestOAuthTokenCallback callback) override;

  // IpProtectionProxyConfigDirectFetcher::Delegate implementation.
  bool IsProxyConfigFetchEnabled() override;
  void AuthenticateRequest(std::unique_ptr<network::ResourceRequest>,
                           ip_protection::IpProtectionProxyConfigDirectFetcher::
                               Delegate::AuthenticateRequestCallback) override;
  // Add bidirectional pipes to a new network service.
  void AddNetworkService(
      mojo::PendingReceiver<ip_protection::mojom::CoreHost> pending_receiver,
      mojo::PendingRemote<ip_protection::mojom::CoreControl> pending_remote);

  // KeyedService:
  void Shutdown() override;

  mojo::ReceiverSet<ip_protection::mojom::CoreHost>& receivers_for_testing() {
    return receivers_;
  }
  mojo::ReceiverId receiver_id_for_testing() {
    return receiver_id_for_testing_;
  }
  ip_protection::mojom::CoreControl* last_remote_for_testing() {
    return remotes_.Get(remote_id_for_testing_);
  }

  // Like `SetUp()`, but providing values for each of the member variables. Note
  // `bsa` is moved onto a separate sequence when initializing
  // `ip_protection_token_fetcher_`.
  void SetUpForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface> bsa);

  // Returns whether IP Protection should be disabled for managed users and/or
  // devices, for testing.
  bool ShouldDisableIpProtectionForEnterpriseForTesting();

 private:
  friend class IpProtectionCoreHostTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionCoreHostIdentityBrowserTest,
                           BackoffTimeResetAfterProfileAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(IpProtectionCoreHostUserSettingBrowserTest,
                           OnIpProtectionEnabledChanged);

  // Creating a generic callback in order for `RequestOAuthToken()` to work for
  // `TryGetAuthTokens()` and `GetProxyConfig()`.
  using RequestOAuthTokenInternalCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info)>;

  // Set up `ip_protection_proxy_config_fetcher_`,
  // `ip_protection_token_fetcher_` and
  // `ip_protection_prt_fetcher_` if
  // not already initialized. This accomplishes lazy loading of these components
  // to break dependency loops in browser startup.
  void SetUp();

  // `FetchBlindSignedToken()` uses the
  // `ip_protection_token_fetcher_` to make an async call on the
  // bound sequence into the `quiche::BlindSignAuth` library to request a
  // blind-signed auth token for use at the IP Protection proxies.
  void FetchBlindSignedToken(
      std::optional<signin::AccessTokenInfo> access_token_info,
      uint32_t batch_size,
      quiche::ProxyLayer quiche_proxy_layer,
      TryGetAuthTokensCallback callback);

  void OnFetchBlindSignedTokenCompleted(
      base::TimeTicks bsa_get_tokens_start_time,
      TryGetAuthTokensCallback callback,
      absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens);

  // Calls the IdentityManager asynchronously to request the OAuth token for the
  // logged in user. This method must only be called when
  // `CanRequestOAuthToken()` returns true.
  void RequestOAuthTokenInternal(RequestOAuthTokenInternalCallback callback);

  // The status of the account has changed, either becoming available or
  // becoming unavailable. This is a signal to reset various timeouts (if
  // available) or extend them (if not).
  void AccountStatusChanged(bool account_available);

  // Returns whether IP Protection should be disabled for managed users and/or
  // devices.
  bool ShouldDisableIpProtectionForEnterprise();

  // Instruct the `IpProtectionConfigCache()`(s) in the Network Service to
  // ignore any previously sent `try_again_after` times.
  void InvalidateNetworkContextTryAgainAfterTime();

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  // TrackingProtectionSettingsObserver:
  void OnIpProtectionEnabledChanged() override;

  // The object used to get an OAuth token. `identity_manager_` will be set to
  // nullptr after `Shutdown()` is called, but will otherwise be non-null.
  raw_ptr<signin::IdentityManager> identity_manager_;
  // Used to retrieve whether the user has enabled IP protection via settings.
  // `tracking_protection_settings_` will be set to nullptr after `Shutdown()`
  // is called, but will otherwise be non-null.
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  // Used to request the state of the IP Protection user setting. Will be set to
  // nullptr after `Shutdown()` is called.
  raw_ptr<PrefService> pref_service_;
  // The `Profile` object associated with this `IpProtectionCoreHost()`. Will be
  // reset to nullptr after `Shutdown()` is called.
  // NOTE: If this is used in any `GetForProfile()` call, ensure that there is a
  // corresponding dependency (if needed) registered in the factory class.
  raw_ptr<Profile> profile_;

  std::unique_ptr<
      ip_protection::IpProtectionProbabilisticRevealTokenDirectFetcher>
      ip_protection_prt_fetcher_;
  std::unique_ptr<ip_protection::IpProtectionProxyConfigDirectFetcher>
      ip_protection_proxy_config_fetcher_;
  std::unique_ptr<ip_protection::IpProtectionTokenDirectFetcher>
      ip_protection_token_fetcher_;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The `mojo::Receiver` objects allowing the network service to call methods
  // on `this`.
  //
  // At any given time there should only be two receivers, one for the main
  // profile and another one if an associated incognito window is opened.
  // If one of the corresponding Network Contexts restarts, the
  // corresponding receiver will automatically be removed and a new one
  // bound as part of the Network Context initialization flow.
  mojo::ReceiverSet<ip_protection::mojom::CoreHost> receivers_;

  // Similar to `receivers_`, but containing remotes for all existing
  // IpProtectionProxyDelegates.
  mojo::RemoteSet<ip_protection::mojom::CoreControl> remotes_;

  // The `mojo::ReceiverId` of the most recently added `mojo::Receiver`, for
  // testing.
  mojo::ReceiverId receiver_id_for_testing_;

  // The `mojo::RemoteSetElementId` of the most recently added `mojo::Remote`,
  // for testing.
  mojo::RemoteSetElementId remote_id_for_testing_;

  // True if this class is being tested.
  bool for_testing_ = false;

  // This must be the last member in this class.
  base::WeakPtrFactory<IpProtectionCoreHost> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CORE_HOST_H_
