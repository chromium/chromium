// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/migration_waiter.h"

#include "base/check.h"
#include "chrome/browser/sync/test/integration/migration_watcher.h"

MigrationWaiter::MigrationWaiter(syncer::ModelTypeSet expected_types,
                                 MigrationWatcher* watcher)
    : watcher_(watcher), expected_types_(expected_types) {
  DCHECK(!expected_types_.Empty());
  watcher_->set_migration_waiter(this);
}

MigrationWaiter::~MigrationWaiter() {
  watcher_->clear_migration_waiter();
}

// Returns true when sync reports that there is no pending migration, and
// migration is complete for all data types in |expected_types_|.
bool MigrationWaiter::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting to migrate (" + ModelTypeSetToString(expected_types_) +
             "); " + "Currently migrated: (" +
             ModelTypeSetToString(watcher_->GetMigratedTypes()) + ")";
  return watcher_->GetMigratedTypes().HasAll(expected_types_) &&
         !watcher_->HasPendingBackendMigration();
}

void MigrationWaiter::OnMigrationStateChange() {
  CheckExitCondition();
}
