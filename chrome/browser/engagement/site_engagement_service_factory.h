// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/site_engagement/content/site_engagement_service.h"

class Profile;

namespace site_engagement {

// Singleton that owns all SiteEngagementServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SiteEngagementService.
//
// The default factory behavior is suitable for this factory as:
// * the site engagement service should be created lazily
// * the site engagement service is needed in tests.
class SiteEngagementServiceFactory
    : public ProfileKeyedServiceFactory,
      public SiteEngagementService::ServiceProvider {
 public:
  static SiteEngagementService* GetForProfile(
      content::BrowserContext* browser_context);
  static SiteEngagementService* GetForProfileIfExists(Profile* profile);

  static SiteEngagementServiceFactory* GetInstance();

  SiteEngagementServiceFactory(const SiteEngagementServiceFactory&) = delete;
  SiteEngagementServiceFactory& operator=(const SiteEngagementServiceFactory&) =
      delete;

  // SiteEngagementService::ServiceProvider:
  SiteEngagementService* GetSiteEngagementService(
      content::BrowserContext* browser_context) override;

 private:
  friend struct base::DefaultSingletonTraits<SiteEngagementServiceFactory>;

  SiteEngagementServiceFactory();
  ~SiteEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace site_engagement

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_FACTORY_H_
