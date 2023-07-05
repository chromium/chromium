// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ip_protection/ip_protection_auth_token_getter_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;

namespace quiche {
class BlindSignAuth;
}  // namespace quiche

// Fetches IP protection tokens on demand for the network service.
//
// This class handles both requesting OAuth2 tokens for the signed-in user, and
// fetching blind-signed auth tokens for that user. It may only be used on the
// UI thread.
class IpProtectionAuthTokenGetter
    : public KeyedService,
      public network::mojom::IpProtectionAuthTokenGetter {
 public:
  using TryGetAuthTokensCallback = base::OnceCallback<void(
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          bsa_tokens)>;

  explicit IpProtectionAuthTokenGetter(
      signin::IdentityManager* identity_manager);

  ~IpProtectionAuthTokenGetter() override;

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

  static IpProtectionAuthTokenGetter* Get(Profile* profile);

 private:
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

  // The BlindSignAuth implementation used to fetch blind-signed auth tokens.
  std::unique_ptr<quiche::BlindSignAuth> blind_sign_auth_;

  // For testing, BlindSignAuth is accessed via its interface. In production,
  // this is the same pointer as `blind_sign_auth_`.
  raw_ptr<quiche::BlindSignAuthInterface> bsa_ = nullptr;

  // Used by `RequestOAuthToken()`.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The batch size of the current request.
  uint32_t batch_size_ = 0;

  // The callback for the executing `TryGetAuthTokens()` call.
  TryGetAuthTokensCallback try_get_auth_token_callback_;
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_H_
