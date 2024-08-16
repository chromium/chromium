// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WATCHER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/backend_migrator.h"

class SyncServiceImplHarness;
class MigrationWaiter;

// Helper class to observe and record migration state.
class MigrationWatcher : public syncer::MigrationObserver {
 public:
  explicit MigrationWatcher(SyncServiceImplHarness* harness);

  MigrationWatcher(const MigrationWatcher&) = delete;
  MigrationWatcher& operator=(const MigrationWatcher&) = delete;

  ~MigrationWatcher() override;

  // Returns true if the observed profile has a migration in progress.
  bool HasPendingBackendMigration() const;

  // Returns the set of types this class has observed being migrated.
  syncer::DataTypeSet GetMigratedTypes() const;

  // Implementation of syncer::MigrationObserver.
  void OnMigrationStateChange() override;

  // Registers the |waiter| to receive callbacks on migration state change.
  void set_migration_waiter(MigrationWaiter* waiter);

  // Unregisters the current MigrationWaiter, if any.
  void clear_migration_waiter();

 private:
  // The SyncServiceImplHarness to watch.
  const raw_ptr<SyncServiceImplHarness, DanglingUntriaged> harness_;

  // The set of data types currently undergoing migration.
  syncer::DataTypeSet pending_types_;

  // The set of data types for which migration is complete. Accumulated by
  // successive calls to OnMigrationStateChanged.
  syncer::DataTypeSet migrated_types_;

  // The MigrationWatier that is waiting for this migration to complete.
  raw_ptr<MigrationWaiter> migration_waiter_ = nullptr;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WATCHER_H_
