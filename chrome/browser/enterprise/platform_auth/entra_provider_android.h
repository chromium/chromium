// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"

namespace enterprise_auth {

class EntraProviderAndroid : public enterprise_auth::PlatformAuthProvider {
 public:
  // enterprise_auth::PlatformAuthProvider:
  bool SupportsOriginFiltering() override;
  void FetchOrigins(FetchOriginsCallback on_fetch_complete) override;
  void GetData(const GURL& url,
               PlatformAuthProviderManager::GetDataCallback callback) override;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_
