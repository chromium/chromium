// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash::full_restore {

// Singleton factory that builds and owns FullRestoreService.
class FullRestoreServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static bool IsFullRestoreAvailableForProfile(const Profile* profile);

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
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_FACTORY_H_
