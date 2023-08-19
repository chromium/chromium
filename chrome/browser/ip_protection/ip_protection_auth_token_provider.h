// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_PROVIDER_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_PROVIDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/blind_sign_http_impl.h"
#include "chrome/browser/ip_protection/ip_protection_auth_token_provider_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
  kFailedOAuthToken = 3,
  // There was a failure in BSA with the given status code.
  kFailedBSA400 = 4,
  kFailedBSA401 = 5,
  kFailedBSA403 = 6,

  // Any other issue calling BSA.
  kFailedBSAOther = 7,

  kMaxValue = kFailedBSAOther,
};

// Fetches IP protection tokens on demand for the network service.
//
// This class handles both requesting OAuth2 tokens for the signed-in user, and
// fetching blind-signed auth tokens for that user. It may only be used on the
// UI thread.
class IpProtectionAuthTokenProvider
    : public KeyedService,
      public network::mojom::IpProtectionAuthTokenGetter {
 public:
  IpProtectionAuthTokenProvider(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~IpProtectionAuthTokenProvider() override;

  void SetBlindSignAuthInterfaceForTesting(
      quiche::BlindSignAuthInterface* bsa) {
    bsa_ = bsa;
  }

  // Get a batch of blind-signed auth tokens.
  //
  // It is forbidden for two calls to this method to be outstanding at the same
  // time.
  void TryGetAuthTokens(uint32_t batch_size,
                        TryGetAuthTokensCallback callback) override;

  // KeyedService:
  void Shutdown() override;

  static IpProtectionAuthTokenProvider* Get(Profile* profile);

  void SetReceiver(
      mojo::PendingReceiver<network::mojom::IpProtectionAuthTokenGetter>
          pending_receiver);

  mojo::Receiver<network::mojom::IpProtectionAuthTokenGetter>&
  receiver_for_testing() {
    return receiver_;
  }

  // Base time deltas for calculating `try_again_after`.
  static constexpr base::TimeDelta kNoAccountBackoff = base::Minutes(5);
  static constexpr base::TimeDelta kNotEligibleBackoff = base::Days(1);
  static constexpr base::TimeDelta kTransientBackoff = base::Seconds(5);
  static constexpr base::TimeDelta kBugBackoff = base::Minutes(10);

 private:
  friend class IpProtectionAuthTokenProviderTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionAuthTokenProviderTest, CalculateBackoff);

  // Calls the IdentityManager asynchronously to request the OAuth token for the
  // logged in user.
  void RequestOAuthToken();
  void OnRequestOAuthTokenCompleted(GoogleServiceAuthError error,
                                    signin::AccessTokenInfo access_token_info);

  // `FetchBlindSignedToken()` calls into the `quiche::BlindSignAuth` library to
  // request a blind-signed auth token for use at the IP Protection proxies.
  void FetchBlindSignedToken(signin::AccessTokenInfo access_token_info);
  void OnFetchBlindSignedTokenCompleted(
      absl::StatusOr<absl::Span<quiche::BlindSignToken>>);

  // The object used to get an OAuth token. `identity_manager_` will be set to
  // nullptr after `Shutdown()` is called.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          bsa_tokens,
      IpProtectionTryGetAuthTokensResult result);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  absl::optional<base::TimeDelta> CalculateBackoff(
      IpProtectionTryGetAuthTokensResult result);

  // The BlindSignAuth implementation used to fetch blind-signed auth tokens. A
  // raw pointer to `url_loader_factory_` gets passed to
  // `blind_sign_http_impl_`, so we ensure it stays alive by storing its
  // scoped_refptr here.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<BlindSignHttpImpl> blind_sign_http_impl_;
  std::unique_ptr<quiche::BlindSignAuth> blind_sign_auth_;

  // For testing, BlindSignAuth is accessed via its interface. In production,
  // this is the same pointer as `blind_sign_auth_`.
  raw_ptr<quiche::BlindSignAuthInterface> bsa_ = nullptr;

  // Used by `RequestOAuthToken()`.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The batch size of the current request.
  uint32_t batch_size_ = 0;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`.
  IpProtectionTryGetAuthTokensResult last_try_get_auth_tokens_result_ =
      IpProtectionTryGetAuthTokensResult::kSuccess;
  absl::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The callback for the executing `TryGetAuthTokens()` call.
  TryGetAuthTokensCallback try_get_auth_tokens_callback_;

  // Time that the current operation began, for measurement.
  base::TimeTicks start_time_;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The `mojo::Receiver` object corresponding to the `mojo::PendingRemote` that
  // gets passed to the per-profile NetworkContexts in the network service for
  // requesting blind-signed auth tokens.
  mojo::Receiver<network::mojom::IpProtectionAuthTokenGetter> receiver_{this};
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_PROVIDER_H_
