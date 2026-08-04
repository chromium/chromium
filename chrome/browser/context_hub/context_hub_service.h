// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/types/id_type.h"
#include "chrome/browser/context_hub/auto_todos/auto_todos_store.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/personal_context_types.h"
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

class TabGroupStore;
class ContextHubBackend;

class ContextHubService : public KeyedService, public AutoTodosStore::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAutoTodosChanged(base::span<const AutoTodoEntry> entries) {}
  };

  ContextHubService(
      personal_context::PersonalContextService* personal_context_service,
      optimization_guide::RemoteModelExecutor*
          optimization_guide_remote_model_executor,
      std::unique_ptr<MemoryBank> memory_bank,
      std::unique_ptr<TabGroupStore> tab_group_store,
      std::unique_ptr<ContextHubBackend> context_hub_backend,
      std::unique_ptr<AutoTodosStore> auto_todos_store);

  ContextHubService(const ContextHubService&) = delete;
  ContextHubService& operator=(const ContextHubService&) = delete;
  ~ContextHubService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // AutoTodosStore::Observer:
  void OnAutoTodosChanged(base::span<const AutoTodoEntry> entries) override;

  // TODO(crbug.com/540562062): Receive updates via observer notifications
  // rather than on generation.
  using AutoTodosCallback = base::OnceCallback<void(
      const std::optional<std::vector<AutoTodoEntry>>&)>;

  // Generates 1P AutoTodos and saves them in the AutoTodos store. Invokes
  // `callback` on completion with the response if successful, or std::nullopt.
  void GenerateFirstPartyAutoTodos(AutoTodosCallback callback);

  // Stores or updates a todo feedback item in the in-memory cache.
  void SetTodoFeedback(
      browser::context_hub::mojom::AutoTodoItemFeedbackPtr feedback);
  // Deletes a todo feedback item by id from the in-memory cache.
  void DeleteTodoFeedback(const std::string& id);
  // Clears all todo feedback items from the in-memory cache.
  void ClearTodoFeedbacks();
  // Returns all cached todo feedback items.
  std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr>
  GetTodoFeedbacks() const;

  using GroupTabsCallback =
      base::OnceCallback<void(std::vector<TabGroupEntry> groups,
                              std::vector<TabData> ungrouped_tabs)>;
  // Groups tabs based on the provided `tabs` list.
  void GroupTabs(std::vector<TabData> tabs,
                 const std::string& user_command,
                 GroupTabsCallback callback);

  // Adds a tab group chat history turn to the cache.
  void AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::Role role,
      std::string_view message_content);
  // Returns all tab group chat history turns stored in the LRU cache in
  // chronological order (oldest to newest).
  std::vector<optimization_guide::proto::ChatHistoryTurn>
  GetTabGroupChatHistory() const;
  // Clears all tab group chat history turns from the LRU cache.
  void ClearTabGroupChatHistory();

  // Memory bank wrappers that forward operations to the underlying storage
  // backend.
  // Saves a tab to the memory bank.
  void SaveTab(const GURL& url,
               std::string_view tab_title,
               std::string_view page_text,
               MemoryBank::OperationCompleteCallback callback);
  // Saves a text selection to the memory bank.
  void SaveTextSelection(const GURL& url,
                         std::string_view tab_title,
                         std::string_view selected_text,
                         MemoryBank::OperationCompleteCallback callback);
  // Deletes an entry from the memory bank.
  void DeleteEntries(base::span<const int64_t> ids,
                     MemoryBank::OperationCompleteCallback callback);
  // Returns all entries from the memory bank.
  void GetAllEntries(MemoryBank::GetEntriesCallback callback) const;
  // Returns entries for the given IDs from the memory bank.
  void GetEntriesByIds(base::span<const int64_t> ids,
                       MemoryBank::GetEntriesCallback callback) const;

  using GetTabGroupsCallback =
      base::OnceCallback<void(std::vector<TabGroupEntry>)>;
  // Returns all stored tab groups.
  void GetTabGroups(GetTabGroupsCallback callback) const;
  // Deletes all stored tab groups.
  void DeleteAllTabGroups(base::OnceClosure callback);

  using MemoryBankChatCallback =
      base::OnceCallback<void(std::optional<std::string> response)>;
  // Executes a memory bank chat request for the specified memory bank entry
  // IDs.
  void ExecuteMemoryBankChat(base::span<const int64_t> entry_ids,
                             const std::string& user_command,
                             MemoryBankChatCallback callback);

  base::WeakPtr<ContextHubService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Callback invoked when memory bank entries are fetched for a chat request.
  void OnMemoryBankEntriesFetched(const std::string& user_command,
                                  MemoryBankChatCallback callback,
                                  std::vector<MemoryBankEntry> entries);

  // Generates tab groups based on the provided `tabs` and invokes `callback`
  // with the resulting groups and any ungrouped tabs.
  void GenerateTabGroups(std::vector<TabData> tabs,
                         const std::string& user_command,
                         GroupTabsCallback callback);

  // Handles the async response from the AutoTodos fetch.
  void OnFirstPartyAutoTodosFetched(
      AutoTodosCallback callback,
      personal_context::FetchContextResult result);

  // Handles the result of the model execution from `GenerateTabGroups`.
  void HandleTabGroupModelExecutionResult(
      std::vector<TabData> tabs,
      GroupTabsCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Handles the result of the model execution from `ExecuteMemoryBankChat`.
  void HandleMemoryBankChatModelExecutionResult(
      MemoryBankChatCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<optimization_guide::RemoteModelExecutor>
      optimization_guide_remote_model_executor_;

  using TabGroupChatHistoryTurnId =
      base::IdType64<class TabGroupChatHistoryTurnIdTag>;
  base::LRUCache<TabGroupChatHistoryTurnId,
                 optimization_guide::proto::ChatHistoryTurn>
      tab_group_chat_history_cache_;

  // In-memory storage for feedback on Auto Todo items. The key is the ID of the
  // Auto Todo item in question and the value is whether the item was liked or
  // disliked by the user. This cache is to gather teamfood feedback only.
  base::LRUCache<std::string, bool> todo_feedback_cache_;

  // Backend storage engine for SQLite operations. May be null if DB storage is
  // disabled.
  std::unique_ptr<ContextHubBackend> context_hub_backend_;

  // Guaranteed to be non-null. If features::kMemoryBanks is disabled, this
  // will be a NoOpMemoryBank.
  std::unique_ptr<MemoryBank> memory_bank_;

  std::unique_ptr<TabGroupStore> tab_group_store_;

  std::unique_ptr<AutoTodosStore> auto_todos_store_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ContextHubService> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
