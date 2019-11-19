// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/noop_site_data_writer.h"

namespace performance_manager {

NoopSiteDataWriter::~NoopSiteDataWriter() = default;

void NoopSiteDataWriter::NotifySiteLoaded() {}

void NoopSiteDataWriter::NotifySiteUnloaded() {}

void NoopSiteDataWriter::NotifySiteVisibilityChanged(
    performance_manager::TabVisibility visibility) {}

void NoopSiteDataWriter::NotifyUpdatesFaviconInBackground() {}

void NoopSiteDataWriter::NotifyUpdatesTitleInBackground() {}

void NoopSiteDataWriter::NotifyUsesAudioInBackground() {}

void NoopSiteDataWriter::NotifyUsesNotificationsInBackground() {}

void NoopSiteDataWriter::NotifyLoadTimePerformanceMeasurement(
    base::TimeDelta load_duration,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {}

NoopSiteDataWriter::NoopSiteDataWriter()
    : SiteDataWriter(nullptr, performance_manager::TabVisibility::kForeground) {
}

}  // namespace performance_manager
