// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_database.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/common/chrome_switches.h"
#include "sql/error_delegate_util.h"
#include "sql/init_status.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace extensions {

// A size threshold at which data should be flushed to the database.  The
// ActivityDatabase will signal the Delegate to write out data based on a
// periodic timer, but will also initiate a flush if AdviseFlush indicates that
// more than kSizeThresholdForFlush action records are queued in memory.  This
// should be set large enough that write costs can be amortized across many
// records, but not so large that too much space can be tied up holding records
// in memory.
static const int kSizeThresholdForFlush = 200;

ActivityDatabase::ActivityDatabase(ActivityDatabase::Delegate* delegate)
    : delegate_(delegate),
      db_({.exclusive_locking = false, .page_size = 4096, .cache_size = 32}),
      valid_db_(false),
      batch_mode_(true),
      already_closed_(false),
      did_init_(false) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogTesting)) {
    batching_period_ = base::TimeDelta::FromSeconds(10);
  } else {
    batching_period_ = base::TimeDelta::FromMinutes(2);
  }
}

ActivityDatabase::~ActivityDatabase() {}

void ActivityDatabase::Init(const base::FilePath& db_name) {
  if (did_init_)
    return;
  did_init_ = true;
  DCHECK(GetActivityLogTaskRunner()->RunsTasksInCurrentSequence());
  db_.set_histogram_tag("Activity");
  db_.set_error_callback(base::BindRepeating(
      &ActivityDatabase::DatabaseErrorCallback, base::Unretained(this)));

  // This db does not use [meta] table, store mmap status data elsewhere.
  db_.set_mmap_alt_status();

  if (!db_.Open(db_name)) {
    LOG(ERROR) << db_.GetErrorMessage();
    return LogInitFailure();
  }

  // Wrap the initialization in a transaction so that the db doesn't
  // get corrupted if init fails/crashes.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return LogInitFailure();

#if defined(OS_MAC)
  // Exclude the database from backups.
  base::mac::SetFileBackupExclusion(db_name);
#endif

  if (!delegate_->InitDatabase(&db_))
    return LogInitFailure();

  sql::InitStatus stat = committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
  if (stat != sql::INIT_OK)
    return LogInitFailure();

  // Pre-loads the first <cache-size> pages into the cache.
  // Doesn't do anything if the database is new.
  db_.Preload();

  valid_db_ = true;
  timer_.Start(FROM_HERE,
               batching_period_,
               this,
               &ActivityDatabase::RecordBatchedActions);
}

void ActivityDatabase::LogInitFailure() {
  LOG(ERROR) << "Couldn't initialize the activity log database.";
  SoftFailureClose();
}

void ActivityDatabase::AdviseFlush(int size) {
  if (!valid_db_)
    return;
  if (!batch_mode_ || size == kFlushImmediately ||
      size >= kSizeThresholdForFlush) {
    if (!delegate_->FlushDatabase(&db_))
      SoftFailureClose();
  }
}

void ActivityDatabase::RecordBatchedActions() {
  if (valid_db_) {
    if (!delegate_->FlushDatabase(&db_))
      SoftFailureClose();
  }
}

void ActivityDatabase::SetBatchModeForTesting(bool batch_mode) {
  if (batch_mode && !batch_mode_) {
    timer_.Start(FROM_HERE,
                 batching_period_,
                 this,
                 &ActivityDatabase::RecordBatchedActions);
  } else if (!batch_mode && batch_mode_) {
    timer_.Stop();
    RecordBatchedActions();
  }
  batch_mode_ = batch_mode;
}

sql::Database* ActivityDatabase::GetSqlConnection() {
  DCHECK(GetActivityLogTaskRunner()->RunsTasksInCurrentSequence());
  if (valid_db_) {
    return &db_;
  } else {
    return NULL;
  }
}

void ActivityDatabase::Close() {
  timer_.Stop();
  if (!already_closed_) {
    RecordBatchedActions();
    db_.reset_error_callback();
  }
  valid_db_ = false;
  already_closed_ = true;
  // Call DatabaseCloseCallback() just before deleting the ActivityDatabase
  // itself--these two objects should have the same lifetime.
  delegate_->OnDatabaseClose();
  delete this;
}

void ActivityDatabase::HardFailureClose() {
  if (already_closed_) return;
  valid_db_ = false;
  timer_.Stop();
  db_.reset_error_callback();
  db_.RazeAndClose();
  delegate_->OnDatabaseFailure();
  already_closed_ = true;
}

void ActivityDatabase::SoftFailureClose() {
  valid_db_ = false;
  timer_.Stop();
  delegate_->OnDatabaseFailure();
}

void ActivityDatabase::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  if (sql::IsErrorCatastrophic(error)) {
    LOG(ERROR) << "Killing the ActivityDatabase due to catastrophic error.";
    HardFailureClose();
  } else if (error != SQLITE_BUSY) {
    // We ignore SQLITE_BUSY errors because they are presumably transient.
    LOG(ERROR) << "Closing the ActivityDatabase due to error.";
    SoftFailureClose();
  }
}

void ActivityDatabase::RecordBatchedActionsWhileTesting() {
  RecordBatchedActions();
  timer_.Stop();
}

void ActivityDatabase::SetTimerForTesting(int ms) {
  timer_.Stop();
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(ms),
               this,
               &ActivityDatabase::RecordBatchedActionsWhileTesting);
}

// static
bool ActivityDatabase::InitializeTable(sql::Database* db,
                                       const char* table_name,
                                       const char* const content_fields[],
                                       const char* const field_types[],
                                       const int num_content_fields) {
  if (!db->DoesTableExist(table_name)) {
    std::string table_creator =
        base::StringPrintf("CREATE TABLE %s (", table_name);
    for (int i = 0; i < num_content_fields; i++) {
      table_creator += base::StringPrintf("%s%s %s",
                                          i == 0 ? "" : ", ",
                                          content_fields[i],
                                          field_types[i]);
    }
    table_creator += ")";
    if (!db->Execute(table_creator.c_str()))
      return false;
  } else {
    // In case we ever want to add new fields, this initializes them to be
    // empty strings.
    for (int i = 0; i < num_content_fields; i++) {
      if (!db->DoesColumnExist(table_name, content_fields[i])) {
        std::string table_updater = base::StringPrintf(
            "ALTER TABLE %s ADD COLUMN %s %s; ",
             table_name,
             content_fields[i],
             field_types[i]);
        if (!db->Execute(table_updater.c_str()))
          return false;
      }
    }
  }
  return true;
}

}  // namespace extensions
