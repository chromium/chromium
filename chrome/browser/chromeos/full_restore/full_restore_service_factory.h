// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
namespace full_restore {

// Singleton factory that builds and owns FullRestoreService.
class FullRestoreServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static FullRestoreServiceFactory* GetInstance();

  static FullRestoreService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<FullRestoreServiceFactory>;

  FullRestoreServiceFactory();
  ~FullRestoreServiceFactory() override;

  FullRestoreServiceFactory(const FullRestoreServiceFactory&) = delete;
  FullRestoreServiceFactory& operator=(const FullRestoreServiceFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_FACTORY_H_
