// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/ip_protection_config_http.h"
#include "chrome/browser/ip_protection/ip_protection_config_provider_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;

namespace quiche {
class BlindSignAuthInterface;
class BlindSignAuth;
struct BlindSignToken;
}  // namespace quiche

// The result of a fetch of tokens from the IP Protection auth token server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IpProtectionTryGetAuthTokensResult {
  // The request was successful and resulted in new tokens.
  kSuccess = 0,
  // No primary account is set.
  kFailedNoAccount = 1,
  // Chrome determined the primary account is not eligible.
  kFailedNotEligible = 2,
  // There was a failure fetching an OAuth token for the primary account.
  // Deprecated in favor of `kFailedOAuthToken{Transient,Persistent}`.
  kFailedOAuthTokenDeprecated = 3,
  // There was a failure in BSA with the given status code.
  kFailedBSA400 = 4,
  kFailedBSA401 = 5,
  kFailedBSA403 = 6,

  // Any other issue calling BSA.
  kFailedBSAOther = 7,

  // There was a transient failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenTransient = 8,
  // There was a persistent failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenPersistent = 9,

  kMaxValue = kFailedOAuthTokenPersistent,
};

// Fetches IP protection tokens on demand for the network service.
//
// This class handles both requesting OAuth2 tokens for the signed-in user, and
// fetching blind-signed auth tokens for that user. It may only be used on the
// UI thread.
class IpProtectionConfigProvider
    : public KeyedService,
      public network::mojom::IpProtectionConfigGetter,
      public signin::IdentityManager::Observer {
 public:
  IpProtectionConfigProvider(
      signin::IdentityManager* identity_manager,
      Profile* profile);

  ~IpProtectionConfigProvider() override;

  // Get a batch of blind-signed auth tokens.
  //
  // It is forbidden for two calls to this method to be outstanding at the same
  // time.
  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;

  // Get the list of IP Protection proxies.
  void GetProxyList(GetProxyListCallback callback) override;

  // KeyedService:
  void Shutdown() override;

  static IpProtectionConfigProvider* Get(Profile* profile);

  void AddReceiver(
      mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
          pending_receiver);

  mojo::ReceiverSet<network::mojom::IpProtectionConfigGetter>&
  receivers_for_testing() {
    return receivers_;
  }
  mojo::ReceiverId receiver_id_for_testing() {
    return receiver_id_for_testing_;
  }

  // Like `SetUp()`, but providing values for each of the member variables.
  void SetUpForTesting(
      std::unique_ptr<IpProtectionConfigHttp> ip_protection_config_http_,
      quiche::BlindSignAuthInterface* bsa);

  // Base time deltas for calculating `try_again_after`.
  static constexpr base::TimeDelta kNotEligibleBackoff = base::Days(1);
  static constexpr base::TimeDelta kTransientBackoff = base::Seconds(5);
  static constexpr base::TimeDelta kBugBackoff = base::Minutes(10);

 private:
  friend class IpProtectionConfigProviderTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionConfigProviderTest, CalculateBackoff);
  FRIEND_TEST_ALL_PREFIXES(IpProtectionConfigProviderBrowserTest,
                           BackoffTimeResetAfterProfileAvailabilityChange);

  // Set up `ip_protection_config_http_` and `bsa_`, if not already initialized.
  // This accomplishes lazy loading of these components to break dependency
  // loops in browser startup.
  void SetUp();

  // Calls the IdentityManager asynchronously to request the OAuth token for the
  // logged in user.
  void RequestOAuthToken(uint32_t batch_size,
                         TryGetAuthTokensCallback callback);
  void OnRequestOAuthTokenCompleted(
      std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
          oauth_token_fetcher,
      base::TimeTicks oauth_token_fetch_start_time,
      uint32_t batch_size,
      TryGetAuthTokensCallback callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // `FetchBlindSignedToken()` calls into the `quiche::BlindSignAuth` library to
  // request a blind-signed auth token for use at the IP Protection proxies.
  void FetchBlindSignedToken(signin::AccessTokenInfo access_token_info,
                             uint32_t batch_size,
                             TryGetAuthTokensCallback callback);
  void OnFetchBlindSignedTokenCompleted(
      base::TimeTicks bsa_get_tokens_start_time,
      TryGetAuthTokensCallback callback,
      absl::StatusOr<absl::Span<quiche::BlindSignToken>>);
  static network::mojom::BlindSignedAuthTokenPtr CreateBlindSignedAuthToken(
      quiche::BlindSignToken bsa_token);

  void ClearOAuthTokenProblemBackoff();

  // The object used to get an OAuth token. `identity_manager_` will be set to
  // nullptr after `Shutdown()` is called, but will otherwise be non-null.
  raw_ptr<signin::IdentityManager> identity_manager_;
  // The `Profile` object associated with this
  // `IpProtectionConfigProvider()`. Will be reset to nullptr after
  // `Shutdown()` is called.
  // NOTE: If this is used in any `GetForProfile()` call, ensure that there is a
  // corresponding dependency (if needed) registered in the factory class.
  raw_ptr<Profile> profile_;

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          bsa_tokens,
      TryGetAuthTokensCallback callback,
      IpProtectionTryGetAuthTokensResult result);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  absl::optional<base::TimeDelta> CalculateBackoff(
      IpProtectionTryGetAuthTokensResult result);

  // Instruct the `IpProtectionConfigCache()`(s) in the Network Service to
  // ignore any previously sent `try_again_after` times.
  void InvalidateNetworkContextTryAgainAfterTime();

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

  // The BlindSignAuth implementation used to fetch blind-signed auth tokens. A
  // raw pointer to `url_loader_factory_` gets passed to
  // `ip_protection_config_http_`, so we ensure it stays alive by storing its
  // scoped_refptr here.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<IpProtectionConfigHttp> ip_protection_config_http_;
  std::unique_ptr<quiche::BlindSignAuth> blind_sign_auth_;

  // For testing, BlindSignAuth is accessed via its interface. In production,
  // this is the same pointer as `blind_sign_auth_`.
  raw_ptr<quiche::BlindSignAuthInterface> bsa_ = nullptr;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`. `last_try_get_auth_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver
  // (so, from either the main profile or an associated incognito mode profile).
  IpProtectionTryGetAuthTokensResult last_try_get_auth_tokens_result_ =
      IpProtectionTryGetAuthTokensResult::kSuccess;
  absl::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The `mojo::Receiver` objects corresponding to the `mojo::PendingRemote`
  // objects that get passed to the per-profile NetworkContexts in the network
  // service for requesting blind-signed auth tokens. At any given time there
  // should only be two receivers, one for the main profile and another one if
  // an associated incognito window is opened. If one of the corresponding
  // Network Contexts restarts, the corresponding receiver will automatically be
  // removed and a new one bound as part of the Network Context initialization
  // flow.
  mojo::ReceiverSet<network::mojom::IpProtectionConfigGetter> receivers_;
  // The `mojo::ReceiverId` of the most recently added `mojo::Receiver`, for
  // testing.
  mojo::ReceiverId receiver_id_for_testing_;

  // This must be the last member in this class.
  base::WeakPtrFactory<IpProtectionConfigProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_H_
