// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_DATABASE_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_DATABASE_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "extensions/common/extension.h"
#include "sql/database.h"

namespace base {
class FilePath;
}

namespace extensions {

// Encapsulates the SQL connection for the activity log database.  This class
// holds the database connection and has methods for writing.  All of the
// methods except the constructor need to be called on the DB thread.
//
// Object ownership and lifetime is a bit complicated for ActivityLog,
// ActivityLogPolicy, and ActivityDatabase:
//
//    ActivityLog ----> ActivityLogPolicy ----> ActivityDatabase
//                         ^                               |
//                         |                               |
//                         \--(ActivityDatabase::Delegate)-/
//
// The ActivityLogPolicy object contains a pointer to the ActivityDatabase, and
// the ActivityDatabase contains a pointer back to the ActivityLogPolicy object
// (as an instance of ActivityDatabase::Delegate).
//
// Since some cleanup must happen on the database thread, deletion should occur
// as follows:
//   1. ActivityLog calls ActivityLogPolicy::Close()
//   2. ActivityLogPolicy should call ActivityDatabase::Close() on the database
//      thread.
//   3. ActivityDatabase::Close() shuts down the database, then calls
//      ActivityDatabase::Delegate::OnDatabaseClose().
//   4. ActivityDatabase::Delegate::OnDatabaseClose() should delete the
//      ActivityLogPolicy object.
//   5. ActivityDatabase::Close() finishes running by deleting the
//      ActivityDatabase object.
//
// (This assumes the common case that the ActivityLogPolicy uses an
// ActivityDatabase and implements the ActivityDatabase::Delegate interface.
// It is also possible for an ActivityLogPolicy to not use a database at all,
// in which case ActivityLogPolicy::Close() should directly delete itself.)
class ActivityDatabase {
 public:
  // Interface defining calls that the ActivityDatabase can make into a
  // ActivityLogPolicy instance to implement policy-specific behavior.  Methods
  // are always invoked on the database thread.  Classes other than
  // ActivityDatabase should not call these methods.
  class Delegate {
   protected:
    friend class ActivityDatabase;

    // A Delegate is never directly deleted; it should instead delete itself
    // after any final cleanup when OnDatabaseClose() is invoked.
    virtual ~Delegate() {}

    // Initializes the database schema; this gives a policy a chance to create
    // or update database tables as needed.  Should return true on success.
    // Will be called from within a database transaction.
    virtual bool InitDatabase(sql::Database* db) = 0;

    // Requests that the policy flush any pending actions to the database.
    // Should return true on success or false on a database error.  Will not be
    // called from a transaction (the implementation may wish to use a
    // transaction for the flush).
    virtual bool FlushDatabase(sql::Database* db) = 0;

    // Called if the database encounters a permanent error; the policy should
    // not expect to make any future writes to the database and may want to
    // discard any queued data.
    virtual void OnDatabaseFailure() = 0;

    // Called by ActivityDatabase just before the ActivityDatabase object is
    // deleted.  The database will make no further callbacks after invoking
    // this method, so it is an appropriate time for the policy to delete
    // itself.
    virtual void OnDatabaseClose() = 0;
  };

  // Value to be passed to AdviseFlush below to force a database flush.
  static const int kFlushImmediately = -1;

  // Need to call Init to actually use the ActivityDatabase.  The Delegate
  // provides hooks for an ActivityLogPolicy to control the database schema and
  // reads/writes.
  explicit ActivityDatabase(Delegate* delegate);

  ActivityDatabase(const ActivityDatabase&) = delete;
  ActivityDatabase& operator=(const ActivityDatabase&) = delete;

  // Opens the DB.  This invokes OnDatabaseInit in the delegate to create or
  // update the database schema if needed.
  void Init(const base::FilePath& db_name);

  // An ActivityLogPolicy should call this to kill the ActivityDatabase.
  void Close();

  // Inform the database that there may be additional data which could be
  // written out.  The size parameter should indicate (approximately) how many
  // records are queued to be written; the database may use this information to
  // schedule a flush early if too much data is queueing up.  A value of
  // kFlushImmediately will force an immediate call into
  // Delegate::FlushDatabase(); otherwise, it is up to the database to
  // determine when to flush.
  void AdviseFlush(int size);

  // Turns off batch I/O writing mode. This should only be used in unit tests,
  // browser tests, or in our special --enable-extension-activity-log-testing
  // policy state.
  void SetBatchModeForTesting(bool batch_mode);

  bool is_db_valid() const { return valid_db_; }

  // A helper method for initializing or upgrading a database table.  The
  // content_fields array should list the names of all of the columns in the
  // database. The field_types should specify the types of the corresponding
  // columns (e.g., INTEGER or LONGVARCHAR). There should be the same number of
  // field_types as content_fields, since the two arrays should correspond.
  static bool InitializeTable(sql::Database* db,
                              base::cstring_view table_name,
                              const char* const content_fields[],
                              const char* const field_types[],
                              const int num_content_fields);

 private:
  // This should never be invoked by another class. Use Close() to order a
  // suicide.
  virtual ~ActivityDatabase();

  // Used by the Init() method as a convenience for handling a failed database
  // initialization attempt. Prints an error and puts us in the soft failure
  // state.
  void LogInitFailure();

  // When we're in batched mode (which is on by default), we write to the db
  // every X minutes instead of on every API call. This prevents the annoyance
  // of writing to disk multiple times a second.
  void RecordBatchedActions();

  // If an error is unrecoverable or occurred while we were trying to close
  // the database properly, we take "emergency" actions: break any outstanding
  // transactions, raze the database, and close. When next opened, the
  // database will be empty.
  void HardFailureClose();

  // Doesn't actually close the DB, but changes bools to prevent further writes
  // or changes to it.
  void SoftFailureClose();

  // Handle errors in database writes. For a serious & permanent error, it
  // invokes HardFailureClose(); for a less serious/permanent error, it invokes
  // SoftFailureClose().
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // For unit testing only.
  void RecordBatchedActionsWhileTesting();
  void SetTimerForTesting(int milliseconds);

  // Retrieve a handle to the raw SQL database.  This is only intended to be
  // used by ActivityLogDatabasePolicy::GetDatabaseConnection(), and should
  // only be called on the database thread.
  sql::Database* GetSqlConnection();

  SEQUENCE_CHECKER(sequence_checker_);

  // A reference a Delegate for policy-specific database behavior.  See the
  // top-level comment for ActivityDatabase for comments on cleanup.
  raw_ptr<Delegate, DanglingUntriaged> delegate_;

  sql::Database db_;
  bool valid_db_;
  bool batch_mode_;
  base::TimeDelta batching_period_;
  base::RepeatingTimer timer_;
  bool already_closed_;
  bool did_init_;

  friend class ActivityLogDatabasePolicy;
  FRIEND_TEST_ALL_PREFIXES(ActivityDatabaseTest, BatchModeOff);
  FRIEND_TEST_ALL_PREFIXES(ActivityDatabaseTest, BatchModeOn);
  FRIEND_TEST_ALL_PREFIXES(ActivityDatabaseTest, BatchModeFlush);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_DATABASE_H_
