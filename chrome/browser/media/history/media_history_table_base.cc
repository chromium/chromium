// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_table_base.h"

#include "base/task/updateable_sequenced_task_runner.h"
#include "sql/statement.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace media_history {

base::UpdateableSequencedTaskRunner* MediaHistoryTableBase::GetTaskRunner() {
  return db_task_runner_.get();
}

MediaHistoryTableBase::MediaHistoryTableBase(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : db_task_runner_(std::move(db_task_runner)), db_(nullptr) {}

MediaHistoryTableBase::~MediaHistoryTableBase() = default;

void MediaHistoryTableBase::SetCancelled() {
  cancelled_.Set();
}

sql::InitStatus MediaHistoryTableBase::Initialize(sql::Database* db) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(db);

  db_ = db;

  if (CanAccessDatabase())
    return CreateTableIfNonExistent();
  return sql::InitStatus::INIT_FAILURE;
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
  return !cancelled_.IsSet() && db_;
}

void MediaHistoryTableBase::BindProto(
    sql::Statement& s,
    int col,
    const google::protobuf::MessageLite& protobuf) {
  std::string out;
  CHECK(protobuf.SerializeToString(&out));
  s.BindBlob(col, out);
}

bool MediaHistoryTableBase::GetProto(sql::Statement& s,
                                     int col,
                                     google::protobuf::MessageLite& protobuf) {
  std::string value;
  s.ColumnBlobAsString(col, &value);
  return protobuf.ParseFromString(value);
}

bool MediaHistoryTableBase::DeleteURL(const GURL& url) {
  NOTREACHED();
  return false;
}

}  // namespace media_history
