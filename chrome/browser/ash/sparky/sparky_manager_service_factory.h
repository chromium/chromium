// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SPARKY_SPARKY_MANAGER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_SPARKY_SPARKY_MANAGER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/sparky/sparky_manager_impl.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class SparkyManagerImpl;

// Profile keyed service factory for the Sparky Manager service. Sparky Manager
// Service does not exist for OTR profiles.
class SparkyManagerServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of `SparkyManagerService` for the passed `profile`.
  // Returns nullptr when passed an OTR `profile`.
  static SparkyManagerImpl* GetForProfile(Profile* profile);

  // Returns the singleton instance of `SparkyManagerServiceFactory`. Creates
  // one if it doesn't already exist. Clients should directly call
  // `SparkyManagerServiceFactory::GetForProfile` to acquire a pointer to the
  // `SparkyManagerServiceFactory` instance of a given profile.
  static SparkyManagerServiceFactory* GetInstance();

  SparkyManagerServiceFactory(const SparkyManagerServiceFactory&) = delete;
  SparkyManagerServiceFactory& operator=(const SparkyManagerServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<SparkyManagerServiceFactory>;

  SparkyManagerServiceFactory();
  ~SparkyManagerServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SPARKY_SPARKY_MANAGER_SERVICE_FACTORY_H_
