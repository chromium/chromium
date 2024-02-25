// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANTA_MANTA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MANTA_MANTA_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace manta {

class MantaService;

// Profile keyed service factory for the Manta service. Manta Service does not
// exist for OTR profiles.
class MantaServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of `MantaService` for the passed `profile`. Returns
  // nullptr when passed an OTR `profile`.
  static MantaService* GetForProfile(Profile* profile);

  // Returns the singleton instance of `MantaServiceFactory`. Creates one if it
  // doesn't already exist. Clients should directly call
  // `MantaServiceFactory::GetForProfile` to acquire a pointer to the
  // `MantaServiceFactory` instance of a given profile.
  static MantaServiceFactory* GetInstance();

  MantaServiceFactory(const MantaServiceFactory&) = delete;
  MantaServiceFactory& operator=(const MantaServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MantaServiceFactory>;

  MantaServiceFactory();
  ~MantaServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace manta

#endif  // CHROME_BROWSER_MANTA_MANTA_SERVICE_FACTORY_H_
