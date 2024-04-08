// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DriveService;
class Profile;

class DriveServiceFactory : ProfileKeyedServiceFactory {
 public:
  static DriveService* GetForProfile(Profile* profile);
  static DriveServiceFactory* GetInstance();
  DriveServiceFactory(const DriveServiceFactory&) = delete;

 private:
  friend base::NoDestructor<DriveServiceFactory>;
  DriveServiceFactory();
  ~DriveServiceFactory() override;

  // Uses BrowserContextKeyedServiceFactory to build a DriveService.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FILE_SUGGESTION_DRIVE_SERVICE_FACTORY_H_
