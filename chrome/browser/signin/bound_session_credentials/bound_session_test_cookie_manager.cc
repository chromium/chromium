// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"

#include "net/cookies/cookie_access_result.h"

void BoundSessionTestCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& source_url,
    const net::CookieOptions& cookie_options,
    SetCanonicalCookieCallback callback) {
  cookie_ = cookie;
  if (callback) {
    std::move(callback).Run(net::CookieAccessResult());
  }
}
