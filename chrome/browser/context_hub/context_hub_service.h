// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace context_hub {

class ContextHubService : public KeyedService {
 public:
  explicit ContextHubService(
      personal_context::PersonalContextService* personal_context_service,
      std::unique_ptr<MemoryBank> memory_bank);

  ContextHubService(const ContextHubService&) = delete;
  ContextHubService& operator=(const ContextHubService&) = delete;
  ~ContextHubService() override;

  using AutoTodosCallback = base::OnceCallback<void(
      std::optional<personal_context::proto::AutoTodosResponse>)>;

  // Generates auto-todos and invokes `callback` on completion, whether it's
  // successful or not.
  void GenerateAutoTodos(AutoTodosCallback callback);

  // Memory bank wrappers that forward operations to the underlying storage
  // backend.
  // Saves a tab to the memory bank.
  void SaveTab(const GURL& url,
               const std::string& tab_title,
               const std::string& page_text,
               MemoryBank::OperationCompleteCallback callback);
  // Saves a text selection to the memory bank.
  void SaveTextSelection(const GURL& url,
                         const std::string& tab_title,
                         const std::string& selected_text,
                         MemoryBank::OperationCompleteCallback callback);
  // Deletes an entry from the memory bank.
  void DeleteEntries(base::span<const int64_t> ids,
                     MemoryBank::OperationCompleteCallback callback);
  // Returns all entries from the memory bank.
  void GetAllEntries(MemoryBank::GetAllEntriesCallback callback) const;

 private:
  // Handles the async response from the AutoTodos fetch.
  void OnAutoTodosFetched(AutoTodosCallback callback,
                          personal_context::FetchContextResult result);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;

  // Guaranteed to be non-null. If features::kMemoryBanks is disabled, this
  // will be a NoOpMemoryBank.
  std::unique_ptr<MemoryBank> memory_bank_;

  base::WeakPtrFactory<ContextHubService> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
