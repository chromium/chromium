// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash::app_restore {

class AppRestoreArcTaskHandler;

class AppRestoreArcTaskHandlerFactory : public ProfileKeyedServiceFactory {
 public:
  static AppRestoreArcTaskHandler* GetForProfile(Profile* profile);

  static AppRestoreArcTaskHandlerFactory* GetInstance();

 private:
  friend base::NoDestructor<AppRestoreArcTaskHandlerFactory>;

  AppRestoreArcTaskHandlerFactory();
  AppRestoreArcTaskHandlerFactory(const AppRestoreArcTaskHandlerFactory&) =
      delete;
  AppRestoreArcTaskHandlerFactory& operator=(
      const AppRestoreArcTaskHandlerFactory&) = delete;
  ~AppRestoreArcTaskHandlerFactory() override;

  // BrowserContextKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash::app_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_FACTORY_H_
