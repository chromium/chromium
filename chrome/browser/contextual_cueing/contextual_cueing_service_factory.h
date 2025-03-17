// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class KeyedService;
class Profile;

namespace contextual_cueing {

class ContextualCueingService;

// Singleton that owns all `ContextualCueingService` instances, each mapped to
// one profile. Listens for profile destructions and clean up the associated
// ContextualCueingServices.
class ContextualCueingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the `ContextualCueingService` instance for `profile`. Create it if
  // there is no instance.
  static ContextualCueingService* GetForProfile(Profile* profile);

  // Gets the singleton instance of this factory class.
  static ContextualCueingServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ContextualCueingServiceFactory>;

  ContextualCueingServiceFactory();
  ~ContextualCueingServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_FACTORY_H_
