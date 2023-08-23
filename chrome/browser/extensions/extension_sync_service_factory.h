// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ExtensionSyncService;

class ExtensionSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionSyncService* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionSyncServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ExtensionSyncServiceFactory>;

  ExtensionSyncServiceFactory();
  ~ExtensionSyncServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_
