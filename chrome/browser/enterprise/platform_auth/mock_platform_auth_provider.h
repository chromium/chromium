// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_MOCK_PLATFORM_AUTH_PROVIDER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_MOCK_PLATFORM_AUTH_PROVIDER_H_

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_auth {

class MockPlatformAuthProvider : public PlatformAuthProvider {
 public:
  MockPlatformAuthProvider();
  ~MockPlatformAuthProvider() override;
  MOCK_METHOD(bool, SupportsOriginFiltering, (), (override));
  MOCK_METHOD(void, FetchOrigins, (FetchOriginsCallback), (override));
  MOCK_METHOD(void,
              GetData,
              (const GURL&, PlatformAuthProviderManager::GetDataCallback),
              (override));
  MOCK_METHOD(void, Die, ());
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_MOCK_PLATFORM_AUTH_PROVIDER_H_
