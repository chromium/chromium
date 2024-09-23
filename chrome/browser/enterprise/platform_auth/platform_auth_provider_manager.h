// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROVIDER_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROVIDER_MANAGER_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/http/http_request_headers.h"
#include "url/origin.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace enterprise_auth {

class PlatformAuthProvider;

// Provides a means by which a browser can enable and use platform-based
// authentication (for example Cloud AP SSO on Windows and Enterprise Extensible
// SSO on MacOS).
class PlatformAuthProviderManager {
 public:
  // Returns the process-wide instance owned by `BrowserMainLoop`.
  static PlatformAuthProviderManager& GetInstance();

  // Note: do not construct a new instance on demand; use the one returned by
  // `GetInstance()`.
  PlatformAuthProviderManager();
  PlatformAuthProviderManager(const PlatformAuthProviderManager&) = delete;
  PlatformAuthProviderManager& operator=(const PlatformAuthProviderManager&) =
      delete;
  ~PlatformAuthProviderManager();

  // Enables or disables platform-based auth asynchronously. `on_complete` will
  // be run (possibly synchronously) once processing is complete. Conversely,
  // `on_complete` will not be run if `SetEnabled()` is called again before a
  // previous call is fully processed.
  void SetEnabled(bool enabled, base::OnceClosure on_complete);

  // Returns true if platform-based authentication is enabled.
  bool IsEnabled() const;

  // Returns true if `url` corresponds to one of the origins `origins_`.
  bool IsEnabledFor(const GURL& url) const;

  using GetDataCallback = base::OnceCallback<void(net::HttpRequestHeaders)>;

  // Initiates an asynchronous fetch for proof of possession cookies and headers
  // to present to `url`. Data will only be fetched if platform-based auth is
  // enabled. `callback` will be run on the caller's sequence (possibly
  // synchronously) with the results.
  void GetData(const GURL& url, GetDataCallback callback) const;

  const base::flat_set<url::Origin>& GetOriginsForTesting() { return origins_; }

 private:
  friend class ScopedSetProviderForTesting;
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerTest, DefaultDisabled);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerTest, DisableNoop);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerTest, NotSupported);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerTest,
                           SupportedWithEmptyOrigins);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerTest, OriginRemoval);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerMetricsTest, Success);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthProviderManagerNoOriginFilteringTest,
                           OriginFilteringNotSupported);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthNavigationThrottleTest, ManagerDisabled);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthNavigationThrottleTest, EmptyOrigins);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthNavigationThrottleTest, EmptyData);
  FRIEND_TEST_ALL_PREFIXES(PlatformAuthNavigationThrottleTest, DataReceived);

  explicit PlatformAuthProviderManager(
      std::unique_ptr<PlatformAuthProvider> provider);

  void StartFetchOrigins();
  void OnOrigins(std::unique_ptr<std::vector<url::Origin>> origins);

  // Returns the previously-configured instance.
  std::unique_ptr<PlatformAuthProvider> SetProviderForTesting(
      std::unique_ptr<PlatformAuthProvider> provider);

  std::unique_ptr<PlatformAuthProvider> provider_;
  bool supports_origin_filtering_;
  bool enabled_ = false;

  base::OnceClosure on_enable_complete_;

  // The collections of IdP/STS origins.
  base::flat_set<url::Origin> origins_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PlatformAuthProviderManager> weak_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROVIDER_MANAGER_H_
