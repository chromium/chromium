// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_table_base.h"

#include "base/updateable_sequenced_task_runner.h"

namespace media_history {

base::UpdateableSequencedTaskRunner* MediaHistoryTableBase::GetTaskRunner() {
  return db_task_runner_.get();
}

MediaHistoryTableBase::MediaHistoryTableBase(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : db_task_runner_(std::move(db_task_runner)), db_(nullptr) {}

MediaHistoryTableBase::~MediaHistoryTableBase() = default;

sql::InitStatus MediaHistoryTableBase::Initialize(sql::Database* db) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(db);

  db_ = db;
  return CreateTableIfNonExistent();
}

sql::Database* MediaHistoryTableBase::DB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_;
}

void MediaHistoryTableBase::ResetDB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = nullptr;
}

bool MediaHistoryTableBase::CanAccessDatabase() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_;
}

}  // namespace media_history
