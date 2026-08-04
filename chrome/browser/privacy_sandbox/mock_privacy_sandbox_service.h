// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

class KeyedService;

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MockPrivacySandboxService();
  ~MockPrivacySandboxService() override;

  MOCK_METHOD(void, ForceChromeBuildForTests, (bool), (override));
  // Mock this method to enable opening the settings page in tests.
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (override));
  MOCK_METHOD(bool, IsRestrictedNoticeEnabled, (), (override));
  MOCK_METHOD(void, SetRelatedWebsiteSetsDataAccessEnabled, (bool), (override));
  MOCK_METHOD(bool,
              IsRelatedWebsiteSetsDataAccessEnabled,
              (),
              (const, override));
  MOCK_METHOD(bool,
              IsRelatedWebsiteSetsDataAccessManaged,
              (),
              (const, override));
  MOCK_METHOD(std::optional<net::SchemefulSite>,
              GetRelatedWebsiteSetOwner,
              (const GURL& site_url),
              (const, override));
  MOCK_METHOD(std::optional<std::u16string>,
              GetRelatedWebsiteSetOwnerForDisplay,
              (const GURL& site_url),
              (const, override));
  MOCK_METHOD(bool,
              IsPartOfManagedRelatedWebsiteSet,
              (const net::SchemefulSite& site),
              (const, override));
  MOCK_METHOD(bool, ShouldUsePrivacyPolicyChinaDomain, (), (override));
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_
