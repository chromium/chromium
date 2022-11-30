// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/configuration_refresher.h"

#include "components/sync/base/model_type.h"

ConfigurationRefresher::ConfigurationRefresher() = default;
ConfigurationRefresher::~ConfigurationRefresher() = default;

void ConfigurationRefresher::Observe(syncer::SyncService* sync_service) {
  scoped_observations_.AddObservation(sync_service);
}

void ConfigurationRefresher::OnSyncConfigurationCompleted(
    syncer::SyncService* sync_service) {
  sync_service->TriggerRefresh(syncer::ModelTypeSet::All());
}

void ConfigurationRefresher::OnSyncShutdown(syncer::SyncService* sync_service) {
  scoped_observations_.RemoveObservation(sync_service);
}
