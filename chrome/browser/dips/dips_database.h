// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
#define CHROME_BROWSER_DIPS_DIPS_DATABASE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/statement.h"

namespace base {
class Time;
}

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
             absl::optional<base::Time> first_storage_time,
             absl::optional<base::Time> first_interaction_time) {
    return Write(site, first_storage_time, first_storage_time,
                 first_interaction_time, first_interaction_time);
  }

  bool Write(const std::string& site,
             absl::optional<base::Time> first_storage_time,
             absl::optional<base::Time> last_storage_time,
             absl::optional<base::Time> first_interaction_time,
             absl::optional<base::Time> last_interaction_time);

  absl::optional<StateValue> Read(const std::string& site);

  // Delete the row from the bounces table for `site`. Returns true if query
  // executes successfully.
  bool RemoveRow(const std::string& site);

  bool in_memory() const { return db_path_.empty(); }

 protected:
  // Initialization functions --------------------------------------------------
  sql::InitStatus OpenDatabase();
  sql::InitStatus InitImpl();
  bool InitTables();

 private:
  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_DIPS_DIPS_DATABASE_H_
