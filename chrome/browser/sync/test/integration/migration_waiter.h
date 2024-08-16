// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WAITER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/sync/base/data_type.h"

class MigrationWatcher;

// Helper class that checks if the sync backend has successfully completed
// migration for a set of data types.
//
// Collaborates with the MigrationWatcher, defined above.
class MigrationWaiter : public StatusChangeChecker {
 public:
  // Initialize a waiter that will wait until |watcher|'s migrated types
  // match the provided |exptected_types|.
  MigrationWaiter(syncer::DataTypeSet expected_types,
                  MigrationWatcher* watcher);

  MigrationWaiter(const MigrationWaiter&) = delete;
  MigrationWaiter& operator=(const MigrationWaiter&) = delete;

  ~MigrationWaiter() override;

  // StatusChangeChecker implementation .
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // Callback invoked by our associated waiter when migration state changes.
  void OnMigrationStateChange();

 private:
  // The MigrationWatcher we're observering.
  const raw_ptr<MigrationWatcher> watcher_;

  // The set of data types that are expected to eventually undergo migration.
  const syncer::DataTypeSet expected_types_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_MIGRATION_WAITER_H_
