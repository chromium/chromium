// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_noop_data_writer.h"

namespace resource_coordinator {

LocalSiteCharacteristicsNoopDataWriter::
    LocalSiteCharacteristicsNoopDataWriter() = default;
LocalSiteCharacteristicsNoopDataWriter::
    ~LocalSiteCharacteristicsNoopDataWriter() = default;

void LocalSiteCharacteristicsNoopDataWriter::NotifySiteLoaded() {}

void LocalSiteCharacteristicsNoopDataWriter::NotifySiteUnloaded() {}

void LocalSiteCharacteristicsNoopDataWriter::NotifySiteVisibilityChanged(
    performance_manager::TabVisibility visibility) {}

void LocalSiteCharacteristicsNoopDataWriter::
    NotifyUpdatesFaviconInBackground() {}

void LocalSiteCharacteristicsNoopDataWriter::NotifyUpdatesTitleInBackground() {}

void LocalSiteCharacteristicsNoopDataWriter::NotifyUsesAudioInBackground() {}

void LocalSiteCharacteristicsNoopDataWriter::
    NotifyUsesNotificationsInBackground() {}

void LocalSiteCharacteristicsNoopDataWriter::
    NotifyLoadTimePerformanceMeasurement(
        base::TimeDelta load_duration,
        base::TimeDelta cpu_usage_estimate,
        uint64_t private_footprint_kb_estimate) {}

}  // namespace resource_coordinator
