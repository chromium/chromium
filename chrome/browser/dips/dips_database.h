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
  // The length of time since last user interaction or site storage that a
  // site's entry will not be subject to garbage collection due to expiration.
  // However, even with interaction or storage within this period, if there are
  // more than |max_entries_| entries, an entry can still be deleted by
  // |GarbageCollectOldest()|.
  static const base::TimeDelta kMaxAge;

  // The length of time that will be waited between emitting db health metrics.
  static const base::TimeDelta kMetricsInterval;

  // Passing in an absl::nullopt `db_path` causes the db to be created in
  // memory. Init() must be called before using the DIPSDatabase to make sure it
  // is initialized.
  explicit DIPSDatabase(const absl::optional<base::FilePath>& db_path);

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  ~DIPSDatabase();

  DIPSDatabase(const DIPSDatabase&) = delete;
  DIPSDatabase& operator=(const DIPSDatabase&) = delete;

  // DIPS Bounce table functions -----------------------------------------------
  bool Write(const std::string& site,
             const TimestampRange& storage_times,
             const TimestampRange& interaction_times,
             const TimestampRange& stateful_bounce_times,
             const TimestampRange& stateless_bounce_times);

  absl::optional<StateValue> Read(const std::string& site);

  std::vector<std::string> GetAllSitesForTesting() const;

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

  // Returns the number of entries present in the database.
  size_t GetEntryCount() const;

  // If the number of entries in the database is greater than
  // |GetMaxEntries()|, garbage collect. Returns the number of entries deleted
  // (useful for debugging).
  size_t GarbageCollect();

  // Removes entries for sites without user interaction or site storage within
  // |kMaxAge|. Returns the number of entries deleted.
  size_t GarbageCollectExpired();

  // Removes the |purge_goal| entries with the oldest
  // |MAX(last_user_interaction_time,last_site_storage_time)| value. Returns the
  // number of entries deleted.
  size_t GarbageCollectOldest(int purge_goal);

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_path_.empty();
  }

  // Checks that the internal SQLite database is initialized.
  bool CheckDBInit() const;

  size_t GetMaxEntries() const { return max_entries_; }
  size_t GetPurgeEntries() const { return purge_entries_; }

  void SetMaxEntriesForTesting(size_t entries) { max_entries_ = entries; }
  void SetPurgeEntriesForTesting(size_t entries) { purge_entries_ = entries; }

 protected:
  // Initialization functions --------------------------------------------------
  sql::InitStatus Init();
  sql::InitStatus InitImpl();
  sql::InitStatus OpenDatabase();
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

  void ComputeDatabaseMetrics() const;

 private:
  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  // When the number of entries in the database exceeds |max_entries_|, purge
  // down to |max_entries_| - |purge_entries_|.
  size_t max_entries_ = 3500;
  size_t purge_entries_ = 300;
  const base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  mutable base::Time last_health_metrics_time_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::Time::Min();
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
