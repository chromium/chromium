// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
#define CHROME_BROWSER_DIPS_DIPS_DATABASE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "chrome/browser/dips/dips_utils.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/statement.h"

// TODO(crbug.com/1342228): This is currently in-memory only. Add support for a
// persistent SQLite database to be used for non-OTR profiles.
//
// Encapsulates an SQL database that holds DIPS info.
class DIPSDatabase {
 public:
  // Passing in an absl::nullopt `db_path` causes the db to be created in
  // memory. Init() must be called before using the DIPSDatabase to make sure it
  // is initialized.
  explicit DIPSDatabase(const absl::optional<base::FilePath>& db_path);

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  ~DIPSDatabase();

  DIPSDatabase(const DIPSDatabase&) = delete;
  DIPSDatabase& operator=(const DIPSDatabase&) = delete;

  // Must be called after creation but before any other methods are called.
  // When not INIT_OK, no other functions should be called.
  sql::InitStatus Init();

  // DIPS Bounce table functions -----------------------------------------------
  bool Write(const std::string& site,
             const TimestampRange& storage_times,
             const TimestampRange& interaction_times,
             const TimestampRange& stateful_bounce_times,
             const TimestampRange& stateless_bounce_times);

  absl::optional<StateValue> Read(const std::string& site);

  // Returns all sites that did a bounce after |range_start| with their last
  // interaction happening before |last_interaction|.
  //
  // Note: |last_interaction| must be earlier than |range_start|.
  std::vector<std::string> GetSitesThatBounced(
      base::Time range_start,
      base::Time last_interaction) const;

  // Returns all sites that did a stateful bounce after |range_start| with their
  // last interaction happening before |last_interaction|.
  //
  // Note: |last_interaction| must be earlier than |range_start|.
  std::vector<std::string> GetSitesThatBouncedWithState(
      base::Time range_start,
      base::Time last_interaction) const;

  // Returns all sites that wrote to storage after |range_start| with their last
  // interaction happening before |last_interaction|.
  //
  // Note: |last_interaction| must be earlier than |range_start|.
  std::vector<std::string> GetSitesThatUsedStorage(
      base::Time range_start,
      base::Time last_interaction) const;

  // Delete the row from the bounces table for `site`. Returns true if query
  // executes successfully.
  bool RemoveRow(const std::string& site);

  bool RemoveEventsByTime(const base::Time& delete_begin,
                          const base::Time& delete_end,
                          const DIPSEventRemovalType type);

  bool in_memory() const { return db_path_.empty(); }

 protected:
  // Initialization functions --------------------------------------------------
  sql::InitStatus OpenDatabase();
  sql::InitStatus InitImpl();
  bool InitTables();

  // Internal utility functions ------------------------------------------------
  bool ClearTimestamps(const base::Time& delete_begin,
                       const base::Time& delete_end,
                       const DIPSEventRemovalType type);
  bool AdjustFirstTimestamps(const base::Time& delete_begin,
                             const base::Time& delete_end,
                             const DIPSEventRemovalType type);
  bool AdjustLastTimestamps(const base::Time& delete_begin,
                            const base::Time& delete_end,
                            const DIPSEventRemovalType type);

 private:
  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
