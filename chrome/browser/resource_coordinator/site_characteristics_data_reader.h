// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_READER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_READER_H_

#include "chrome/browser/performance_manager/persistence/site_data/feature_usage.h"

#include "base/callback_forward.h"

namespace resource_coordinator {

// Pure virtual interface to read the characteristics of an origin. This is a
// usable abstraction for both the local and global database.
class SiteCharacteristicsDataReader {
 public:
  SiteCharacteristicsDataReader() = default;
  virtual ~SiteCharacteristicsDataReader() {}

  // Accessors for the site characteristics usage.
  virtual performance_manager::SiteFeatureUsage UpdatesFaviconInBackground()
      const = 0;
  virtual performance_manager::SiteFeatureUsage UpdatesTitleInBackground()
      const = 0;
  virtual performance_manager::SiteFeatureUsage UsesAudioInBackground()
      const = 0;
  virtual performance_manager::SiteFeatureUsage UsesNotificationsInBackground()
      const = 0;

  // Returns true if this reader is fully initialized and serving the most
  // authoritative data. This can initially return false as the backing store is
  // loaded asynchronously.
  virtual bool DataLoaded() const = 0;

  // Registers a callback that will be invoked when the data backing this object
  // has been loaded. Note that if "DataLoaded" is true at the time this is
  // called it may immediately invoke the callback. The callback will not be
  // invoked after this object has been destroyed.
  virtual void RegisterDataLoadedCallback(base::OnceClosure&& callback) = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_READER_H_
