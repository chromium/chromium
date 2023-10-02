// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace page_info {
class AboutThisSiteService;
}

// This factory helps construct and find the AboutThisSiteService instance for a
// Profile.
class AboutThisSiteServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static page_info::AboutThisSiteService* GetForProfile(Profile* profile);
  static AboutThisSiteServiceFactory* GetInstance();

  AboutThisSiteServiceFactory(const AboutThisSiteServiceFactory&) = delete;
  AboutThisSiteServiceFactory& operator=(const AboutThisSiteServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AboutThisSiteServiceFactory>;

  AboutThisSiteServiceFactory();
  ~AboutThisSiteServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
