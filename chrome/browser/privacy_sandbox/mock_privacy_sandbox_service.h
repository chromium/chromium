// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_

#include <memory>

#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

class KeyedService;

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MockPrivacySandboxService();
  ~MockPrivacySandboxService() override;

  MOCK_METHOD(PrivacySandboxService::PromptType,
              GetRequiredPromptType,
              (PrivacySandboxService::SurfaceType),
              (override));
  MOCK_METHOD(void,
              PromptActionOccurred,
              (PrivacySandboxService::PromptAction,
               PrivacySandboxService::SurfaceType),
              (override));
#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void,
              PromptOpenedForBrowser,
              (Browser*, views::Widget*),
              (override));
  MOCK_METHOD(void, PromptClosedForBrowser, (Browser*), (override));
  MOCK_METHOD(bool, IsPromptOpenForBrowser, (Browser*), (override));
#endif  // !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, ForceChromeBuildForTests, (bool), (override));
  // Mock this method to enable opening the settings page in tests.
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (override));
  MOCK_METHOD(bool, IsRestrictedNoticeEnabled, (), (override));
  MOCK_METHOD(void, SetFirstPartySetsDataAccessEnabled, (bool), (override));
  MOCK_METHOD(bool, IsFirstPartySetsDataAccessEnabled, (), (const, override));
  MOCK_METHOD(bool, IsFirstPartySetsDataAccessManaged, (), (const, override));
  MOCK_METHOD((base::flat_map<net::SchemefulSite, net::SchemefulSite>),
              GetSampleFirstPartySets,
              (),
              (const, override));
  MOCK_METHOD(std::optional<net::SchemefulSite>,
              GetFirstPartySetOwner,
              (const GURL& site_url),
              (const, override));
  MOCK_METHOD(std::optional<std::u16string>,
              GetFirstPartySetOwnerForDisplay,
              (const GURL& site_url),
              (const, override));
  MOCK_METHOD(bool,
              IsPartOfManagedFirstPartySet,
              (const net::SchemefulSite& site),
              (const, override));
  MOCK_METHOD(void,
              GetFledgeJoiningEtldPlusOneForDisplay,
              (base::OnceCallback<void(std::vector<std::string>)>),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetBlockedFledgeJoiningTopFramesForDisplay,
              (),
              (const, override));
  MOCK_METHOD(void,
              SetFledgeJoiningAllowed,
              ((const std::string&), bool),
              (const, override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetCurrentTopTopics,
              (),
              (const, override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetFirstLevelTopics,
              (),
              (const, override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetChildTopicsCurrentlyAssigned,
              (const privacy_sandbox::CanonicalTopic& topic),
              (const, override));
  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetBlockedTopics,
              (),
              (const, override));
  MOCK_METHOD(void,
              SetTopicAllowed,
              (privacy_sandbox::CanonicalTopic, bool),
              (override));
  MOCK_METHOD(bool,
              PrivacySandboxPrivacyGuideShouldShowAdTopicsCard,
              (),
              (override));
  MOCK_METHOD(void, TopicsToggleChanged, (bool), (const, override));
  MOCK_METHOD(bool, TopicsConsentRequired, (), (const, override));
  MOCK_METHOD(bool, TopicsHasActiveConsent, (), (const, override));
  MOCK_METHOD(privacy_sandbox::TopicsConsentUpdateSource,
              TopicsConsentLastUpdateSource,
              (),
              (const, override));
  MOCK_METHOD(base::Time, TopicsConsentLastUpdateTime, (), (const, override));
  MOCK_METHOD(std::string, TopicsConsentLastUpdateText, (), (const, override));
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SERVICE_H_
