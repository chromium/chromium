// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_STORE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_STORE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/performance_manager/persistence/site_data/tab_visibility.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_reader.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_writer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace resource_coordinator {

// Pure virtual interface for a site characteristics data store.
class SiteCharacteristicsDataStore : public KeyedService {
 public:
  SiteCharacteristicsDataStore() = default;
  ~SiteCharacteristicsDataStore() override {}

  // Returns a SiteCharacteristicsDataReader for the given origin.
  virtual std::unique_ptr<SiteCharacteristicsDataReader> GetReaderForOrigin(
      const url::Origin& origin) = 0;

  // Returns a SiteCharacteristicsDataWriter for the given origin.
  //
  // |tab_visibility| indicates the current visibility of the tab. The writer
  // starts in an unloaded state, NotifyTabLoaded() must be called explicitly
  // afterwards if the site is loaded.
  virtual std::unique_ptr<SiteCharacteristicsDataWriter> GetWriterForOrigin(
      const url::Origin& origin,
      performance_manager::TabVisibility tab_visibility) = 0;

  // Indicate if the SiteCharacteristicsDataWriter served by this data store
  // actually persist informations.
  virtual bool IsRecordingForTesting() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteCharacteristicsDataStore);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_STORE_H_
