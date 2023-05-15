// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include <memory>
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "url/gurl.h"

class SigninClient;
class BoundSessionCookieFetcher;
class BoundSessionCookieObserver;

class BoundSessionCookieControllerImpl : public BoundSessionCookieController {
 public:
  BoundSessionCookieControllerImpl(SigninClient* client,
                                   const GURL& url,
                                   const std::string& cookie_name,
                                   Delegate* delegate);

  void Initialize() override;

  void OnRequestBlockedOnCookie(
      base::OnceClosure resume_blocked_request) override;

  ~BoundSessionCookieControllerImpl() override;

  BoundSessionCookieControllerImpl(const BoundSessionCookieControllerImpl&) =
      delete;
  BoundSessionCookieControllerImpl& operator=(
      const BoundSessionCookieControllerImpl&) = delete;

 private:
  friend class BoundSessionCookieControllerImplTest;

  // Used by tests to provide their own implementation of the
  // `BoundSessionRefreshCookieFetcher`.
  using RefreshCookieFetcherFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionRefreshCookieFetcher>(
          SigninClient* client,
          const GURL& url,
          const std::string& cookie_name)>;

  std::unique_ptr<BoundSessionRefreshCookieFetcher> CreateRefreshCookieFetcher()
      const;

  bool IsCookieFresh();
  void MaybeRefreshCookie();
  void SetCookieExpirationTimeAndNotify(base::Time expiration_time);
  void OnCookieRefreshFetched(BoundSessionRefreshCookieFetcher::Result result);
  void ResumeBlockedRequests();

  void set_refresh_cookie_fetcher_factory_for_testing(
      RefreshCookieFetcherFactoryForTesting
          refresh_cookie_fetcher_factory_for_testing) {
    refresh_cookie_fetcher_factory_for_testing_ =
        refresh_cookie_fetcher_factory_for_testing;
  }

  const raw_ptr<SigninClient> client_;
  std::unique_ptr<BoundSessionCookieObserver> cookie_observer_;
  std::unique_ptr<BoundSessionRefreshCookieFetcher> refresh_cookie_fetcher_;
  std::vector<base::OnceClosure> resume_blocked_requests_;

  RefreshCookieFetcherFactoryForTesting
      refresh_cookie_fetcher_factory_for_testing_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_
