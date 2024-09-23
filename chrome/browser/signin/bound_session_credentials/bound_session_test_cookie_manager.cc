// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"

#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"

BoundSessionTestCookieManager::BoundSessionTestCookieManager() = default;
BoundSessionTestCookieManager::~BoundSessionTestCookieManager() = default;

// static
net::CanonicalCookie BoundSessionTestCookieManager::CreateCookie(
    const GURL& url,
    const std::string& cookie_name,
    std::optional<base::Time> expiry_date) {
  base::Time expiration_time =
      expiry_date.value_or(base::Time::Now() + base::Minutes(10));
  return *net::CanonicalCookie::CreateSanitizedCookie(
      /*url=*/url, cookie_name, "value", /*domain=*/url.host(),
      /*path=*/url.path(),
      /*creation_time=*/base::Time::Now(), expiration_time,
      /*last_access_time=*/base::Time::Now(), /*secure=*/true,
      /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
      net::CookiePriority::COOKIE_PRIORITY_HIGH,
      /*partition_key=*/std::nullopt, /*status=*/nullptr);
}

size_t BoundSessionTestCookieManager::GetNumberOfDeleteCookiesCallbacks() {
  return delete_cookies_callbacks_.size();
}

void BoundSessionTestCookieManager::RunDeleteCookiesCallback() {
  std::vector<DeleteCookiesCallback> callbacks;
  delete_cookies_callbacks_.swap(callbacks);
  for (auto& callback : callbacks) {
    const uint32_t kNumDeletedIgnored = 0;
    std::move(callback).Run(kNumDeletedIgnored);
  }
}

std::vector<network::mojom::CookieDeletionFilterPtr>
BoundSessionTestCookieManager::TakeCookieDeletionFilters() {
  return std::move(cookie_deletion_filters_);
}

void BoundSessionTestCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& source_url,
    const net::CookieOptions& cookie_options,
    SetCanonicalCookieCallback callback) {
  cookies_.insert(cookie);
  DispatchCookieChange(net::CookieChangeInfo(cookie, net::CookieAccessResult(),
                                             net::CookieChangeCause::INSERTED));
  if (callback) {
    std::move(callback).Run(net::CookieAccessResult());
  }
}

void BoundSessionTestCookieManager::GetCookieList(
    const GURL& url,
    const net::CookieOptions& cookie_options,
    const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
    GetCookieListCallback callback) {
  std::vector<net::CookieWithAccessResult> cookie_result;
  for (const auto& cookie : cookies_) {
    cookie_result.emplace_back(cookie, net::CookieAccessResult());
  }
  std::move(callback).Run(cookie_result, {});
}

void BoundSessionTestCookieManager::AddCookieChangeListener(
    const GURL& url,
    const std::optional<std::string>& name,
    mojo::PendingRemote<network::mojom::CookieChangeListener> listener) {
  mojo::Remote<network::mojom::CookieChangeListener> listener_remote(
      std::move(listener));
  // In the context of bound session credentials we always pass a cookie name.
  CHECK(name.has_value());
  cookie_to_listener_.insert({*name, std::move(listener_remote)});
}

void BoundSessionTestCookieManager::DispatchCookieChange(
    const net::CookieChangeInfo& change) {
  if (!cookie_to_listener_.contains(change.cookie.Name())) {
    return;
  }
  cookie_to_listener_[change.cookie.Name()]->OnCookieChange(change);
}

void BoundSessionTestCookieManager::DeleteCookies(
    network::mojom::CookieDeletionFilterPtr filter,
    DeleteCookiesCallback callback) {
  cookie_deletion_filters_.push_back(std::move(filter));
  delete_cookies_callbacks_.push_back(std::move(callback));
}
