// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_FETCHER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SigninClient;

class BoundSessionCookieFetcher {
 public:
  // Returns the expected expiration date of the cookie. This is optional as
  // set cookie might fail.
  using SetCookieCompleteCallback =
      base::OnceCallback<void(absl::optional<const base::Time>)>;

  BoundSessionCookieFetcher(SigninClient* client,
                            SetCookieCompleteCallback callback);
  ~BoundSessionCookieFetcher();

 private:
  void StartSettingCookie();
  void OnCookieSet(const base::Time& expiry_date,
                   net::CookieAccessResult access_result);

  const raw_ptr<SigninClient> client_;
  SetCookieCompleteCallback callback_;
  base::WeakPtrFactory<BoundSessionCookieFetcher> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_FETCHER_H_
