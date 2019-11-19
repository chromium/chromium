// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_INSPECTOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_INSPECTOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "base/supports_user_data.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data.pb.h"
#include "url/origin.h"

namespace performance_manager {

class SiteDataCache;

// An interface that allows LocalSite data cachess to expose diagnostic
// information for the associated web UI.
class SiteDataCacheInspector {
 public:
  // Returns the name of the data cache, which should uniquely identify the kind
  // of storage it implements.
  virtual const char* GetDataCacheName() = 0;

  // Retrieves the origins that are current represented by in-memory data
  // at the present time.
  virtual std::vector<url::Origin> GetAllInMemoryOrigins() = 0;

  // Retrieves the number of rows and the on-disk size of the store. Invokes
  // the |on_have_data| callback once the data has been collected, or once it's
  // determined that the data can't be retrieved.
  // On callback |num_rows| is the number of rows in the database, or -1 if
  // the number can't be determined. |on_disk_size_kb| is the on-disk size of
  // the database, or -1 if the on-disk size can't be determined.
  using DataStoreSizeCallback =
      base::OnceCallback<void(base::Optional<int64_t> num_rows,
                              base::Optional<int64_t> on_disk_size_kb)>;
  virtual void GetDataStoreSize(DataStoreSizeCallback on_have_data) = 0;

  // Retrieves the in-memory data for a given origin.
  // On return |data| contains the available data for |origin| if available,
  // and |is_dirty| is true if the entry needs flushing to disk.
  // Returns true if an entry exists for |origin|.
  virtual bool GetDataForOrigin(const url::Origin& origin,
                                bool* is_dirty,
                                std::unique_ptr<SiteDataProto>* data) = 0;

  // Retrieves the data cache this inspector is associated with.
  virtual SiteDataCache* GetDataCache() = 0;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_INSPECTOR_H_
