// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/configuration_refresher.h"

#include "components/sync/base/model_type.h"

ConfigurationRefresher::ConfigurationRefresher() = default;
ConfigurationRefresher::~ConfigurationRefresher() = default;

void ConfigurationRefresher::Observe(syncer::SyncService* sync_service) {
  scoped_observer_.Add(sync_service);
}

void ConfigurationRefresher::OnSyncConfigurationCompleted(
    syncer::SyncService* sync_service) {
  // Only allowed to trigger refresh/schedule nudges for protocol types, things
  // like PROXY_TABS are not allowed.
  sync_service->TriggerRefresh(syncer::Intersection(
      sync_service->GetActiveDataTypes(), syncer::ProtocolTypes()));
}
