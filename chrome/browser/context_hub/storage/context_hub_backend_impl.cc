// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/context_hub_backend_impl.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/storage/context_hub_database.h"

namespace context_hub {

ContextHubBackendImpl::ContextHubBackendImpl(const base::FilePath& db_path)
    : db_task_runner_(base::ThreadPool::CreateSequencedTaskRunnerForResource(
          {
              base::MayBlock(),
              base::TaskPriority::USER_VISIBLE,
              base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
          },
          db_path)),
      db_(db_task_runner_) {
  OnInitDatabase(db_path);
}

ContextHubBackendImpl::~ContextHubBackendImpl() = default;

void ContextHubBackendImpl::OnInitDatabase(const base::FilePath& db_path) {
  db_state_ = DbState::kInitializing;
  db_.AsyncCall(&ContextHubDatabase::Init)
      .WithArgs(db_path)
      .Then(base::BindOnce(&ContextHubBackendImpl::OnDatabaseInitialized,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ContextHubBackendImpl::OnDatabaseInitialized(bool success) {
  db_state_ = success ? DbState::kReady : DbState::kFailed;
  if (!success) {
    // TODO(crbug.com/534420047): Replace this with a non-local histogram
    // once metrics are finalized and setup as needed.
    LOCAL_HISTOGRAM_BOOLEAN("ContextHub.DatabaseInitFailed", true);
  }

  std::vector<base::OnceClosure> ops = std::move(queued_operations_);
  for (auto& op : ops) {
    std::move(op).Run();
  }
}

void ContextHubBackendImpl::AddOrUpdateMemoryBankEntry(
    MemoryBankEntry entry,
    OperationCompleteCallback callback) {
  switch (db_state_) {
    case DbState::kUninitialized:
    case DbState::kInitializing:
      queued_operations_.push_back(
          base::BindOnce(&ContextHubBackendImpl::AddOrUpdateMemoryBankEntry,
                         weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                         std::move(callback)));
      break;
    case DbState::kReady:
      db_.AsyncCall(&ContextHubDatabase::AddOrUpdateMemoryBankEntry)
          .WithArgs(std::move(entry))
          .Then(std::move(callback));
      break;
    case DbState::kFailed:
      std::move(callback).Run(false);
      break;
  }
}

void ContextHubBackendImpl::DeleteMemoryBankEntries(
    base::span<const int64_t> ids,
    OperationCompleteCallback callback) {
  switch (db_state_) {
    case DbState::kUninitialized:
    case DbState::kInitializing:
      queued_operations_.push_back(
          base::BindOnce(&ContextHubBackendImpl::DeleteMemoryBankEntries,
                         weak_ptr_factory_.GetWeakPtr(), base::ToVector(ids),
                         std::move(callback)));
      break;
    case DbState::kReady:
      db_.AsyncCall(&ContextHubDatabase::DeleteMemoryBankEntries)
          .WithArgs(base::ToVector(ids))
          .Then(std::move(callback));
      break;
    case DbState::kFailed:
      std::move(callback).Run(false);
      break;
  }
}

void ContextHubBackendImpl::GetAllMemoryBankEntries(
    GetEntriesCallback callback) const {
  auto* non_const_this = const_cast<ContextHubBackendImpl*>(this);
  switch (db_state_) {
    case DbState::kUninitialized:
    case DbState::kInitializing:
      non_const_this->queued_operations_.push_back(
          base::BindOnce(&ContextHubBackendImpl::GetAllMemoryBankEntries,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
    case DbState::kReady:
      db_.AsyncCall(&ContextHubDatabase::GetAllMemoryBankEntries)
          .Then(std::move(callback));
      break;
    case DbState::kFailed:
      std::move(const_cast<GetEntriesCallback&>(callback)).Run({});
      break;
  }
}

void ContextHubBackendImpl::GetMemoryBankEntriesByIds(
    base::span<const int64_t> ids,
    GetEntriesCallback callback) const {
  auto* non_const_this = const_cast<ContextHubBackendImpl*>(this);
  switch (db_state_) {
    case DbState::kUninitialized:
    case DbState::kInitializing:
      non_const_this->queued_operations_.push_back(
          base::BindOnce(&ContextHubBackendImpl::GetMemoryBankEntriesByIds,
                         weak_ptr_factory_.GetWeakPtr(), base::ToVector(ids),
                         std::move(callback)));
      break;
    case DbState::kReady:
      db_.AsyncCall(&ContextHubDatabase::GetMemoryBankEntriesByIds)
          .WithArgs(base::ToVector(ids))
          .Then(std::move(callback));
      break;
    case DbState::kFailed:
      std::move(const_cast<GetEntriesCallback&>(callback)).Run({});
      break;
  }
}

}  // namespace context_hub
