// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_BROWSER_ACTUATOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_BROWSER_ACTUATOR_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace browser_actuator {
class BrowserActuatorService;

// A factory to create a unique BrowserActuatorService.
class BrowserActuatorServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the BrowserActuatorService for the profile.
  // Returns nullptr if `kBrowserActuator` is disabled or if `profile` is
  // not a regular profile (e.g., OffTheRecord/Incognito or Guest sessions).
  static BrowserActuatorService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of BrowserActuatorServiceFactory.
  static BrowserActuatorServiceFactory* GetInstance();

  BrowserActuatorServiceFactory(const BrowserActuatorServiceFactory&) = delete;
  BrowserActuatorServiceFactory& operator=(
      const BrowserActuatorServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BrowserActuatorServiceFactory>;

  BrowserActuatorServiceFactory();
  ~BrowserActuatorServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_BROWSER_ACTUATOR_SERVICE_FACTORY_H_
