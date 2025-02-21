// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "url/gurl.h"

namespace enterprise_auth {

extern const char kAllIdentityProviders[];

// Class that provides authentication functionalities from Extensible Enterprise
// SSO. This class does not support origin filtering because call to a platform
// API is required to know if a URL is supported. This class relies on the
// SSO extensions installed on the device.
class ExtensibleEnterpriseSSOProvider : public PlatformAuthProvider {
 public:
  struct Metrics {
    explicit Metrics(const std::string& host);
    ~Metrics();
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    std::string host;
    base::Time start_time;
    base::Time end_time;
    bool url_is_supported = false;
    bool success = false;
  };

  struct DelegateResult {
    DelegateResult(const std::string& name,
                   bool success,
                   net::HttpRequestHeaders headers = net::HttpRequestHeaders());
    ~DelegateResult();
    DelegateResult(const DelegateResult&) = delete;
    DelegateResult& operator=(const DelegateResult&) = delete;

    std::string name;
    bool success;
    net::HttpRequestHeaders headers;
  };

  ExtensibleEnterpriseSSOProvider();
  ~ExtensibleEnterpriseSSOProvider() override;
  ExtensibleEnterpriseSSOProvider(const ExtensibleEnterpriseSSOProvider&) =
      delete;
  ExtensibleEnterpriseSSOProvider& operator=(
      const ExtensibleEnterpriseSSOProvider&) = delete;

  static std::set<std::string> GetSupportedIdentityProviders();
  static base::Value::List GetSupportedIdentityProvidersList();

  // enterprise_auth::PlatformAuthProvider:
  bool SupportsOriginFiltering() override;
  void FetchOrigins(FetchOriginsCallback on_fetch_complete) override;
  void GetData(const GURL& url,
               PlatformAuthProviderManager::GetDataCallback callback) override;

 private:
  base::WeakPtrFactory<ExtensibleEnterpriseSSOProvider> weak_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_
