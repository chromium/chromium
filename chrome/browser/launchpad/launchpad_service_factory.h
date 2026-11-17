// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace launchpad {
class LaunchpadService;
}  // namespace launchpad

class LaunchpadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static launchpad::LaunchpadService* GetForProfile(Profile* profile);
  static LaunchpadServiceFactory* GetInstance();

  LaunchpadServiceFactory(const LaunchpadServiceFactory&) = delete;
  LaunchpadServiceFactory& operator=(const LaunchpadServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LaunchpadServiceFactory>;

  LaunchpadServiceFactory();
  ~LaunchpadServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_FACTORY_H_
