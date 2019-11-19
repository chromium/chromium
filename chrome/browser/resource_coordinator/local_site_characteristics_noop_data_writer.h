// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_NOOP_DATA_WRITER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_NOOP_DATA_WRITER_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_writer.h"

namespace resource_coordinator {

// Specialization of a SiteCharacteristicsDataWriter that doesn't record
// anyting.
class LocalSiteCharacteristicsNoopDataWriter
    : public SiteCharacteristicsDataWriter {
 public:
  LocalSiteCharacteristicsNoopDataWriter();
  ~LocalSiteCharacteristicsNoopDataWriter() override;

  // SiteCharacteristicsDataWriter:
  void NotifySiteLoaded() override;
  void NotifySiteUnloaded() override;
  void NotifySiteVisibilityChanged(
      performance_manager::TabVisibility visibility) override;
  void NotifyUpdatesFaviconInBackground() override;
  void NotifyUpdatesTitleInBackground() override;
  void NotifyUsesAudioInBackground() override;
  void NotifyUsesNotificationsInBackground() override;
  void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsNoopDataWriter);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_NOOP_DATA_WRITER_H_
