// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DATA_MIGRATION_DATA_MIGRATION_FACTORY_H_
#define CHROME_BROWSER_ASH_DATA_MIGRATION_DATA_MIGRATION_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace data_migration {

class DataMigrationFactory : public ProfileKeyedServiceFactory {
 public:
  static DataMigrationFactory* GetInstance();

  DataMigrationFactory(const DataMigrationFactory&) = delete;
  DataMigrationFactory& operator=(const DataMigrationFactory&) = delete;
  ~DataMigrationFactory() override;

 private:
  friend base::NoDestructor<DataMigrationFactory>;

  DataMigrationFactory();

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace data_migration

#endif  // CHROME_BROWSER_ASH_DATA_MIGRATION_DATA_MIGRATION_FACTORY_H_
