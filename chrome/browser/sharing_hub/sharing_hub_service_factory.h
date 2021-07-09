// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace sharing_hub {

class SharingHubService;

class SharingHubServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SharingHubService* GetForProfile(Profile* profile);
  static SharingHubServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SharingHubServiceFactory>;

  SharingHubServiceFactory();
  ~SharingHubServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SharingHubServiceFactory);
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_SERVICE_FACTORY_H_
