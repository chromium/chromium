// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_HEADER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_HEADER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_header_service.h"

namespace content {
class BrowserContext;
}

namespace storage_access_api::trial {
class StorageAccessHeaderService;

class StorageAccessHeaderServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static StorageAccessHeaderServiceFactory* GetInstance();
  static StorageAccessHeaderService* GetForProfile(Profile* profile);
  static ProfileSelections CreateProfileSelections();

  explicit StorageAccessHeaderServiceFactory(
      base::PassKey<StorageAccessHeaderServiceFactory>);
  StorageAccessHeaderServiceFactory(const StorageAccessHeaderServiceFactory&) =
      delete;
  StorageAccessHeaderServiceFactory& operator=(
      const StorageAccessHeaderServiceFactory&) = delete;
  StorageAccessHeaderServiceFactory(StorageAccessHeaderServiceFactory&&) =
      delete;
  StorageAccessHeaderServiceFactory& operator=(
      StorageAccessHeaderServiceFactory&&) = delete;

 private:
  ~StorageAccessHeaderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace storage_access_api::trial

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_HEADER_SERVICE_FACTORY_H_
