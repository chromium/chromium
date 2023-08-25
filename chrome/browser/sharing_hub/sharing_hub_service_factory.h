// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace sharing_hub {

class SharingHubService;

class SharingHubServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SharingHubService* GetForProfile(Profile* profile);
  static SharingHubServiceFactory* GetInstance();

  SharingHubServiceFactory(const SharingHubServiceFactory&) = delete;
  SharingHubServiceFactory& operator=(const SharingHubServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SharingHubServiceFactory>;

  SharingHubServiceFactory();
  ~SharingHubServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_FACTORY_H_
