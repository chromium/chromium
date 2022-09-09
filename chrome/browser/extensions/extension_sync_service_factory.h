// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ExtensionSyncService;

class ExtensionSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionSyncService* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionSyncServiceFactory>;

  ExtensionSyncServiceFactory();
  ~ExtensionSyncServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_FACTORY_H_
