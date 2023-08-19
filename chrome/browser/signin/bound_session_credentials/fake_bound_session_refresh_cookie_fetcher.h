// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_FAKE_BOUND_SESSION_REFRESH_COOKIE_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_FAKE_BOUND_SESSION_REFRESH_COOKIE_FETCHER_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network::mojom {
class CookieManager;
}

class FakeBoundSessionRefreshCookieFetcher
    : public BoundSessionRefreshCookieFetcher {
 public:
  FakeBoundSessionRefreshCookieFetcher(
      network::mojom::CookieManager* cookie_manager,
      const GURL& url,
      base::flat_set<std::string> cookie_names,
      absl::optional<base::TimeDelta> unlock_automatically_in = absl::nullopt);
  ~FakeBoundSessionRefreshCookieFetcher() override;

  // BoundSessionRefreshCookieFetcher:
  void Start(RefreshCookieCompleteCallback callback) override;

  // `cookie_expiration` is set only if `result` is
  // `BoundSessionRefreshCookieFetcher::Result::kSuccess`.
  void SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result result,
      absl::optional<base::Time> cookie_expiration);

 protected:
  std::unique_ptr<net::CanonicalCookie> CreateFakeCookie(
      const std::string& cookie_name,
      base::Time cookie_expiration);
  void OnRefreshCookieCompleted(
      std::vector<std::unique_ptr<net::CanonicalCookie>> cookies);
  void InsertCookieInCookieJar(std::unique_ptr<net::CanonicalCookie> cookie);
  void OnCookieSet(net::CookieAccessResult access_result);
  void ResetCallbackCounter();

  const raw_ptr<network::mojom::CookieManager> cookie_manager_;
  const GURL url_;
  const base::flat_set<std::string> cookie_names_;
  RefreshCookieCompleteCallback callback_;
  size_t callback_counter_ = 0;

  // `this` might be used temporarily for local development until the server
  // endpoint is fully developed and is stable. In production,
  // `unlock_automatically_in_` is set to simulate a fake delay, upon completion
  // the request is completed. If `unlock_automatically_in_` is not set,
  // `SimulateCompleteRefreshRequest()` must be called manually to complete
  // the refresh request.
  absl::optional<base::TimeDelta> unlock_automatically_in_;
  base::WeakPtrFactory<FakeBoundSessionRefreshCookieFetcher> weak_ptr_factory_{
      this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_FAKE_BOUND_SESSION_REFRESH_COOKIE_FETCHER_H_
