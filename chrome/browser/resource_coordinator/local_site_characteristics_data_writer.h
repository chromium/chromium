// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_WRITER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_WRITER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_writer.h"

namespace resource_coordinator {

// Specialization of a SiteCharacteristicsDataWriter that delegates to a
// LocalSiteCharacteristicsDataImpl.
//
// This writer is initially in an unloaded state, a |NotifySiteLoaded| event
// should be sent if/when the tab using it gets loaded.
class LocalSiteCharacteristicsDataWriter
    : public SiteCharacteristicsDataWriter {
 public:
  ~LocalSiteCharacteristicsDataWriter() override;

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

  internal::LocalSiteCharacteristicsDataImpl* impl_for_testing() const {
    return impl_.get();
  }

 private:
  friend class LocalSiteCharacteristicsDataWriterTest;
  friend class LocalSiteCharacteristicsDataStoreTest;
  friend class LocalSiteCharacteristicsDataStore;

  // Private constructor, these objects are meant to be created by a site
  // characteristics data store.
  LocalSiteCharacteristicsDataWriter(
      scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl,
      performance_manager::TabVisibility tab_visibility);

  // The LocalSiteCharacteristicDataInternal object we delegate to.
  const scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl_;

  // The visibility of the tab using this writer.
  performance_manager::TabVisibility tab_visibility_;

  // Indicates if the tab using this writer is loaded.
  bool is_loaded_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataWriter);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_WRITER_H_
