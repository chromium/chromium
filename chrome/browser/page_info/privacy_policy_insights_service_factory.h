// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_PRIVACY_POLICY_INSIGHTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_INFO_PRIVACY_POLICY_INSIGHTS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace page_info {
class PrivacyPolicyInsightsService;
}

// Helps construct and find the PrivacyPolicyInsightsService instance for a
// Profile.
class PrivacyPolicyInsightsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static page_info::PrivacyPolicyInsightsService* GetForProfile(
      Profile* profile);
  static PrivacyPolicyInsightsServiceFactory* GetInstance();

  PrivacyPolicyInsightsServiceFactory(
      const PrivacyPolicyInsightsServiceFactory&) = delete;
  PrivacyPolicyInsightsServiceFactory& operator=(
      const PrivacyPolicyInsightsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<PrivacyPolicyInsightsServiceFactory>;

  PrivacyPolicyInsightsServiceFactory();
  ~PrivacyPolicyInsightsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PAGE_INFO_PRIVACY_POLICY_INSIGHTS_SERVICE_FACTORY_H_
