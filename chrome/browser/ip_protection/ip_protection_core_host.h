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
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/ip_protection_core_host_factory.h"
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_direct_fetcher.h"
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
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"

class Profile;

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
struct BlindSignToken;
}  // namespace quiche

// Fetches IP protection tokens on demand for the network service.
//
// This class handles both requesting OAuth2 tokens for the signed-in user, and
// fetching blind-signed auth tokens for that user. It may only be used on the
// UI thread.
class IpProtectionCoreHost
    : public KeyedService,
      public network::mojom::IpProtectionConfigGetter,
      public signin::IdentityManager::Observer,
      public privacy_sandbox::TrackingProtectionSettingsObserver {
 public:
  IpProtectionCoreHost(
      signin::IdentityManager* identity_manager,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      PrefService* pref_service,
      Profile* profile);

  ~IpProtectionCoreHost() override;

  // IpProtectionConfigGetter:

  // Get a batch of blind-signed auth tokens. It is forbidden for two calls to
  // this method for the same proxy layer to be outstanding at the same time.
  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;
  // Get the list of IP Protection proxies.
  void GetProxyList(GetProxyListCallback callback) override;

  static bool CanIpProtectionBeEnabled();

  // Checks if IP Protection is disabled via user settings.
  bool IsIpProtectionEnabled();

  // Add bidirectional pipes to a new network service.
  void AddNetworkService(
      mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
          pending_receiver,
      mojo::PendingRemote<network::mojom::IpProtectionControl> pending_remote);

  // KeyedService:
  void Shutdown() override;

  static IpProtectionCoreHost* Get(Profile* profile);

  mojo::ReceiverSet<network::mojom::IpProtectionConfigGetter>&
  receivers_for_testing() {
    return receivers_;
  }
  mojo::ReceiverId receiver_id_for_testing() {
    return receiver_id_for_testing_;
  }
  network::mojom::IpProtectionControl* last_remote_for_testing() {
    return remotes_.Get(remote_id_for_testing_);
  }

  // Like `SetUp()`, but providing values for each of the member variables. Note
  // `bsa` is moved onto a separate sequence when initializing
  // `ip_protection_token_direct_fetcher_`.
  void SetUpForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface> bsa);

 private:
  friend class IpProtectionCoreHostTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionCoreHostTest, CalculateBackoff);
  FRIEND_TEST_ALL_PREFIXES(IpProtectionCoreHostIdentityBrowserTest,
                           BackoffTimeResetAfterProfileAvailabilityChange);
  FRIEND_TEST_ALL_PREFIXES(IpProtectionCoreHostUserSettingBrowserTest,
                           OnIpProtectionEnabledChanged);

  // Set up `ip_protection_proxy_config_fetcher_`,
  // `ip_protection_token_direct_fetcher_` and `url_loader_factory_`, if
  // not already initialized. This accomplishes lazy loading of these components
  // to break dependency loops in browser startup.
  void SetUp();

  // `FetchBlindSignedToken()` uses the
  // `ip_protection_token_direct_fetcher_` to make an async call on the
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

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      std::optional<std::vector<ip_protection::BlindSignedAuthToken>>
          bsa_tokens,
      TryGetAuthTokensCallback callback,
      ip_protection::TryGetAuthTokensResult result,
      std::optional<base::TimeDelta> duration = std::nullopt);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  std::optional<base::TimeDelta> CalculateBackoff(
      ip_protection::TryGetAuthTokensResult result);

  void AuthenticateCallback(
      std::unique_ptr<network::ResourceRequest>,
      ip_protection::IpProtectionProxyConfigDirectFetcher::
          AuthenticateDoneCallback);

  // Creating a generic callback in order for `RequestOAuthToken()` to work for
  // `TryGetAuthTokens()` and `GetProxyList()`.
  using RequestOAuthTokenCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info)>;
  // Calls the IdentityManager asynchronously to request the OAuth token for the
  // logged in user. This method must only be called when
  // `CanRequestOAuthToken()` returns true.
  void RequestOAuthToken(RequestOAuthTokenCallback callback);
  bool CanRequestOAuthToken();

  void OnRequestOAuthTokenCompleted(
      std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
          oauth_token_fetcher,
      RequestOAuthTokenCallback callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  void OnRequestOAuthTokenCompletedForTryGetAuthTokens(
      uint32_t batch_size,
      quiche::ProxyLayer quiche_proxy_layer,
      TryGetAuthTokensCallback callback,
      base::TimeTicks oauth_token_fetch_start_time,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  void OnRequestOAuthTokenCompletedForGetProxyConfig(
      std::unique_ptr<network::ResourceRequest> resource_request,
      ip_protection::IpProtectionProxyConfigDirectFetcher::
          AuthenticateDoneCallback callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  void ClearOAuthTokenProblemBackoff();

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
  // The `Profile` object associated with this
  // `IpProtectionCoreHost()`. Will be reset to nullptr after
  // `Shutdown()` is called.
  // NOTE: If this is used in any `GetForProfile()` call, ensure that there is a
  // corresponding dependency (if needed) registered in the factory class.
  raw_ptr<Profile> profile_;

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

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ip_protection::IpProtectionProxyConfigDirectFetcher>
      ip_protection_proxy_config_fetcher_;

  // The thread pool task runner on which async calls are made to
  // `ip_protection_token_direct_fetcher_` to fetch blind signed tokens.
  // This is needed to move some of the expensive token generation work off the
  // UI thread.
  scoped_refptr<base::SequencedTaskRunner> token_fetcher_task_runner_;

  // An IpProtectionTokenFetcher instance that is bound to the given sequenced
  // `token_fetcher_task_runner_` on which all calls to the
  // `quiche::BlindSignAuth` library will happen on.
  base::SequenceBound<ip_protection::IpProtectionTokenDirectFetcher>
      ip_protection_token_direct_fetcher_;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`. `last_try_get_auth_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver
  // (so, from either the main profile or an associated incognito mode profile).
  ip_protection::TryGetAuthTokensResult last_try_get_auth_tokens_result_ =
      ip_protection::TryGetAuthTokensResult::kSuccess;
  std::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The `mojo::Receiver` objects allowing the network service to call methods
  // on `this`.
  //
  // At any given time there should only be two receivers, one for the main
  // profile and another one if an associated incognito window is opened.
  // If one of the corresponding Network Contexts restarts, the
  // corresponding receiver will automatically be removed and a new one
  // bound as part of the Network Context initialization flow.
  mojo::ReceiverSet<network::mojom::IpProtectionConfigGetter> receivers_;

  // Similar to `receivers_`, but containing remotes for all existing
  // IpProtectionProxyDelegates.
  mojo::RemoteSet<network::mojom::IpProtectionControl> remotes_;

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
