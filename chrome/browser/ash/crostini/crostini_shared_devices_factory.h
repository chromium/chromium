// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SHARED_DEVICES_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SHARED_DEVICES_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniSharedDevices;

class CrostiniSharedDevicesFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniSharedDevices* GetForProfile(Profile* profile);

  static CrostiniSharedDevicesFactory* GetInstance();

  CrostiniSharedDevicesFactory(const CrostiniSharedDevicesFactory&) = delete;
  CrostiniSharedDevicesFactory& operator=(const CrostiniSharedDevicesFactory&) =
      delete;

 private:
  friend class base::NoDestructor<CrostiniSharedDevicesFactory>;

  CrostiniSharedDevicesFactory();
  ~CrostiniSharedDevicesFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SHARED_DEVICES_FACTORY_H_
