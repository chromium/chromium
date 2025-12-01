// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_backend.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"

namespace tabs {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;
using TransactionCallback = base::OnceCallback<bool(OpenTransaction*)>;

namespace {

// MayBlock - DB access may involve disk I/O.
// BEST_EFFORT - Default priority except during critical restore and shutdown
//   tasks.
// BLOCK_SHUTDOWN - Ensure all data is persisted to disk before shutdown. This
//   may not work reliably on Android due to the OS killing the process.
// MUST_USE_FOREGROUND - This reduces the benefit of the BEST_EFFORT, but is
//   required to ensure a priority inversion does not occur when boosting the
//   priority.
constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
    base::ThreadPolicy::MUST_USE_FOREGROUND};

}  // namespace

TabStateStorageBackend::TabStateStorageBackend(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      db_task_runner_(base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          kDBTaskTraits)) {}

TabStateStorageBackend::~TabStateStorageBackend() {
  if (database_) {
    db_task_runner_->DeleteSoon(FROM_HERE, std::move(database_));
  }
}

void TabStateStorageBackend::Initialize() {
  database_ = std::make_unique<TabStateStorageDatabase>(profile_path_);
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::Initialize,
                     base::Unretained(database_.get())),
      base::BindOnce(&TabStateStorageBackend::OnDBReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::BoostPriority() {
  IncrementBoostCounter();
  WaitForAllPendingOperations(
      base::BindOnce(&TabStateStorageBackend::DecrementBoostCounter,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::WaitForAllPendingOperations(
    base::OnceClosure on_idle) {
  db_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                    std::move(on_idle));
}

void TabStateStorageBackend::Update(
    std::unique_ptr<TabStateStorageUpdater> updater) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageUpdater::Execute, std::move(updater),
                     base::Unretained(database_.get())),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::LoadAllNodes(
    const std::string& window_tag,
    bool is_off_the_record,
    std::unique_ptr<StorageLoadedData::Builder> builder,
    OnStorageLoadedData on_storage_loaded_data) {
  IncrementBoostCounter();
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::LoadAllNodes,
                     base::Unretained(database_.get()), window_tag,
                     is_off_the_record, std::move(builder)),
      base::BindOnce(&TabStateStorageBackend::OnLoadDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_storage_loaded_data)));
}

void TabStateStorageBackend::ClearAllNodes() {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TabStateStorageDatabase::ClearAllNodes,
                                base::Unretained(database_.get())));
}

void TabStateStorageBackend::ClearWindow(const std::string& window_tag) {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TabStateStorageDatabase::ClearWindow,
                                base::Unretained(database_.get()), window_tag));
}

void TabStateStorageBackend::OnDBReady(bool success) {}

void TabStateStorageBackend::OnWrite(bool success) {}

void TabStateStorageBackend::IncrementBoostCounter() {
  boosted_priority_count_++;
  if (boosted_priority_count_ == 1) {
    db_task_runner_->UpdatePriority(base::TaskPriority::USER_BLOCKING);
  }
}

void TabStateStorageBackend::DecrementBoostCounter() {
  boosted_priority_count_--;
  if (boosted_priority_count_ == 0) {
    db_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }
}

void TabStateStorageBackend::OnLoadDone(
    OnStorageLoadedData on_storage_loaded_data,
    std::unique_ptr<StorageLoadedData> storage_loaded_data) {
  DecrementBoostCounter();
  std::move(on_storage_loaded_data).Run(std::move(storage_loaded_data));
}

}  // namespace tabs
