// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PACKAGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PACKAGE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniPackageService;

class CrostiniPackageServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniPackageService* GetForProfile(Profile* profile);

  static CrostiniPackageServiceFactory* GetInstance();

  CrostiniPackageServiceFactory(const CrostiniPackageServiceFactory&) = delete;
  CrostiniPackageServiceFactory& operator=(
      const CrostiniPackageServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniPackageServiceFactory>;

  CrostiniPackageServiceFactory();

  ~CrostiniPackageServiceFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PACKAGE_SERVICE_FACTORY_H_
