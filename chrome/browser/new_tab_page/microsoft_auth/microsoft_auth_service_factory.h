// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class MicrosoftAuthService;
class Profile;

class MicrosoftAuthServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the MicrosoftAuthService for |profile|.
  static MicrosoftAuthService* GetForProfile(Profile* profile);

  static MicrosoftAuthServiceFactory* GetInstance();

  MicrosoftAuthServiceFactory(const MicrosoftAuthServiceFactory&) = delete;
  MicrosoftAuthServiceFactory& operator=(const MicrosoftAuthServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<MicrosoftAuthServiceFactory>;

  MicrosoftAuthServiceFactory();
  ~MicrosoftAuthServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MICROSOFT_AUTH_MICROSOFT_AUTH_SERVICE_FACTORY_H_
