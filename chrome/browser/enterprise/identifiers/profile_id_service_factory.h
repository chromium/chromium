// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace enterprise {

class ProfileIdService;

class ProfileIdServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ProfileIdServiceFactory();
  ~ProfileIdServiceFactory() override;

  static ProfileIdService* GetForProfile(Profile* profile);
  static ProfileIdServiceFactory* GetInstance();

 private:
  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise

#endif  // CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_H_
