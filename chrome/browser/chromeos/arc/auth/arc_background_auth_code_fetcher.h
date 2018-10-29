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
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace arc {

// Exposed for testing.
extern const char kAuthTokenExchangeEndPoint[];

// The instance is not reusable, so for each Fetch(), the instance must be
// re-created. Deleting the instance cancels inflight operation.
class ArcBackgroundAuthCodeFetcher : public ArcAuthCodeFetcher,
                                     public OAuth2TokenService::Consumer {
 public:
  // |account_id| is the id used by the OAuth Token Service chain.
  ArcBackgroundAuthCodeFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const std::string& account_id,
      bool initial_signin);
  ~ArcBackgroundAuthCodeFetcher() override;

  // ArcAuthCodeFetcher:
  void Fetch(FetchCallback callback) override;

  void SkipMergeSessionForTesting();

 private:
  void ResetFetchers();
  void OnPrepared(net::URLRequestContextGetter* request_context_getter);

  // OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  void ReportResult(const std::string& auth_code,
                    OptInSilentAuthCode uma_status);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Unowned pointer.
  Profile* const profile_;
  ArcAuthContext context_;
  FetchCallback callback_;

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Keeps context of account code request. |initial_signin_| is true if request
  // is made for initial sign-in flow.
  bool initial_signin_;

  base::WeakPtrFactory<ArcBackgroundAuthCodeFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBackgroundAuthCodeFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_BACKGROUND_AUTH_CODE_FETCHER_H_
