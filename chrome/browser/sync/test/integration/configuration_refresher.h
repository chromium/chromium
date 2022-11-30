// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONFIGURATION_REFRESHER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONFIGURATION_REFRESHER_H_

#include "base/scoped_multi_source_observation.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

// Triggers a GetUpdates via refresh for any observed SyncService after a
// configuration. This class was created to be used in conjunction with fake
// invalidations. It turns out there's a race during configuration, after the
// initial GetUpdates was called, but before invalidations were re-subscribed to
// that caused updates to be missed. This resulted in some flakey test cases,
// see crbug,com/644367 for more details. This class fills the gap by forcing a
// GetUpdates after configuration to fetch anything missed while a client was
// not subscribed to invalidation(s).
class ConfigurationRefresher : public syncer::SyncServiceObserver {
 public:
  ConfigurationRefresher();

  ConfigurationRefresher(const ConfigurationRefresher&) = delete;
  ConfigurationRefresher& operator=(const ConfigurationRefresher&) = delete;

  ~ConfigurationRefresher() override;
  void Observe(syncer::SyncService* sync_service);

 private:
  // syncer::SyncServiceObserver implementation.
  void OnSyncConfigurationCompleted(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  base::ScopedMultiSourceObservation<syncer::SyncService,
                                     syncer::SyncServiceObserver>
      scoped_observations_{this};
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONFIGURATION_REFRESHER_H_
