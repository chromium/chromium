// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class BoundSessionRefreshCookieFetcherImpl
    : public BoundSessionRefreshCookieFetcher,
      public network::mojom::CookieAccessObserver {
 public:
  BoundSessionRefreshCookieFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionBindingHelper& session_binding_helper,
      std::string_view session_id,
      const GURL& refresh_url,
      const GURL& cookie_url,
      base::flat_set<std::string> cookie_names,
      bool is_off_the_record_profile,
      bound_session_credentials::RotationDebugInfo debug_info);
  ~BoundSessionRefreshCookieFetcherImpl() override;

  // BoundSessionRefreshCookieFetcher:
  void Start(
      RefreshCookieCompleteCallback callback,
      std::optional<std::string> sec_session_challenge_response) override;
  bool IsChallengeReceived() const override;
  std::optional<std::string> TakeSecSessionChallengeResponseIfAny() override;

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

  struct ChallengeHeaderItems {
    std::string challenge;
    std::string session_id;
  };

  // Returns parameters encoded in the challenge header value.
  static ChallengeHeaderItems ParseChallengeHeader(const std::string& header);

  void StartRefreshRequest(
      std::optional<std::string> sec_session_challenge_response);
  void OnURLLoaderComplete(scoped_refptr<net::HttpResponseHeaders> headers);
  Result GetResultFromNetErrorAndHttpStatusCode(
      net::Error net_error,
      std::optional<int> response_code);
  void ReportRefreshResult();

  // Returns `std::nullopt` if assertion isn't required.
  std::optional<std::string> GetChallengeIfBindingKeyAssertionRequired(
      const scoped_refptr<net::HttpResponseHeaders>& headers) const;
  void HandleBindingKeyAssertionRequired(
      const std::string& challenge_header_value);
  void CompleteRequestAndReportRefreshResult(Result result);
  void RefreshWithChallenge(const std::string& challenge,
                            size_t generate_assertion_attempt = 0);
  void OnGenerateBindingKeyAssertion(
      base::ElapsedTimer generate_assertion_timer,
      const std::string& challenge,
      size_t generate_assertion_attempt,
      base::expected<std::string, SessionBindingHelper::Error>
          assertion_or_error);

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override;
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ref<SessionBindingHelper> session_binding_helper_;

  const std::string session_id_;
  const GURL refresh_url_;

  // Used to check whether the refresh request has set the required cookie.
  // Otherwise, the request is considered a failure.
  const GURL expected_cookie_domain_;
  const base::flat_set<std::string> expected_cookie_names_;

  // Required to attach X-Client-Data header to cookie rotation request for
  // GWS-visible Finch experiment.
  const bool is_off_the_record_profile_;

  RefreshCookieCompleteCallback callback_;

  bool expected_cookies_set_ = false;
  base::OneShotTimer reported_cookies_notified_timer_;
  bool reported_cookies_notified_ = false;

  // Refresh request result.
  Result result_;
  bool cookie_refresh_completed_ = false;
  size_t assertion_requests_count_ = 0;
  std::optional<std::string> sec_session_challenge_response_;
  bound_session_credentials::RotationDebugInfo debug_info_;

  // Non-null after a fetch has started.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;
  std::optional<base::TimeTicks> cookie_refresh_duration_;
  base::WeakPtrFactory<BoundSessionRefreshCookieFetcherImpl> weak_ptr_factory_{
      this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_IMPL_H_
