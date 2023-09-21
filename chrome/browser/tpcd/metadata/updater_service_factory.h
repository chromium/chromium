// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_METADATA_UPDATER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TPCD_METADATA_UPDATER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace tpcd::metadata {

class UpdaterService;

class UpdaterServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static UpdaterServiceFactory* GetInstance();
  static UpdaterService* GetForProfile(Profile* profile);
  static ProfileSelections CreateProfileSelections();

  UpdaterServiceFactory(const UpdaterServiceFactory&) = delete;
  UpdaterServiceFactory& operator=(const UpdaterServiceFactory&) = delete;
  UpdaterServiceFactory(UpdaterServiceFactory&&) = delete;
  UpdaterServiceFactory& operator=(UpdaterServiceFactory&&) = delete;

 private:
  friend class base::NoDestructor<UpdaterServiceFactory>;

  UpdaterServiceFactory();
  ~UpdaterServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace tpcd::metadata

#endif  // CHROME_BROWSER_TPCD_METADATA_UPDATER_SERVICE_FACTORY_H_
