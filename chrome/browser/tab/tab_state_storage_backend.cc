// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_backend.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

namespace {
constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};
}  // namespace

TabStateStorageBackend::TabStateStorageBackend(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      db_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kDBTaskTraits)) {}

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

void TabStateStorageBackend::SaveTabState(int id,
                                          tabs_pb::TabState tab_state) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::SaveTabState,
                     base::Unretained(database_.get()), id,
                     std::move(tab_state)),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::LoadAllTabStates(
    base::OnceCallback<void(std::vector<tabs_pb::TabState>)> callback) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::LoadAllTabStates,
                     base::Unretained(database_.get())),
      base::BindOnce(&TabStateStorageBackend::OnAllTabsRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageBackend::OnDBReady(bool success) {}

void TabStateStorageBackend::OnWrite(bool success) {}

void TabStateStorageBackend::OnAllTabsRead(
    base::OnceCallback<void(std::vector<tabs_pb::TabState>)> callback,
    std::vector<tabs_pb::TabState> result) {
  std::move(callback).Run(std::move(result));
}

}  // namespace tabs
