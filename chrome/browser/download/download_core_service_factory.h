// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DownloadCoreService;

// Singleton that owns all DownloadCoreServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated DownloadCoreService.
class DownloadCoreServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the DownloadCoreService for |context|, creating if not yet created.
  static DownloadCoreService* GetForBrowserContext(
      content::BrowserContext* context);

  static DownloadCoreServiceFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

 private:
  friend base::NoDestructor<DownloadCoreServiceFactory>;

  DownloadCoreServiceFactory();
  ~DownloadCoreServiceFactory() override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_FACTORY_H_
