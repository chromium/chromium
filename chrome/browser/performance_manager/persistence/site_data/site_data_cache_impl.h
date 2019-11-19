// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_impl.h"

namespace performance_manager {

// Implementation of a SiteDataCache that serves normal reader and writers.
//
// This class should never be used for off the record profiles, the
// NonRecordingSiteDataCache class should be used instead.
class SiteDataCacheImpl : public SiteDataCache,
                          public SiteDataCacheInspector,
                          public internal::SiteDataImpl::OnDestroyDelegate {
 public:
  using SiteDataMap = base::flat_map<url::Origin, internal::SiteDataImpl*>;

  SiteDataCacheImpl(const std::string& browser_context_id,
                    const base::FilePath& browser_context_path);
  ~SiteDataCacheImpl() override;

  // SiteDataCache:
  std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) override;
  std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) override;
  bool IsRecordingForTesting() override;

  const SiteDataMap& origin_data_map_for_testing() const {
    return origin_data_map_;
  }

  // NOTE: This should be called before creating any SiteDataImpl object (this
  // doesn't update the data store used by these objects).
  void SetDataStoreForTesting(std::unique_ptr<SiteDataStore> data_store) {
    data_store_ = std::move(data_store);
  }

  // SiteDataCacheImplInspector:
  const char* GetDataCacheName() override;
  std::vector<url::Origin> GetAllInMemoryOrigins() override;
  void GetDataStoreSize(DataStoreSizeCallback on_have_data) override;
  bool GetDataForOrigin(const url::Origin& origin,
                        bool* is_dirty,
                        std::unique_ptr<SiteDataProto>* data) override;
  SiteDataCacheImpl* GetDataCache() override;

  // Remove a specific set of entries from the cache and the on-disk store.
  void ClearSiteDataForOrigins(
      const std::vector<url::Origin>& origins_to_remove);

  // Clear the data cache and the on-disk store.
  void ClearAllSiteData();

  // Set a callback that will be called once the data store backing this cache
  // has been fully initialized.
  void SetInitializationCallbackForTesting(base::OnceClosure callback);

 private:
  // Returns a pointer to the SiteDataImpl object associated with |origin|,
  // create one and add it to |origin_data_map_| if it doesn't exist.
  internal::SiteDataImpl* GetOrCreateFeatureImpl(const url::Origin& origin);

  // internal::SiteDataImpl::OnDestroyDelegate:
  void OnSiteDataImplDestroyed(internal::SiteDataImpl* impl) override;

  // Map an origin to a SiteDataImpl pointer.
  SiteDataMap origin_data_map_;

  std::unique_ptr<SiteDataStore> data_store_;

  // The ID of the browser context this data store is associated with.
  const std::string browser_context_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SiteDataCacheImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_IMPL_H_
