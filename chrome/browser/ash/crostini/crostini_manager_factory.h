// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace crostini {

class CrostiniManager;

class CrostiniManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniManager* GetForProfile(Profile* profile);
  static CrostiniManagerFactory* GetInstance();

  CrostiniManagerFactory(const CrostiniManagerFactory&) = delete;
  CrostiniManagerFactory& operator=(const CrostiniManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniManagerFactory>;

  CrostiniManagerFactory();
  ~CrostiniManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_FACTORY_H_
