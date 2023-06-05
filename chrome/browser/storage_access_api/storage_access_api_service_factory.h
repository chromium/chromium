// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_FACTORY_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class StorageAccessAPIServiceImpl;

// Singleton that owns StorageAccessAPIServiceImpl objects and associates them
// with corresponding Profiles.
//
// Listens for each Profile's destruction notification and cleans up the
// associated StorageAccessAPIServiceImpl.
class StorageAccessAPIServiceFactory : public ProfileKeyedServiceFactory {
 public:
  StorageAccessAPIServiceFactory(const StorageAccessAPIServiceFactory&) =
      delete;
  StorageAccessAPIServiceFactory& operator=(
      const StorageAccessAPIServiceFactory&) = delete;

  static StorageAccessAPIServiceImpl* GetForBrowserContext(
      content::BrowserContext* context);

  static StorageAccessAPIServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<StorageAccessAPIServiceFactory>;

  StorageAccessAPIServiceFactory();
  ~StorageAccessAPIServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_SERVICE_FACTORY_H_
