// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class OneGoogleBarService;
class Profile;

class OneGoogleBarServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the OneGoogleBarService for |profile|.
  static OneGoogleBarService* GetForProfile(Profile* profile);

  static OneGoogleBarServiceFactory* GetInstance();

  OneGoogleBarServiceFactory(const OneGoogleBarServiceFactory&) = delete;
  OneGoogleBarServiceFactory& operator=(const OneGoogleBarServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<OneGoogleBarServiceFactory>;

  OneGoogleBarServiceFactory();
  ~OneGoogleBarServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_FACTORY_H_
