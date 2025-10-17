// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_backend.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

using Transaction = TabStateStorageDatabase::Transaction;
using TransactionCallback = base::OnceCallback<bool(Transaction*)>;

namespace {
constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

// Runs a set of database `operations` in a transaction.
bool RunTransaction(TabStateStorageDatabase* db,
                    TransactionCallback operations) {
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

// Batches several operations into a single callback by calling each one
// sequentially. The returned callback returns false and stops execution if any
// batched callback fails.
TransactionCallback BatchOperations(
    std::vector<TransactionCallback> operations) {
  return base::BindOnce(
      [](std::vector<TransactionCallback> ops,
         Transaction* transaction) -> bool {
        for (auto& op : ops) {
          if (!std::move(op).Run(transaction)) {
            return false;
          }
        }
        return true;
      },
      std::move(operations));
}

// Saves a Node to the database.
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

// Updates the children of a Node from the database.
bool SaveChildrenSequence(TabStateStorageDatabase* db,
                          int id,
                          std::unique_ptr<Payload> children,
                          Transaction* transaction) {
  std::string serialized = children->SerializePayload();
  bool success = db->SaveNodeChildren(transaction, id, std::move(serialized));
  if (!success) {
    DLOG(ERROR) << "Could not perform save node children operation.";
  }
  return true;
}

// Removes a Node from the database.
bool RemoveNodeSequence(TabStateStorageDatabase* db,
                        int id,
                        Transaction* transaction) {
  bool success = db->RemoveNode(transaction, id);
  if (!success) {
    DLOG(ERROR) << "Could not perform remove node operation.";
  }
  return true;
}

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

void TabStateStorageBackend::Save(int id,
                                  TabStorageType type,
                                  std::unique_ptr<StoragePackage> package) {
  TransactionCallback save_sequence =
      base::BindOnce(&SaveNodeSequence, base::Unretained(database_.get()), id,
                     type, std::move(package));
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunTransaction, base::Unretained(database_.get()),
                     std::move(save_sequence)),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::SaveChildren(int id,
                                          std::unique_ptr<Payload> children) {
  TransactionCallback save_sequence =
      base::BindOnce(&SaveChildrenSequence, base::Unretained(database_.get()),
                     id, std::move(children));
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunTransaction, base::Unretained(database_.get()),
                     std::move(save_sequence)),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::RemoveNode(int id) {
  TransactionCallback remove_node_sequence = base::BindOnce(
      &RemoveNodeSequence, base::Unretained(database_.get()), id);

  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunTransaction, base::Unretained(database_.get()),
                     std::move(remove_node_sequence)),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::RemoveNodeAndUpdateParent(
    int id,
    int parent_id,
    std::unique_ptr<Payload> children) {
  TransactionCallback save_children_sequence =
      base::BindOnce(&SaveChildrenSequence, base::Unretained(database_.get()),
                     id, std::move(children));
  TransactionCallback remove_node_sequence = base::BindOnce(
      &RemoveNodeSequence, base::Unretained(database_.get()), id);

  std::vector<TransactionCallback> callbacks(2);
  callbacks.push_back(std::move(save_children_sequence));
  callbacks.push_back(std::move(remove_node_sequence));
  auto callback_combined = BatchOperations(std::move(callbacks));

  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunTransaction, base::Unretained(database_.get()),
                     std::move(callback_combined)),
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
