// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class SigninClient;

class BoundSessionRefreshCookieFetcherImpl
    : public BoundSessionRefreshCookieFetcher {
 public:
  explicit BoundSessionRefreshCookieFetcherImpl(SigninClient* client);
  ~BoundSessionRefreshCookieFetcherImpl() override;

  // BoundSessionRefreshCookieFetcher:
  void Start(RefreshCookieCompleteCallback callback) override;

 private:
  void StartRefreshRequest();
  void OnURLLoaderComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  const raw_ptr<SigninClient> client_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  RefreshCookieCompleteCallback callback_;

  // Non-null after a fetch has started.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<BoundSessionRefreshCookieFetcherImpl> weak_ptr_factory_{
      this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_
