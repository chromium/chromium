// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UPGRADER_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UPGRADER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniUpgrader;

class CrostiniUpgraderFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniUpgrader* GetForProfile(Profile* profile);

  static CrostiniUpgraderFactory* GetInstance();

  CrostiniUpgraderFactory(const CrostiniUpgraderFactory&) = delete;
  CrostiniUpgraderFactory& operator=(const CrostiniUpgraderFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniUpgraderFactory>;

  CrostiniUpgraderFactory();
  ~CrostiniUpgraderFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UPGRADER_FACTORY_H_
