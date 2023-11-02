// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace captive_portal {
class CaptivePortalService;
}

// Singleton that owns all captive_portal::CaptivePortalServices and associates
// them with Profiles.  Listens for the Profile's destruction notification and
// cleans up the associated captive_portal::CaptivePortalService.  Incognito
// profiles have their own captive_portal::CaptivePortalService.
class CaptivePortalServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the captive_portal::CaptivePortalService for |profile|.
  static captive_portal::CaptivePortalService* GetForProfile(Profile* profile);

  static CaptivePortalServiceFactory* GetInstance();

  CaptivePortalServiceFactory(const CaptivePortalServiceFactory&) = delete;
  CaptivePortalServiceFactory& operator=(const CaptivePortalServiceFactory&) =
      delete;

 private:
  friend class CaptivePortalBrowserTest;
  friend class CaptivePortalServiceTest;
  friend struct base::DefaultSingletonTraits<CaptivePortalServiceFactory>;

  CaptivePortalServiceFactory();
  ~CaptivePortalServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
