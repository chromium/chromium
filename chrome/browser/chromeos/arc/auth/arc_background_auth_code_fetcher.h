// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_context.h"
#include "net/url_request/url_fetcher_delegate.h"

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
extern const char kAuthTokenExchangeEndPoint[];

// The instance is not reusable, so for each Fetch(), the instance must be
// re-created. Deleting the instance cancels inflight operation.
class ArcBackgroundAuthCodeFetcher : public ArcAuthCodeFetcher {
 public:
  // |account_id| is the id used by the OAuth Token Service chain.
  ArcBackgroundAuthCodeFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const std::string& account_id,
      bool initial_signin,
      bool is_primary_account);
  ~ArcBackgroundAuthCodeFetcher() override;

  // ArcAuthCodeFetcher:
  void Fetch(FetchCallback callback) override;

  void SkipMergeSessionForTesting();

 private:
  void ResetFetchers();
  void OnPrepared(bool success);

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  void ReportResult(const std::string& auth_code,
                    OptInSilentAuthCode uma_status);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Unowned pointer.
  Profile* const profile_;
  ArcAuthContext context_;
  FetchCallback callback_;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Keeps context of account code request. |initial_signin_| is true if request
  // is made for initial sign-in flow.
  const bool initial_signin_;

  // Is this fetcher being used to fetch auth codes for the Device/Primary
  // Account on Chrome OS.
  const bool is_primary_account_;

  base::WeakPtrFactory<ArcBackgroundAuthCodeFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcBackgroundAuthCodeFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_
