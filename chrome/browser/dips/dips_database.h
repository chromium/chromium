// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
#define CHROME_BROWSER_DIPS_DIPS_DATABASE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

// Encapsulates an SQL database that holds DIPS info.
class DIPSDatabase {
 public:
  // The length of time that will be waited between emitting db health metrics.
  static const base::TimeDelta kMetricsInterval;

  // How long DIPS maintains popups in storage (for recording Popup Heuristic
  // storage accesses).
  static const base::TimeDelta kPopupTtl;

  // Passing in an absl::nullopt `db_path` causes the db to be created in
  // memory. Init() must be called before using the DIPSDatabase to make sure it
  // is initialized.
  explicit DIPSDatabase(const absl::optional<base::FilePath>& db_path);

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  ~DIPSDatabase();

  DIPSDatabase(const DIPSDatabase&) = delete;
  DIPSDatabase& operator=(const DIPSDatabase&) = delete;

  // Updates `db_` to use the latest schema. Returns whether the migration was
  // successful.
  bool MigrateAsNeeded();

  // Migrates from v1 to v2 of the DIPS database schema. This migration:
  // - Makes all timestamp columns nullable instead of using base::Time() as
  // default.
  // - Replaces both the first and last stateless bounce columns to track the
  // first and last bounce times instead.
  bool MigrateToVersion2();

  // Migrates from v2 to v3 of the DIPS database schema. This migration adds two
  // extra columns for recording the first and last time a web authn assertion
  // was called.
  bool MigrateToVersion3();
  bool MigrateToVersion4();

  // DIPS Bounce table functions -----------------------------------------------
  bool Write(const std::string& site,
             const TimestampRange& storage_times,
             const TimestampRange& interaction_times,
             const TimestampRange& stateful_bounce_times,
             const TimestampRange& bounce_times,
             const TimestampRange& web_authn_assertion_times);

  bool WritePopup(const std::string& opener_site,
                  const std::string& popup_site,
                  const uint64_t access_id,
                  const base::Time& popup_time);

  // This is implicitly `inline`. Don't move its definition to the .cc file.
  bool HasExpired(absl::optional<base::Time> time) {
    return time.has_value() &&
           (time.value() + dips::kInteractionTtl.Get()) < clock_->Now();
  }

  absl::optional<StateValue> Read(const std::string& site);

  absl::optional<PopupsStateValue> ReadPopup(const std::string& opener_site,
                                             const std::string& popup_site);

  // Note: this doesn't clear expired interactions from the database unlike
  // the other database querying methods.
  std::vector<std::string> GetAllSitesForTesting(const DIPSDatabaseTable table);

  // Returns the subset of sites in |sites| WITH user interaction or successful
  // web authn assertion recorded.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  //
  // TODO(njeunje): Consider making this FilterSites(set<string> sites,
  // FilterType filter) where FilterType lets us specify if we want to filter
  // out interactions, web authn assertions, or both. There may be other
  // criteria that we want to filter for in the future & this name might get
  // even longer.
  std::set<std::string> FilterSitesWithInteractionOrWaa(
      const std::set<std::string>& sites);

  // Returns all sites which bounced the user and aren't protected from DIPS.
  //
  // A site can be protected in several ways:
  // - it's still in its grace period after the first bounce
  // - it received user interaction or WAA before the first bounce
  // - it received user interaction or WAA in the grace period after the first
  // bounce.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  std::vector<std::string> GetSitesThatBounced(base::TimeDelta grace_period);

  // Returns all sites which used storage and aren't protected from DIPS.
  //
  // A site can be protected in several ways:
  // - it's still in its grace period after the first storage
  // - it received user interaction or WAA before the first storage
  // - it received user interaction or WAA in the grace period after the first
  // storage.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  std::vector<std::string> GetSitesThatUsedStorage(
      base::TimeDelta grace_period);

  // Returns all sites which statefully bounced the user and aren't protected
  // from DIPS.
  //
  // A site can be protected in several ways:
  // - it's still in its grace period after the first stateful bounce
  // - it received user interaction or WAA before the first stateful bounce
  // - it received user interaction or WAA in the grace period after the first
  // stateful bounce.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  std::vector<std::string> GetSitesThatBouncedWithState(
      base::TimeDelta grace_period);

  // Deletes all rows in the database whose interactions have expired out.
  //
  // When an interaction happens before a DIPS-triggering action or during the
  // following grace-period, it protects that site from its data being cleared
  // by DIPS. Further interactions will prolong that protection until the last
  // one reaches the `interaction_ttl`.
  //
  // Clearing expired interactions effectively restarts the DIPS procedure for
  // determining if a site is a tracker for sites that are cleared.
  //
  // Returns the number of rows that are removed.
  size_t ClearExpiredRows();

  // Delete the row from the bounces table for `site`. Returns true if query
  // executes successfully.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool RemoveRow(const DIPSDatabaseTable table, const std::string& site);

  bool RemoveRows(const DIPSDatabaseTable table,
                  const std::vector<std::string>& sites);

  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool RemoveEventsByTime(const base::Time& delete_begin,
                          const base::Time& delete_end,
                          const DIPSEventRemovalType type);

  // Because |sites| is taken from a ClearDataFilter, this only removes
  // storage and stateful bounce timestamps at the moment.
  bool RemoveEventsBySite(bool preserve,
                          const std::vector<std::string>& sites,
                          const DIPSEventRemovalType type);

  // Returns the number of entries present in the database.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  size_t GetEntryCount(const DIPSDatabaseTable table);

  // If the number of entries in the database is greater than
  // |GetMaxEntries()|, garbage collect. Returns the number of entries deleted
  // (useful for debugging).
  size_t GarbageCollect();

  // Removes the |purge_goal| entries with the oldest
  // |MAX(last_user_interaction_time,last_site_storage_time)| value. Returns the
  // number of entries deleted.
  //
  // NOTE: The SQLITE sub-query in this method must match that of
  // `GetGarbageCollectOldestSitesForTesting()`'s query.
  size_t GarbageCollectOldest(const DIPSDatabaseTable table, int purge_goal);

  // Used for testing the intended behavior `GarbageCollectOldest()`.
  //
  // NOTE: The SQLITE query in this method must match that of
  // `GarbageCollectOldest()`'s sub-query.
  std::vector<std::string> GetGarbageCollectOldestSitesForTesting(
      const DIPSDatabaseTable table);

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_path_.empty();
  }

  // Checks that the internal SQLite database is initialized.
  bool CheckDBInit();

  // Marks meta_table_'s `prepopulated` field as true.
  //
  // Called once this database has finished prepopulating with information from
  // the SiteEngagement service. Returns whether this operation was successful.
  bool MarkAsPrepopulated();

  // Whether the database was prepopulated using the SiteEngagement service.
  bool IsPrepopulated();

  size_t GetMaxEntries() const { return max_entries_; }
  size_t GetPurgeEntries() const { return purge_entries_; }

  // Testing functions --------------------------------------------------
  void SetMaxEntriesForTesting(size_t entries) { max_entries_ = entries; }
  void SetPurgeEntriesForTesting(size_t entries) { purge_entries_ = entries; }
  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  bool ExecuteSqlForTesting(const char* sql);

 protected:
  // Initialization functions --------------------------------------------------
  sql::InitStatus Init();
  sql::InitStatus InitImpl();
  sql::InitStatus OpenDatabase();
  // Creates the bounce table following the latest schema.
  bool InitTables();

  // Internal utility functions ------------------------------------------------

  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool ClearTimestamps(const base::Time& delete_begin,
                       const base::Time& delete_end,
                       const DIPSEventRemovalType type);
  bool ClearTimestampsBySite(bool preserve,
                             const std::vector<std::string>& sites,
                             const DIPSEventRemovalType type);
  bool RemoveEmptyRows();

  void LogDatabaseMetrics();

 private:
  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  // Only ClearTimestamps() should call this method.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool AdjustFirstTimestamps(const base::Time& delete_begin,
                             const base::Time& delete_end,
                             const DIPSEventRemovalType type);
  // Only ClearTimestamps() should call this method.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool AdjustLastTimestamps(const base::Time& delete_begin,
                            const base::Time& delete_end,
                            const DIPSEventRemovalType type);

  // When the number of entries in the database exceeds |max_entries_|, purge
  // down to |max_entries_| - |purge_entries_|.
  size_t max_entries_ = 3500;
  size_t purge_entries_ = 300;
  const base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  mutable base::Time last_health_metrics_time_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::Time::Min();
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
