// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_database_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace ash {

namespace {

using EntryVector =
    leveldb_proto::ProtoDatabase<printing::proto::PrintJobInfo>::KeyEntryVector;

const base::FilePath::CharType kPrintJobDatabaseName[] =
    FILE_PATH_LITERAL("PrintJobDatabase");

const int kMaxInitializeAttempts = 3;

}  // namespace

PrintJobDatabaseImpl::PrintJobDatabaseImpl(
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    base::FilePath profile_path)
    : init_status_(InitStatus::UNINITIALIZED) {
  auto print_job_database_path = profile_path.Append(kPrintJobDatabaseName);

  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  database_ = database_provider->GetDB<printing::proto::PrintJobInfo>(
      leveldb_proto::ProtoDbType::PRINT_JOB_DATABASE, print_job_database_path,
      database_task_runner);
}

PrintJobDatabaseImpl::~PrintJobDatabaseImpl() = default;

void PrintJobDatabaseImpl::Initialize(InitializeCallback callback) {
  if (init_status_ == InitStatus::PENDING)
    return;
  DCHECK_EQ(init_status_, InitStatus::UNINITIALIZED);
  init_status_ = InitStatus::PENDING;
  database_->Init(base::BindOnce(&PrintJobDatabaseImpl::OnInitialized,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback)));
}

bool PrintJobDatabaseImpl::IsInitialized() {
  return init_status_ == InitStatus::INITIALIZED;
}

void PrintJobDatabaseImpl::SavePrintJob(
    const printing::proto::PrintJobInfo& print_job_info,
    SavePrintJobCallback callback) {
  if (init_status_ == InitStatus::FAILED) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Defer execution if database is uninitialized.
  if (init_status_ != InitStatus::INITIALIZED) {
    deferred_callbacks_.push(base::BindOnce(
        &PrintJobDatabaseImpl::SavePrintJob, weak_ptr_factory_.GetWeakPtr(),
        print_job_info, std::move(callback)));
    return;
  }

  cache_[print_job_info.id()] = print_job_info;

  auto entries_to_save = std::make_unique<EntryVector>();
  entries_to_save->push_back(
      std::make_pair(print_job_info.id(), print_job_info));
  database_->UpdateEntries(
      /*entries_to_save=*/std::move(entries_to_save),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobSaved,
                     weak_ptr_factory_.GetWeakPtr(), print_job_info,
                     std::move(callback)));
}

void PrintJobDatabaseImpl::DeletePrintJobs(const std::vector<std::string>& ids,
                                           DeletePrintJobsCallback callback) {
  if (init_status_ == InitStatus::FAILED) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Defer execution if database is uninitialized.
  if (init_status_ != InitStatus::INITIALIZED) {
    deferred_callbacks_.push(base::BindOnce(
        &PrintJobDatabaseImpl::DeletePrintJobs, weak_ptr_factory_.GetWeakPtr(),
        ids, std::move(callback)));
    return;
  }

  database_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<EntryVector>(),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(ids),
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobsDeleted,
                     weak_ptr_factory_.GetWeakPtr(), ids, std::move(callback)));
}

void PrintJobDatabaseImpl::Clear(DeletePrintJobsCallback callback) {
  if (init_status_ == InitStatus::FAILED) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (init_status_ != InitStatus::INITIALIZED) {
    deferred_callbacks_.push(base::BindOnce(&PrintJobDatabaseImpl::Clear,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(callback)));
    return;
  }

  std::vector<std::string> ids;
  ids.reserve(cache_.size());
  for (const auto& pair : cache_)
    ids.push_back(pair.second.id());

  database_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<EntryVector>(),
      /*keys_to_remove=*/
      std::make_unique<std::vector<std::string>>(ids),
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobsDeleted,
                     weak_ptr_factory_.GetWeakPtr(), ids, std::move(callback)));
}

void PrintJobDatabaseImpl::GetPrintJobs(GetPrintJobsCallback callback) {
  if (init_status_ == InitStatus::FAILED) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false,
                       std::vector<printing::proto::PrintJobInfo>()));
    return;
  }

  // Defer execution if database is uninitialized.
  if (init_status_ != InitStatus::INITIALIZED) {
    deferred_callbacks_.push(base::BindOnce(&PrintJobDatabaseImpl::GetPrintJobs,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(callback)));
    return;
  }

  std::vector<printing::proto::PrintJobInfo> entries;
  entries.reserve(cache_.size());
  for (const auto& pair : cache_)
    entries.push_back(pair.second);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, std::move(entries)));
}

void PrintJobDatabaseImpl::OnInitialized(
    InitializeCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  switch (status) {
    case leveldb_proto::Enums::InitStatus::kError:
      if (initialize_attempts_ < kMaxInitializeAttempts) {
        initialize_attempts_++;
        database_->Init(base::BindOnce(&PrintJobDatabaseImpl::OnInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
      } else {
        FinishInitialization(std::move(callback), false);
      }
      break;
    case leveldb_proto::Enums::InitStatus::kOK:
      database_->LoadKeysAndEntries(
          base::BindOnce(&PrintJobDatabaseImpl::OnKeysAndEntriesLoaded,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
    case leveldb_proto::Enums::InitStatus::kInvalidOperation:
    case leveldb_proto::Enums::InitStatus::kNotInitialized:
    case leveldb_proto::Enums::InitStatus::kCorrupt:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void PrintJobDatabaseImpl::OnKeysAndEntriesLoaded(
    InitializeCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, printing::proto::PrintJobInfo>>
        entries) {
  if (success)
    cache_.insert(entries->begin(), entries->end());
  FinishInitialization(std::move(callback), success);
}

void PrintJobDatabaseImpl::FinishInitialization(InitializeCallback callback,
                                                bool success) {
  init_status_ = success ? InitStatus::INITIALIZED : InitStatus::FAILED;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
  // We run deferred callbacks even if initialization failed not to cause
  // possible client-side blocks of next calls to the database.
  while (!deferred_callbacks_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(deferred_callbacks_.front()));
    deferred_callbacks_.pop();
  }
}

void PrintJobDatabaseImpl::OnPrintJobSaved(
    const printing::proto::PrintJobInfo& print_job_info,
    SavePrintJobCallback callback,
    bool success) {
  if (!success)
    cache_.erase(print_job_info.id());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PrintJobDatabaseImpl::OnPrintJobsDeleted(
    const std::vector<std::string>& ids,
    DeletePrintJobsCallback callback,
    bool success) {
  if (success)
    for (const std::string& id : ids)
      cache_.erase(id);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PrintJobDatabaseImpl::GetPrintJobsFromProtoDatabase(
    GetPrintJobsFromProtoDatabaseCallback callback) {
  if (init_status_ == InitStatus::FAILED) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, nullptr));
    return;
  }

  // Defer execution if database is uninitialized.
  if (init_status_ != InitStatus::INITIALIZED) {
    deferred_callbacks_.push(
        base::BindOnce(&PrintJobDatabaseImpl::GetPrintJobsFromProtoDatabase,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  database_->LoadEntries(
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobRetrievedFromDatabase,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobDatabaseImpl::OnPrintJobRetrievedFromDatabase(
    GetPrintJobsFromProtoDatabaseCallback callback,
    bool success,
    std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(entries)));
}

}  // namespace ash
