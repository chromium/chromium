// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROVIDER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "url/origin.h"

class GURL;

namespace enterprise_auth {

// An interface to authentication functionality provided by the platform.
class PlatformAuthProvider {
 public:
  PlatformAuthProvider(const PlatformAuthProvider&) = delete;
  PlatformAuthProvider& operator=(const PlatformAuthProvider&) = delete;
  virtual ~PlatformAuthProvider() = default;

  // A callback run with the results of a call to `FetchOrigins()`. If the arg
  // is null, platform-based auth is not supported and no subsequent calls will
  // ever succeed. Otherwise, the arg contains zero or more IdP (identity
  // provider) / STS (security token service) origins to which auth (SSO) should
  // be attempted.
  using FetchOriginsCallback =
      base::OnceCallback<void(std::unique_ptr<std::vector<url::Origin>>)>;

  // Returns true if the provider supports origin filtering. If this returns
  // true, `FetchOrigins` must be called to get the set of origins for which
  // `GetData` may provide authentication data; otherwise, `GetData` must be
  // called on every request.
  virtual bool SupportsOriginFiltering() = 0;

  // Initiates an asynchronous fetch for IdP/STS origins. `on_fetch_complete`
  // will be run on the caller's sequence with the results. Note: destruction of
  // this fetcher instance is not guaranteed to prevent the callback from being
  // run.
  virtual void FetchOrigins(FetchOriginsCallback on_fetch_complete) = 0;

  // Initiates an asynchronous fetch for proof of possession cookies and headers
  // to present to `url`. `callback` will be run on the caller's sequence
  // (possibly synchronously) with the results.
  virtual void GetData(
      const GURL& url,
      PlatformAuthProviderManager::GetDataCallback callback) = 0;

 protected:
  PlatformAuthProvider() = default;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROVIDER_H_
