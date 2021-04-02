// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WATCHER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WATCHER_H_

#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/backend_migrator.h"

class ProfileSyncServiceHarness;
class MigrationWaiter;

// Helper class to observe and record migration state.
class MigrationWatcher : public syncer::MigrationObserver {
 public:
  explicit MigrationWatcher(ProfileSyncServiceHarness* harness);
  ~MigrationWatcher() override;

  // Returns true if the observed profile has a migration in progress.
  bool HasPendingBackendMigration() const;

  // Returns the set of types this class has observed being migrated.
  syncer::ModelTypeSet GetMigratedTypes() const;

  // Implementation of syncer::MigrationObserver.
  void OnMigrationStateChange() override;

  // Registers the |waiter| to receive callbacks on migration state change.
  void set_migration_waiter(MigrationWaiter* waiter);

  // Unregisters the current MigrationWaiter, if any.
  void clear_migration_waiter();

 private:
  // The ProfileSyncServiceHarness to watch.
  ProfileSyncServiceHarness* const harness_;

  // The set of data types currently undergoing migration.
  syncer::ModelTypeSet pending_types_;

  // The set of data types for which migration is complete. Accumulated by
  // successive calls to OnMigrationStateChanged.
  syncer::ModelTypeSet migrated_types_;

  // The MigrationWatier that is waiting for this migration to complete.
  MigrationWaiter* migration_waiter_;

  DISALLOW_COPY_AND_ASSIGN(MigrationWatcher);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WATCHER_H_
