// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONFIGURATION_REFRESHER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONFIGURATION_REFRESHER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
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
  ~ConfigurationRefresher() override;
  void Observe(syncer::SyncService* sync_service);

 private:
  // syncer::SyncServiceObserver implementation.
  void OnSyncConfigurationCompleted(syncer::SyncService* sync_service) override;

  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ConfigurationRefresher);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONFIGURATION_REFRESHER_H_
