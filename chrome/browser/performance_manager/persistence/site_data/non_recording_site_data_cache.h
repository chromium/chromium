// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NON_RECORDING_SITE_DATA_CACHE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NON_RECORDING_SITE_DATA_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_inspector.h"

namespace performance_manager {

// Implementation of a SiteDataCache that ensures that no data gets persisted.
//
// This class should be used for off the record profiles.
class NonRecordingSiteDataCache : public SiteDataCache,
                                  public SiteDataCacheInspector {
 public:
  NonRecordingSiteDataCache(const std::string& browser_context_id,
                            SiteDataCacheInspector* data_cache_inspector,
                            SiteDataCache* data_cache_for_readers);
  ~NonRecordingSiteDataCache() override;

  // SiteDataCache:
  std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) override;
  std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) override;
  bool IsRecordingForTesting() override;

  // SiteDataCacheInspector:
  const char* GetDataCacheName() override;
  std::vector<url::Origin> GetAllInMemoryOrigins() override;
  void GetDataStoreSize(DataStoreSizeCallback on_have_data) override;
  bool GetDataForOrigin(const url::Origin& origin,
                        bool* is_dirty,
                        std::unique_ptr<SiteDataProto>* data) override;
  NonRecordingSiteDataCache* GetDataCache() override;

 private:
  // The data cache to use to create the readers served by this data store. E.g.
  // during an incognito session it should point to the data cache used by the
  // parent session.
  SiteDataCache* data_cache_for_readers_;

  // The inspector implementation this instance delegates to.
  SiteDataCacheInspector* data_cache_inspector_;

  // The ID of the browser context this data store is associated with.
  const std::string browser_context_id_;

  DISALLOW_COPY_AND_ASSIGN(NonRecordingSiteDataCache);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NON_RECORDING_SITE_DATA_CACHE_H_
