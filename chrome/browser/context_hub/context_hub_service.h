// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "url/gurl.h"

namespace optimization_guide {
class ModelQualityLogEntry;
class RemoteModelExecutor;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace context_hub {
struct TabData {
  int32_t id;
  std::string title;
  GURL url;
};

struct TabGroupData {
  std::string label;
  std::vector<TabData> tabs;
};

class ContextHubService : public KeyedService {
 public:
  explicit ContextHubService(
      personal_context::PersonalContextService* personal_context_service,
      optimization_guide::RemoteModelExecutor*
          optimization_guide_remote_model_executor,
      std::unique_ptr<MemoryBank> memory_bank);

  ContextHubService(const ContextHubService&) = delete;
  ContextHubService& operator=(const ContextHubService&) = delete;
  ~ContextHubService() override;

  using AutoTodosCallback = base::OnceCallback<void(
      std::optional<personal_context::proto::AutoTodosResponse>)>;

  // Generates auto-todos and invokes `callback` on completion, whether it's
  // successful or not.
  void GenerateAutoTodos(AutoTodosCallback callback);

  using GroupTabsCallback =
      base::OnceCallback<void(std::vector<TabGroupData> groups,
                              std::vector<TabData> ungrouped_tabs)>;
  // Groups tabs based on the provided `tabs` list.
  void GroupTabs(std::vector<TabData> tabs, GroupTabsCallback callback);

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

  // Generates tab groups based on the provided `prompt`.
  void GenerateTabGroups(std::string prompt);

 private:
  // Handles the async response from the AutoTodos fetch.
  void OnAutoTodosFetched(AutoTodosCallback callback,
                          personal_context::FetchContextResult result);

  // Handles the result of the model execution from `GenerateTabGroups`.
  void HandleModelExecutionResult(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<optimization_guide::RemoteModelExecutor>
      optimization_guide_remote_model_executor_;

  // Guaranteed to be non-null. If features::kMemoryBanks is disabled, this
  // will be a NoOpMemoryBank.
  std::unique_ptr<MemoryBank> memory_bank_;

  base::WeakPtrFactory<ContextHubService> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
