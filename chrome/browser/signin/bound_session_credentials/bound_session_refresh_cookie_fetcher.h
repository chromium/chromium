// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

#include "net/cookies/canonical_cookie.h"

class SigninClient;

class BoundSessionRefreshCookieFetcher {
 public:
  // Returns the expected expiration date of the cookie. This is optional as
  // set cookie might fail.
  using RefreshCookieCompleteCallback =
      base::OnceCallback<void(absl::optional<const base::Time>)>;

  BoundSessionRefreshCookieFetcher(SigninClient* client,
                                   const GURL& url,
                                   const std::string& cookie_name);
  virtual ~BoundSessionRefreshCookieFetcher();

  BoundSessionRefreshCookieFetcher(const BoundSessionRefreshCookieFetcher&) =
      delete;
  BoundSessionRefreshCookieFetcher& operator=(
      const BoundSessionRefreshCookieFetcher&) = delete;

  virtual void Start(RefreshCookieCompleteCallback callback);

 protected:
  std::unique_ptr<net::CanonicalCookie> CreateFakeCookie(
      const base::Time& cookie_expiration);
  void OnRefreshCookieCompleted(std::unique_ptr<net::CanonicalCookie> cookie);
  void InsertCookieInCookieJar(std::unique_ptr<net::CanonicalCookie> cookie);
  void OnCookieSet(const base::Time& expiry_date,
                   net::CookieAccessResult access_result);

  const raw_ptr<SigninClient> client_;
  const GURL url_;
  const std::string cookie_name_;
  RefreshCookieCompleteCallback callback_;
  base::WeakPtrFactory<BoundSessionRefreshCookieFetcher> weak_ptr_factory_{
      this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REFRESH_COOKIE_FETCHER_H_
