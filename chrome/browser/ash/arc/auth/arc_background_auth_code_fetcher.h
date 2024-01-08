// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_ASH_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/auth/arc_auth_code_fetcher.h"
#include "chrome/browser/ash/arc/auth/arc_auth_context.h"

class Profile;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace arc {

// Exposed for testing.
extern const char kTokenBootstrapEndPoint[];

// The instance is not reusable, so for each Fetch(), the instance must be
// re-created. Deleting the instance cancels inflight operation.
class ArcBackgroundAuthCodeFetcher : public ArcAuthCodeFetcher {
 public:
  // |account_id| is the id used by the OAuth Token Service chain.
  ArcBackgroundAuthCodeFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const CoreAccountId& account_id,
      bool initial_signin,
      bool is_primary_account);

  ArcBackgroundAuthCodeFetcher(const ArcBackgroundAuthCodeFetcher&) = delete;
  ArcBackgroundAuthCodeFetcher& operator=(const ArcBackgroundAuthCodeFetcher&) =
      delete;

  ~ArcBackgroundAuthCodeFetcher() override;

  // ArcAuthCodeFetcher:
  void Fetch(FetchCallback callback) override;

 private:
  void OnPrepared(bool success);
  void AttemptToRecoverAccessToken(const signin::AccessTokenInfo& token_info);

  void StartFetchingAccessToken();

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  void OnSimpleLoaderComplete(signin::AccessTokenInfo token_info,
                              std::unique_ptr<std::string> response_body);

  void ReportResult(const std::string& auth_code,
                    OptInSilentAuthCode uma_status);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Unowned pointer.
  const raw_ptr<Profile> profile_;
  ArcAuthContext context_;
  FetchCallback callback_;

  // Indicates whether |ArcBackgroundAuthCodeFetcher| tried to recover access
  // token. Only one recovery attempt is applied during the fetcher lifetime.
  bool attempted_to_recover_access_token_ = false;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Keeps context of account code request. |initial_signin_| is true if request
  // is made for initial sign-in flow.
  const bool initial_signin_;

  // Is this fetcher being used to fetch auth codes for the Device/Primary
  // Account on Chrome OS.
  const bool is_primary_account_;

  // Indicates if the request to `kTokenBootstrapEndPoint` which fetches the
  // auth code to be used for Google Play Store sign-in should bypass the proxy.
  // Currently we only set the value to true if the network is configured to use
  // a mandatory PAC script which is broken or not reachable.
  bool bypass_proxy_ = false;

  base::WeakPtrFactory<ArcBackgroundAuthCodeFetcher> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_
