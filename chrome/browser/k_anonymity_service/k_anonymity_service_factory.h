// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {

class KAnonymityServiceDelegate;

}  // namespace content

class Profile;

class KAnonymityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  KAnonymityServiceFactory(const KAnonymityServiceFactory&) = delete;
  KAnonymityServiceFactory& operator=(const KAnonymityServiceFactory&) = delete;

  static KAnonymityServiceFactory* GetInstance();

  static content::KAnonymityServiceDelegate* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<KAnonymityServiceFactory>;

  KAnonymityServiceFactory();

  ~KAnonymityServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_FACTORY_H_
