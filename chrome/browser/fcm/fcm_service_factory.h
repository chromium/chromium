// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FCM_FCM_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FCM_FCM_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace fcm {
class FcmService;
}

class Profile;

class FcmServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static fcm::FcmService* GetForProfile(Profile* profile);
  static FcmServiceFactory* GetInstance();

  FcmServiceFactory(const FcmServiceFactory&) = delete;
  FcmServiceFactory& operator=(const FcmServiceFactory&) = delete;

 private:
  friend base::NoDestructor<FcmServiceFactory>;

  FcmServiceFactory();
  ~FcmServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_FCM_FCM_SERVICE_FACTORY_H_
