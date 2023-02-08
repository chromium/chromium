// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_TEST_COOKIE_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_TEST_COOKIE_MANAGER_H_

#include "services/network/test/test_cookie_manager.h"

#include "net/cookies/canonical_cookie.h"

class BoundSessionTestCookieManager : public network::TestCookieManager {
 public:
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override;

  net::CanonicalCookie& cookie() { return cookie_; }

 private:
  net::CanonicalCookie cookie_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_TEST_COOKIE_MANAGER_H_
