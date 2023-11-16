// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictor_database.h"

#include <cstdint>
#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "sql/database.h"

using content::BrowserThread;

namespace {

// TODO(shishir): This should move to a more generic name.
const base::FilePath::CharType kPredictorDatabaseName[] =
    FILE_PATH_LITERAL("Network Action Predictor");

}  // namespace

namespace predictors {

// Refcounted as it is created, initialized and destroyed on a different thread
// to the DB sequence provided to the constructor of this class that is required
// for all methods performing database access.
class PredictorDatabaseInternal
    : public base::RefCountedThreadSafe<PredictorDatabaseInternal> {
 public:
  PredictorDatabaseInternal(const PredictorDatabaseInternal&) = delete;
  PredictorDatabaseInternal& operator=(const PredictorDatabaseInternal&) =
      delete;

 private:
  friend class base::RefCountedThreadSafe<PredictorDatabaseInternal>;
  friend class PredictorDatabase;

  explicit PredictorDatabaseInternal(
      Profile* profile,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  virtual ~PredictorDatabaseInternal();

  // Opens the database file from the profile path. Separated from the
  // constructor to ease construction/destruction of this object on one thread
  // but database access on the DB sequence of |db_task_runner_|.
  void Initialize();
  void LogDatabaseStats();  //  DB sequence.

  // Cancels pending DB transactions. Should only be called on the UI thread.
  void SetCancelled();

  bool is_loading_predictor_enabled_;
  base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // TODO(shishir): These tables may not need to be refcounted. Maybe move them
  // to using a WeakPtr instead.
  scoped_refptr<AutocompleteActionPredictorTable> autocomplete_table_;
  scoped_refptr<ResourcePrefetchPredictorTables> resource_prefetch_tables_;
};

PredictorDatabaseInternal::PredictorDatabaseInternal(
    Profile* profile,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : db_path_(profile->GetPath().Append(kPredictorDatabaseName)),
      db_(std::make_unique<sql::Database>(sql::DatabaseOptions{
          .page_size = 4096,
          .cache_size = 500,
          // TODO(pwnall): Add a meta table and remove this option.
          .mmap_alt_status_discouraged = true,
          .enable_views_discouraged = true,  // Required by mmap_alt_status.
      })),
      db_task_runner_(db_task_runner),
      autocomplete_table_(
          new AutocompleteActionPredictorTable(db_task_runner_)),
      resource_prefetch_tables_(
          new ResourcePrefetchPredictorTables(db_task_runner_)) {
  db_->set_histogram_tag("Predictor");

  is_loading_predictor_enabled_ = IsLoadingPredictorEnabled(profile);
}

PredictorDatabaseInternal::~PredictorDatabaseInternal() {
  // The connection pointer needs to be deleted on the DB sequence since there
  // might be a task in progress on the DB sequence which uses this connection.
  db_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

void PredictorDatabaseInternal::Initialize() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  // TODO(tburkard): figure out if we need this.
  //  db_->set_exclusive_locking();
  if (autocomplete_table_->IsCancelled() ||
      resource_prefetch_tables_->IsCancelled()) {
    return;
  }

  bool success = db_->Open(db_path_);
  db_->Preload();

  if (!success)
    return;

  autocomplete_table_->Initialize(db_.get());
  resource_prefetch_tables_->Initialize(db_.get());

  LogDatabaseStats();
}

void PredictorDatabaseInternal::SetCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  autocomplete_table_->SetCancelled();
  resource_prefetch_tables_->SetCancelled();
}

void PredictorDatabaseInternal::LogDatabaseStats() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  autocomplete_table_->LogDatabaseStats();
  if (is_loading_predictor_enabled_)
    resource_prefetch_tables_->LogDatabaseStats();
}

PredictorDatabase::PredictorDatabase(
    Profile* profile,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : db_(new PredictorDatabaseInternal(profile, db_task_runner)) {
  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&PredictorDatabaseInternal::Initialize, db_));
}

PredictorDatabase::~PredictorDatabase() {
}

void PredictorDatabase::Shutdown() {
  db_->SetCancelled();
}

scoped_refptr<AutocompleteActionPredictorTable>
    PredictorDatabase::autocomplete_table() {
  return db_->autocomplete_table_;
}

scoped_refptr<ResourcePrefetchPredictorTables>
    PredictorDatabase::resource_prefetch_tables() {
  return db_->resource_prefetch_tables_;
}

sql::Database* PredictorDatabase::GetDatabase() {
  return db_->db_.get();
}

}  // namespace predictors
