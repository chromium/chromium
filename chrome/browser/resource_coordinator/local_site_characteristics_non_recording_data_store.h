// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_NON_RECORDING_DATA_STORE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_NON_RECORDING_DATA_STORE_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_store.h"

class Profile;

namespace resource_coordinator {

// Specialization of a SiteCharacteristicsDataStore whose
// SiteCharacteristicsDataWriters don't persist observations and whose
// SiteCharacteristicsDataReader are obtained from another
// SiteCharacteristicsDataStore.
class LocalSiteCharacteristicsNonRecordingDataStore
    : public SiteCharacteristicsDataStore,
      public LocalSiteCharacteristicsDataStoreInspector {
 public:
  // |profile| is the profile this data store is associated with.
  // |data_store_inspector| is the inspector instance this instance will
  // delegate to, may be null, but this is typically the inspector instance
  // associated with |data_store_for_readers|. |data_store_for_readers| should
  // outlive this object.
  LocalSiteCharacteristicsNonRecordingDataStore(
      Profile* profile,
      LocalSiteCharacteristicsDataStoreInspector* data_store_inspector,
      SiteCharacteristicsDataStore* data_store_for_readers);
  ~LocalSiteCharacteristicsNonRecordingDataStore() override;

  // SiteCharacteristicDataStore:
  std::unique_ptr<SiteCharacteristicsDataReader> GetReaderForOrigin(
      const url::Origin& origin) override;
  std::unique_ptr<SiteCharacteristicsDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) override;
  bool IsRecordingForTesting() override;

  // LocalSiteCharacteristicsDataStoreInspector:
  const char* GetDataStoreName() override;
  std::vector<url::Origin> GetAllInMemoryOrigins() override;
  void GetDatabaseSize(DatabaseSizeCallback on_have_data) override;
  bool GetDataForOrigin(const url::Origin& origin,
                        bool* is_dirty,
                        std::unique_ptr<SiteDataProto>* data) override;
  SiteCharacteristicsDataStore* GetDataStore() override;

 private:
  // The data store to use to create the readers served by this data store. E.g.
  // during an incognito session it should point to the data store used by the
  // parent session.
  SiteCharacteristicsDataStore* data_store_for_readers_;

  // The inspector implementation this instance delegates to.
  LocalSiteCharacteristicsDataStoreInspector* data_store_inspector_;

  // The profile this data store is associated with.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsNonRecordingDataStore);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_NON_RECORDING_DATA_STORE_H_
