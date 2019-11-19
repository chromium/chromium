// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_reader.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_writer.h"
#include "chrome/browser/performance_manager/persistence/site_data/tab_visibility.h"
#include "url/origin.h"

namespace performance_manager {

// Pure virtual interface for a site data cache.
class SiteDataCache {
 public:
  SiteDataCache() = default;
  virtual ~SiteDataCache() = default;

  // Returns a SiteDataReader for the given origin.
  virtual std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) = 0;

  // Returns a SiteDataWriter for the given origin.
  //
  // |tab_visibility| indicates the current visibility of the tab. The writer
  // starts in an unloaded state, NotifyTabLoaded() must be called explicitly
  // afterwards if the site is loaded.
  virtual std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) = 0;

  // Indicate if the SiteDataWriter served by this data cache
  // actually persist information.
  virtual bool IsRecordingForTesting() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteDataCache);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_
