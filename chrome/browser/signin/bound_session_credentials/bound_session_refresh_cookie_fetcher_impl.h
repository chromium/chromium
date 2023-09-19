// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

#include <cstddef>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class WaitForNetworkCallbackHelper;
class SessionBindingHelper;

class BoundSessionRefreshCookieFetcherImpl
    : public BoundSessionRefreshCookieFetcher,
      public network::mojom::CookieAccessObserver {
 public:
  BoundSessionRefreshCookieFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      WaitForNetworkCallbackHelper& wait_for_network_callback_helper,
      SessionBindingHelper& session_binding_helper,
      const GURL& cookie_url,
      base::flat_set<std::string> cookie_names);
  ~BoundSessionRefreshCookieFetcherImpl() override;

  // BoundSessionRefreshCookieFetcher:
  void Start(RefreshCookieCompleteCallback callback) override;

 private:
  friend class BoundSessionRefreshCookieFetcherImplTest;
  FRIEND_TEST_ALL_PREFIXES(BoundSessionRefreshCookieFetcherImplTest,
                           GetResultFromNetErrorAndHttpStatusCode);
  FRIEND_TEST_ALL_PREFIXES(BoundSessionRefreshCookieFetcherImplTest,
                           OnCookiesAccessedRead);
  FRIEND_TEST_ALL_PREFIXES(BoundSessionRefreshCookieFetcherImplTest,
                           OnCookiesAccessedChange);
  FRIEND_TEST_ALL_PREFIXES(
      BoundSessionRefreshCookieFetcherImplParseChallengeHeaderTest,
      ParseChallengeHeader);

  // Returns empty if parsing challenge header failed. Otherwise, returns the
  // decoded challenge field value.
  static std::string ParseChallengeHeader(const std::string& header);

  void StartRefreshRequest(
      absl::optional<std::string> sec_session_challenge_response);
  void OnURLLoaderComplete(scoped_refptr<net::HttpResponseHeaders> headers);
  Result GetResultFromNetErrorAndHttpStatusCode(
      net::Error net_error,
      absl::optional<int> response_code);
  void ReportRefreshResult();

  // Returns `absl::nullopt` if assertion isn't required.
  absl::optional<std::string> GetChallengeIfBindingKeyAssertionRequired(
      const scoped_refptr<net::HttpResponseHeaders>& headers) const;
  void HandleBindingKeyAssertionRequired(
      const std::string& challenge_header_value);
  void CompleteRequestAndReportRefreshResult(Result result);
  void RefreshWithChallenge(const std::string& challenge);
  void OnGenerateBindingKeyAssertion(
      base::ElapsedTimer generate_assertion_timer,
      std::string assertion);

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override;
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ref<WaitForNetworkCallbackHelper> wait_for_network_callback_helper_;
  const raw_ref<SessionBindingHelper> session_binding_helper_;

  // Used to check whether the refresh request has set the required cookie.
  // Otherwise, the request is considered a failure.
  const GURL expected_cookie_domain_;
  const base::flat_set<std::string> expected_cookie_names_;

  RefreshCookieCompleteCallback callback_;

  bool expected_cookies_set_ = false;
  base::OneShotTimer reported_cookies_notified_timer_;
  bool reported_cookies_notified_ = false;

  // Refresh request result.
  Result result_;
  bool cookie_refresh_completed_ = false;
  size_t assertion_requests_count_ = 0;

  // Non-null after a fetch has started.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;
  absl::optional<base::TimeTicks> cookie_refresh_duration_;
  base::WeakPtrFactory<BoundSessionRefreshCookieFetcherImpl> weak_ptr_factory_{
      this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_
