// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_INSTALLER_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_INSTALLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniInstaller;

class CrostiniInstallerFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniInstaller* GetForProfile(Profile* profile);

  static CrostiniInstallerFactory* GetInstance();

  CrostiniInstallerFactory(const CrostiniInstallerFactory&) = delete;
  CrostiniInstallerFactory& operator=(const CrostiniInstallerFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniInstallerFactory>;

  CrostiniInstallerFactory();
  ~CrostiniInstallerFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_INSTALLER_FACTORY_H_
