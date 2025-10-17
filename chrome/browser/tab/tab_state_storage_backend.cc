// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_backend.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

using Transaction = TabStateStorageDatabase::Transaction;

namespace {
constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};
}  // namespace

// Runs a set of database `operations` in a transaction.
bool RunTransaction(TabStateStorageDatabase* db,
                    base::OnceCallback<bool(Transaction*)> operations) {
  std::unique_ptr<Transaction> transaction = db->CreateTransaction();
  if (!transaction->Begin()) {
    DLOG(ERROR) << "Could not start transaction.";
    return false;
  }

  if (!std::move(operations).Run(transaction.get())) {
    transaction->Rollback();
    return false;
  }

  if (!transaction->Commit()) {
    DLOG(ERROR) << "Could not commit transaction.";
    return false;
  }

  return true;
}

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

bool SaveNodeSequence(TabStateStorageDatabase* db,
                      int id,
                      TabStorageType type,
                      std::unique_ptr<StoragePackage> package,
                      Transaction* transaction) {
  std::string payload = package->SerializePayload();
  std::string children = package->SerializeChildren();
  bool success = db->SaveNode(transaction, id, type, std::move(payload),
                              std::move(children));
  if (!success) {
    DLOG(ERROR) << "Could not perform save node operation.";
  }
  return true;
}

void TabStateStorageBackend::Save(int id,
                                  TabStorageType type,
                                  std::unique_ptr<StoragePackage> package) {
  base::OnceCallback<bool(Transaction*)> save_sequence =
      base::BindOnce(&SaveNodeSequence, base::Unretained(database_.get()), id,
                     type, std::move(package));
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunTransaction, base::Unretained(database_.get()),
                     std::move(save_sequence)),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SaveChildrenSequence(TabStateStorageDatabase* db,
                          int id,
                          std::unique_ptr<Payload> children,
                          Transaction* transaction) {
  std::string serialized = children->SerializePayload();
  bool success = db->SaveNodeChildren(transaction, id, std::move(serialized));
  if (!success) {
    DLOG(ERROR) << "Could not perform save node operation.";
  }
  return true;
}

void TabStateStorageBackend::SaveChildren(int id,
                                          std::unique_ptr<Payload> children) {
  base::OnceCallback<bool(Transaction*)> save_sequence =
      base::BindOnce(&SaveChildrenSequence, base::Unretained(database_.get()),
                     id, std::move(children));
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunTransaction, base::Unretained(database_.get()),
                     std::move(save_sequence)),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::LoadAllNodes(
    base::OnceCallback<void(std::vector<NodeState>)> callback) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageDatabase::LoadAllNodes,
                     base::Unretained(database_.get())),
      base::BindOnce(&TabStateStorageBackend::OnAllTabsRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageBackend::OnDBReady(bool success) {}

void TabStateStorageBackend::OnWrite(bool success) {}

void TabStateStorageBackend::OnAllTabsRead(
    base::OnceCallback<void(std::vector<NodeState>)> callback,
    std::vector<NodeState> result) {
  std::move(callback).Run(std::move(result));
}

}  // namespace tabs
