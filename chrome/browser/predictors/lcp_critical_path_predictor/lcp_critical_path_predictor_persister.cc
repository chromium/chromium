// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_persister.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace {

std::unique_ptr<LCPCriticalPathPredictorPersister> CreatePersister(
    std::unique_ptr<LCPCriticalPathPredictorDatabase> database) {
  return std::make_unique<LCPCriticalPathPredictorPersister>(
      std::move(database));
}

}  // namespace

LCPCriticalPathPredictorPersister::LCPCriticalPathPredictorPersister(
    std::unique_ptr<LCPCriticalPathPredictorDatabase> database)
    : database_(std::move(database)) {}

LCPCriticalPathPredictorPersister::~LCPCriticalPathPredictorPersister() =
    default;

void LCPCriticalPathPredictorPersister::CreateForFilePath(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const base::FilePath& path,
    base::TimeDelta flush_delay_for_writes,
    base::OnceCallback<void(std::unique_ptr<LCPCriticalPathPredictorPersister>)>
        on_done_initializing) {
  LCPCriticalPathPredictorDatabase::Create(
      /*db_opener=*/base::BindOnce(
          [](const base::FilePath& path, sql::Database* db) {
            const base::FilePath directory = path.DirName();
            if (!base::PathExists(directory) &&
                !base::CreateDirectory(directory)) {
              return false;
            }
            return db->Open(path);
          },
          path),
      db_task_runner, flush_delay_for_writes,
      base::BindOnce(&CreatePersister).Then(std::move(on_done_initializing)));
}

absl::optional<LCPElement> LCPCriticalPathPredictorPersister::GetLCPElement(
    const GURL& page_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sqlite_proto::KeyValueData<LCPElement>* data = database_->LCPElementData();
  CHECK(data);
  LCPElement ret;
  if (!data->TryGetData(page_url.spec(), &ret)) {
    return absl::nullopt;
  }
  return ret;
}

void LCPCriticalPathPredictorPersister::SetLCPElement(
    const GURL& page_url,
    const LCPElement& lcp_element) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sqlite_proto::KeyValueData<LCPElement>* data = database_->LCPElementData();
  CHECK(data);
  data->UpdateData(page_url.spec(), lcp_element);
}
