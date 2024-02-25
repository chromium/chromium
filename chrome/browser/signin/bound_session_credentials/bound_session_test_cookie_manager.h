// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_TEST_COOKIE_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_TEST_COOKIE_MANAGER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/test/test_cookie_manager.h"

class BoundSessionTestCookieManager : public network::TestCookieManager {
 public:
  BoundSessionTestCookieManager();
  ~BoundSessionTestCookieManager() override;

  static net::CanonicalCookie CreateCookie(
      const GURL& url,
      const std::string& cookie_name,
      std::optional<base::Time> expiry_date = std::nullopt);

  size_t GetNumberOfDeleteCookiesCallbacks();

  void RunDeleteCookiesCallback();

  std::vector<network::mojom::CookieDeletionFilterPtr>
  TakeCookieDeletionFilters();

  const base::flat_set<net::CanonicalCookie>& cookies() { return cookies_; }

  // network::TestCookieManager:
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override;
  void GetCookieList(
      const GURL& url,
      const net::CookieOptions& cookie_options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override;
  void AddCookieChangeListener(
      const GURL& url,
      const std::optional<std::string>& name,
      mojo::PendingRemote<network::mojom::CookieChangeListener> listener)
      override;
  void DispatchCookieChange(const net::CookieChangeInfo& change) override;
  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override;

 private:
  base::flat_set<net::CanonicalCookie> cookies_;
  base::flat_map<std::string,
                 mojo::Remote<network::mojom::CookieChangeListener>>
      cookie_to_listener_;
  std::vector<network::mojom::CookieDeletionFilterPtr> cookie_deletion_filters_;
  std::vector<DeleteCookiesCallback> delete_cookies_callbacks_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_TEST_COOKIE_MANAGER_H_
