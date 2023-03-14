// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"

#include "net/cookies/cookie_access_result.h"

// static
net::CanonicalCookie BoundSessionTestCookieManager::CreateCookie(
    const GURL& url,
    const std::string& cookie_name,
    absl::optional<base::Time> expiry_date) {
  base::Time expiration_time =
      expiry_date.value_or(base::Time::Now() + base::Minutes(10));
  return *net::CanonicalCookie::CreateSanitizedCookie(
      /*url=*/url, cookie_name, "value", url.host(),
      /*path=*/"/",
      /*creation_time=*/base::Time::Now(), expiration_time,
      /*last_access_time=*/base::Time::Now(), /*secure=*/true,
      /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
      net::CookiePriority::COOKIE_PRIORITY_HIGH,
      /*same_party=*/true, /*partition_key=*/absl::nullopt);
}

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

void BoundSessionTestCookieManager::GetCookieList(
    const GURL& url,
    const net::CookieOptions& cookie_options,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
    GetCookieListCallback callback) {
  std::move(callback).Run({{cookie_, net::CookieAccessResult()}}, {});
}
