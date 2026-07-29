// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_BACKEND_IMPL_H_
#define CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_BACKEND_IMPL_H_

#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"

namespace context_hub {

class ContextHubDatabase;

class ContextHubBackendImpl : public ContextHubBackend {
 public:
  explicit ContextHubBackendImpl(const base::FilePath& db_path);
  ContextHubBackendImpl(const ContextHubBackendImpl&) = delete;
  ContextHubBackendImpl& operator=(const ContextHubBackendImpl&) = delete;
  ~ContextHubBackendImpl() override;

  // ContextHubBackend implementation.
  void AddOrUpdateMemoryBankEntry(MemoryBankEntry entry,
                                  OperationCompleteCallback callback) override;
  void DeleteMemoryBankEntries(base::span<const int64_t> ids,
                               OperationCompleteCallback callback) override;
  void GetAllMemoryBankEntries(GetEntriesCallback callback) const override;
  void GetMemoryBankEntriesByIds(
      base::span<const int64_t> ids,
      GetEntriesCallback callback) const override;

 private:
  enum class DbState {
    kUninitialized,
    kInitializing,
    kReady,
    kFailed,
  };

  void OnInitDatabase(const base::FilePath& db_path);
  void OnDatabaseInitialized(bool success);

  const scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::SequenceBound<ContextHubDatabase> db_;

  DbState db_state_ = DbState::kUninitialized;
  std::vector<base::OnceClosure> queued_operations_;

  base::WeakPtrFactory<ContextHubBackendImpl> weak_ptr_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_BACKEND_IMPL_H_
