// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_H_

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_store.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_writer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

class Profile;

namespace resource_coordinator {

class LocalSiteCharacteristicsDatabase;

// Implementation of a SiteCharacteristicsDataStore that use the local site
// characteristics database as a backend.
//
// This class should never be used for off the record profiles, the
// LocalSiteCharacteristicsNonRecordingDataStore class should be used instead.
class LocalSiteCharacteristicsDataStore
    : public SiteCharacteristicsDataStore,
      public LocalSiteCharacteristicsDataStoreInspector,
      public internal::LocalSiteCharacteristicsDataImpl::OnDestroyDelegate,
      public history::HistoryServiceObserver {
 public:
  using LocalSiteCharacteristicsMap =
      base::flat_map<url::Origin, internal::LocalSiteCharacteristicsDataImpl*>;

  explicit LocalSiteCharacteristicsDataStore(Profile* profile);
  ~LocalSiteCharacteristicsDataStore() override;

  // SiteCharacteristicDataStore:
  std::unique_ptr<SiteCharacteristicsDataReader> GetReaderForOrigin(
      const url::Origin& origin) override;
  std::unique_ptr<SiteCharacteristicsDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) override;
  bool IsRecordingForTesting() override;

  const LocalSiteCharacteristicsMap& origin_data_map_for_testing() const {
    return origin_data_map_;
  }

  // NOTE: This should be called before creating any
  // LocalSiteCharacteristicsDataImpl object (this doesn't update the database
  // used by these objects).
  void SetDatabaseForTesting(
      std::unique_ptr<LocalSiteCharacteristicsDatabase> database) {
    database_ = std::move(database);
  }

  // LocalSiteCharacteristicsDataStoreInspector:
  const char* GetDataStoreName() override;
  std::vector<url::Origin> GetAllInMemoryOrigins() override;
  void GetDatabaseSize(DatabaseSizeCallback on_have_data) override;
  bool GetDataForOrigin(const url::Origin& origin,
                        bool* is_dirty,
                        std::unique_ptr<SiteDataProto>* data) override;
  LocalSiteCharacteristicsDataStore* GetDataStore() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataStoreTest, EndToEnd);
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataStoreTest,
                           OnURLsDeleted_Partial_OriginNotReferenced);
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataStoreTest,
                           OnURLsDeleted_Partial_OriginStillReferenced);
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataStoreTest,
                           OnURLsDeleted_Full);

  // Returns a pointer to the LocalSiteCharacteristicsDataImpl object
  // associated with |origin|, create one and add it to |origin_data_map_|
  // if it doesn't exist.
  internal::LocalSiteCharacteristicsDataImpl* GetOrCreateFeatureImpl(
      const url::Origin& origin);

  // internal::LocalSiteCharacteristicsDataImpl::OnDestroyDelegate:
  void OnLocalSiteCharacteristicsDataImplDestroyed(
      internal::LocalSiteCharacteristicsDataImpl* impl) override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Map a serialized origin to a LocalSiteCharacteristicDataInternal
  // pointer.
  LocalSiteCharacteristicsMap origin_data_map_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_{this};

  std::unique_ptr<LocalSiteCharacteristicsDatabase> database_;

  // The profile this data store is associated with.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataStore);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_H_
