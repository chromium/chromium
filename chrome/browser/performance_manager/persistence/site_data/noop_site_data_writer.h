// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NOOP_SITE_DATA_WRITER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NOOP_SITE_DATA_WRITER_H_

#include "base/macros.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_writer.h"

namespace performance_manager {

// Specialization of a SiteDataWriter that doesn't record anything.
class NoopSiteDataWriter : public SiteDataWriter {
 public:
  ~NoopSiteDataWriter() override;

  // Implementation of SiteDataWriter:
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
  friend class NonRecordingSiteDataCache;
  // Private constructor, these objects are meant to be created by a
  // NonRecordingSiteDataCache.
  NoopSiteDataWriter();

  DISALLOW_COPY_AND_ASSIGN(NoopSiteDataWriter);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NOOP_SITE_DATA_WRITER_H_
