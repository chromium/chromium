// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_WRITER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_WRITER_H_

#include "base/time/time.h"
#include "chrome/browser/performance_manager/persistence/site_data/tab_visibility.h"

namespace resource_coordinator {

// Pure virtual interface to record the observations made for an origin.
class SiteCharacteristicsDataWriter {
 public:
  SiteCharacteristicsDataWriter() = default;
  virtual ~SiteCharacteristicsDataWriter() {}

  // Records tab load/unload events.
  virtual void NotifySiteLoaded() = 0;
  virtual void NotifySiteUnloaded() = 0;

  // Records visibility change events.
  virtual void NotifySiteVisibilityChanged(
      performance_manager::TabVisibility visibility) = 0;

  // Records feature usage.
  virtual void NotifyUpdatesFaviconInBackground() = 0;
  virtual void NotifyUpdatesTitleInBackground() = 0;
  virtual void NotifyUsesAudioInBackground() = 0;
  virtual void NotifyUsesNotificationsInBackground() = 0;

  // Records performance measurements.
  virtual void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_SITE_CHARACTERISTICS_DATA_WRITER_H_
