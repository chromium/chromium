// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_DATA_TYPE_STORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_DATA_TYPE_STORE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace syncer {
class DataTypeStoreService;
}  // namespace syncer

class DataTypeStoreServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static syncer::DataTypeStoreService* GetForProfile(Profile* profile);
  static DataTypeStoreServiceFactory* GetInstance();

  DataTypeStoreServiceFactory(const DataTypeStoreServiceFactory&) = delete;
  DataTypeStoreServiceFactory& operator=(const DataTypeStoreServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<DataTypeStoreServiceFactory>;

  DataTypeStoreServiceFactory();
  ~DataTypeStoreServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_DATA_TYPE_STORE_SERVICE_FACTORY_H_
