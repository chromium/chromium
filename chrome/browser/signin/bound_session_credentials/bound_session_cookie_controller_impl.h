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
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "url/gurl.h"

class SigninClient;
class BoundSessionCookieFetcher;

// This class is responsible for tracking a single bound session cookie:
// - It observers cookie changes
// - Caches cookie expiry date
// - Initiates a cookie refresh at creation time
// - Receives requests to refresh cookie [on demand cookie refresh]
// - Proactively schedule cookie refresh before it expires
// - To execute a the refresh:
//      (1) It requests an async signature from the [future] token binding
//      service.
//      (2) After receiving the signature, it creates a
//      'BoundSessionRefreshCookieFetcher' to do the network refresh request.
// - It is responsible on resuming blocked request for the managed domain on
// cookie updates, persistent refresh errors or timeout.
// - Monitors cookie changes and update the renderers
class BoundSessionCookieControllerImpl : public BoundSessionCookieController {
 public:
  BoundSessionCookieControllerImpl(SigninClient* client,
                                   const GURL& url,
                                   const std::string& cookie_name,
                                   Delegate* delegate);

  void Initialize() override;

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

  void StartRefreshCookieRequest();
  void SetCookieExpirationTimeAndNotify(const base::Time& expiration_time);
  void OnCookieRefreshFetched(absl::optional<const base::Time> expiration_time);

  void set_refresh_cookie_fetcher_factory_for_testing(
      RefreshCookieFetcherFactoryForTesting
          refresh_cookie_fetcher_factory_for_testing) {
    refresh_cookie_fetcher_factory_for_testing_ =
        refresh_cookie_fetcher_factory_for_testing;
  }

  const raw_ptr<SigninClient> client_;
  std::unique_ptr<BoundSessionRefreshCookieFetcher> refresh_cookie_fetcher_;
  RefreshCookieFetcherFactoryForTesting
      refresh_cookie_fetcher_factory_for_testing_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_
