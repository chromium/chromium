// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "url/gurl.h"

namespace enterprise_auth {

// Class that provides authentication functionalities from Extensible Enterprise
// SSO. This class does not support origin filtering because call to a platform
// API is required to know if a URL is supported. This class relies on the
// SSO extensions installed on the device.
class ExtensibleEnterpriseSSOProvider : public PlatformAuthProvider {
 public:
  ExtensibleEnterpriseSSOProvider();
  ~ExtensibleEnterpriseSSOProvider() override;
  ExtensibleEnterpriseSSOProvider(const ExtensibleEnterpriseSSOProvider&) =
      delete;
  ExtensibleEnterpriseSSOProvider& operator=(
      const ExtensibleEnterpriseSSOProvider&) = delete;

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
