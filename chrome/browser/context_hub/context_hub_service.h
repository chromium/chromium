// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_observer.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/types/id_type.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/auto_todos/auto_todos_store.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/smart_search.pb.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif

class Profile;

#if !BUILDFLAG(IS_ANDROID)
class BrowserTabStripTracker;
class BrowserWindowInterface;
class TabStripModel;
class TabStripModelChange;
struct TabStripSelectionChange;
#endif

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {
class ModelQualityLogEntry;
class RemoteModelExecutor;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace page_content_annotations {
class PageContentExtractionService;
}  // namespace page_content_annotations

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace context_hub {

class TabGroupStore;
class ContextHubBackend;

class ContextHubService : public KeyedService,
                          public AutoTodosStore::Observer,
                          public signin::IdentityManager::Observer,
                          public base::PowerSuspendObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
                          public BrowserTabStripTrackerDelegate,
                          public TabStripModelObserver
#endif
{
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAutoTodosChanged(base::span<const AutoTodoEntry> entries) {}
    virtual void OnFirstPartyAutoTodosGenerationStateChanged(
        bool is_generating) {}
    virtual void OnThirdPartyAutoTodosGenerationStateChanged(
        bool is_generating) {}
  };

  ContextHubService(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      personal_context::PersonalContextService* personal_context_service,
      optimization_guide::RemoteModelExecutor*
          optimization_guide_remote_model_executor,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service,
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

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokensLoaded() override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  // base::PowerSuspendObserver:
  void OnResume() override;

#if !BUILDFLAG(IS_ANDROID)
  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
#endif

  // Generates 1P AutoTodos and saves them in the AutoTodos store. Invokes
  // `callback` on completion indicating whether the generation was successful.
  void GenerateFirstPartyAutoTodos(AutoTodosStore::OperationCallback callback);

  // Returns the timestamp when First Party Auto Todos were last generated.
  base::Time GetLastFirstPartyGenerationTime() const;

  // Returns the timestamp when Third Party Auto Todos were last generated.
  base::Time GetLastThirdPartyGenerationTime() const;

  // Generates tab-based todos and saves them in the AutoTodos store. Invokes
  // `callback` on completion indicating whether the generation was successful.
  void GenerateTabBasedTodos(
      std::vector<base::WeakPtr<content::WebContents>> tabs,
      AutoTodosStore::OperationCallback callback);

  // Returns true if a First Party Auto Todos generation request is in flight.
  bool IsGeneratingFirstPartyAutoTodos() const;

  using GetAutoTodosCallback =
      base::OnceCallback<void(std::vector<AutoTodoEntry>)>;
  // Returns all stored AutoTodos.
  void GetAutoTodos(GetAutoTodosCallback callback) const;

  // Updates a todo item in the AutoTodos store. Designed to be called with a
  // single complete todo item from the UI.
  void UpdateAutoTodo(AutoTodoEntry item,
                      AutoTodosStore::OperationCallback callback);

  // Deletes a third-party todo item matching the given tab ID from the
  // AutoTodos store.
  void DeleteAutoTodoByTabId(int64_t tab_id,
                             AutoTodosStore::OperationCallback callback);

  // Clears all 1P Auto Todos from the AutoTodos store.
  void ClearFirstPartyAutoTodos(AutoTodosStore::OperationCallback callback);

  // Clears all 3P Auto Todos from the AutoTodos store.
  void ClearThirdPartyAutoTodos(AutoTodosStore::OperationCallback callback);

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
                              std::vector<TabData> ungrouped_tabs,
                              std::string text_response)>;
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

  // Adds a memory bank chat history turn to the cache.
  void AddMemoryBankChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::Role role,
      std::string_view message_content);
  // Returns all memory bank chat history turns stored in the LRU cache in
  // chronological order (oldest to newest).
  std::vector<optimization_guide::proto::ChatHistoryTurn>
  GetMemoryBankChatHistory() const;
  // Clears all memory bank chat history turns from the LRU cache.
  void ClearMemoryBankChatHistory();

  // Sets the pending memory bank entry waiting to be saved by the user.
  void SetPendingMemoryBankEntry(MemoryBankEntry entry);

  // Retrieves the current pending memory bank entry, if present.
  std::optional<MemoryBankEntry> GetPendingMemoryBankEntry() const;

  // Commits the current pending memory bank entry to the memory bank with the
  // provided tags, note, and collection, and clears the pending entry.
  bool SavePendingMemoryBankEntry(
      std::vector<std::string> tags = {},
      std::optional<std::string> note = std::nullopt,
      std::optional<std::string> collection = std::nullopt);

  // Memory bank wrappers that forward operations to the underlying storage
  // backend.
  // Saves an entry in the memory bank.
  void SaveMemoryBankEntry(MemoryBankEntry entry,
                           MemoryBank::OperationCompleteCallback callback);
  // Updates an entry in the memory bank with new tags, note, and collection.
  void UpdateMemoryBankEntryAnnotations(
      int64_t id,
      std::vector<std::string> tags,
      std::optional<std::string> note,
      std::optional<std::string> collection,
      MemoryBank::OperationCompleteCallback callback);
  // Deletes an entry from the memory bank.
  void DeleteEntries(base::span<const int64_t> ids,
                     MemoryBank::OperationCompleteCallback callback);
  // Returns all entries from the memory bank.
  void GetAllEntries(MemoryBank::GetEntriesCallback callback) const;
  // Returns entries for the given IDs from the memory bank.
  void GetEntriesByIds(base::span<const int64_t> ids,
                       MemoryBank::GetEntriesCallback callback) const;
  // Returns all unique tags from the memory bank.
  void GetAllMemoryBankTags(MemoryBank::GetStringsCallback callback) const;
  // Returns all unique collections from the memory bank.
  void GetAllMemoryBankCollections(
      MemoryBank::GetStringsCallback callback) const;

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

  using SmartSearchCallback = base::OnceCallback<void(
      const std::vector<personal_context::proto::SmartSearchItem>& results)>;
  // Executes the provided natural language query to search across Drive
  // artifacts.
  void ExecuteSmartSearch(const std::string& query,
                          SmartSearchCallback callback);

  using ConfirmAllTabGroupsCallback =
      base::OnceCallback<void(bool success,
                              std::vector<base::Uuid> added_group_guids)>;
  // Commits all unconfirmed tab groups to Chrome's native TabGroupSyncService
  // as confirmed groups and clears in-memory storage.
  void ConfirmAllTabGroups(ConfirmAllTabGroupsCallback callback);
  // Returns all confirmed tab groups for the current profile.
  std::vector<TabGroupEntry> GetConfirmedTabGroups() const;
  // Returns the confirmed tab group for the given group_guid.
  std::optional<TabGroupEntry> GetConfirmedTabGroup(
      const base::Uuid& group_guid) const;
  // Returns the local tab group ID for the confirmed group with the given
  // group_guid.
  std::optional<tab_groups::LocalTabGroupID> GetLocalGroupIdForConfirmedGroup(
      const base::Uuid& group_guid) const;
  // Removes the confirmed tab group with the specified group_guid.
  // Returns true if the group was found and removed, false otherwise.
  bool RemoveConfirmedTabGroup(const base::Uuid& group_guid);
  // Removes all confirmed tab groups.
  bool RemoveAllConfirmedTabGroups();
  // Connects a local tab group to the confirmed tab group with group_guid.
  void ConnectLocalTabGroup(const base::Uuid& group_guid,
                            const tab_groups::LocalTabGroupID& local_id);

  base::WeakPtr<ContextHubService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Attempts to generate 1P AutoTodos if eligible and not already in flight.
  void MaybeTriggerFirstPartyAutoTodosGeneration();

  // Triggered periodically by `first_party_auto_todos_timer_` to generate 1P
  // AutoTodos.
  void OnFirstPartyAutoTodosTimerTriggered();

  // Adds a TabGroupEntry to TabGroupSyncService and returns its SavedTabGroup
  // GUID if successful, or std::nullopt if conversion failed.
  std::optional<base::Uuid> AddTabGroupToSyncService(
      const TabGroupEntry& entry);

  // Adds multiple TabGroupEntries to TabGroupSyncService and returns the added
  // GUIDs.
  std::vector<base::Uuid> AddTabGroupsToSyncService(
      base::span<const TabGroupEntry> entries);

  // Callback invoked when all tab groups are fetched from the store to confirm
  // them into TabGroupSyncService.
  void OnAllTabGroupsFetchedForConfirmation(
      ConfirmAllTabGroupsCallback callback,
      std::vector<TabGroupEntry> groups);

  // Callback invoked when memory bank entries are fetched for a chat request.
  void OnMemoryBankEntriesFetched(const std::string& user_command,
                                  MemoryBankChatCallback callback,
                                  std::vector<MemoryBankEntry> entries);

  // Generates tab groups based on the provided `tabs` and invokes `callback`
  // with the resulting groups and any ungrouped tabs.
  void GenerateTabGroups(std::vector<TabData> tabs,
                         const std::string& user_command,
                         GroupTabsCallback callback);

  // Handles the async response when all auto todos are fetched to populate
  // existing first party todos in the CMS request.
  void OnCachedFirstPartyAutoTodosFetched(
      std::vector<AutoTodoEntry> stored_todos);

  // Handles the async response from the AutoTodos fetch.
  void OnFirstPartyAutoTodosFetched(
      personal_context::FetchContextResult result);

  // Handles the async response from the SmartSearch fetch.
  void OnSmartSearchFetched(SmartSearchCallback callback,
                            personal_context::FetchContextResult result);

  // Cleans up First Party Auto Todos generation state, notifies observers, and
  // invokes any pending completion callbacks.
  void FinishFirstPartyAutoTodosGeneration(bool success);

  // Handles the async response when all auto todos are fetched to filter tabs
  // for tab-based todos generation.
  void OnAllAutoTodosFetchedForTabBasedTodos(
      std::vector<base::WeakPtr<content::WebContents>> tabs,
      AutoTodosStore::OperationCallback callback,
      std::vector<AutoTodoEntry> stored_todos);

  // Handles the async response when APC is fetched for tabs.
  void OnTabContextsFetched(
      std::vector<
          std::pair<TabData,
                    std::optional<optimization_guide::proto::PageContext>>>
          tab_contexts);

  // Dispatches pending tab-based todos MES requests up to the concurrency
  // limit.
  void ProcessNextTabBasedTodosMesBatch();

  // Handles a single MES response for a tab in tab-based todos generation.
  void OnTabBasedTodosMesResponseReceived(
      int64_t tab_id,
      base::Time last_active_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Cleans up tab-based todos generation state and invokes the completion
  // callback.
  void FinishTabBasedTodosGeneration(bool success);

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

  const raw_ref<Profile> profile_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<optimization_guide::RemoteModelExecutor>
      optimization_guide_remote_model_executor_;
  const raw_ref<tab_groups::TabGroupSyncService>
      tab_group_sync_service_;
  const raw_ref<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_;

  // Indicates if a First Party Auto Todos generation request is in flight.
  bool is_generating_first_party_auto_todos_ = false;

  // Stores client callbacks waiting for completion of an in-flight
  // `GenerateFirstPartyAutoTodos` request. This allows concurrent requests
  // (e.g. a user click during a background timer fetch) to attach to the
  // in-flight request rather than immediately failing with false.
  std::vector<AutoTodosStore::OperationCallback> pending_first_party_callbacks_;

  // Stores the client's callback during an in-flight `GenerateTabBasedTodos`
  // request while page contexts are being extracted and model execution is
  // pending. Also serves to prevent concurrent tab-based todo generation
  // requests.
  // TODO(crbug.com/543605762): Consider adding a timeout timer to ensure this
  // callback is not held indefinitely if page content extraction stalls.
  AutoTodosStore::OperationCallback pending_tab_todos_callback_;

  // Number of concurrent Model Execution Service (MES) requests currently in
  // flight for tab-based todos generation.
  int active_tab_todos_requests_ = 0;

  // Queue of candidate tabs and their extracted page contexts waiting to be
  // dispatched for model execution.
  std::queue<std::pair<TabData, optimization_guide::proto::PageContext>>
      pending_tab_todos_requests_;

  // Accumulates generated tab-based todos from completed MES requests during an
  // in-flight generation session before batch-saving them to the store.
  std::vector<AutoTodoEntry> generated_tab_todos_;

  using TabGroupChatHistoryTurnId =
      base::IdType64<class TabGroupChatHistoryTurnIdTag>;
  TabGroupChatHistoryTurnId::Generator
      tab_group_chat_history_turn_id_generator_;
  base::LRUCache<TabGroupChatHistoryTurnId,
                 optimization_guide::proto::ChatHistoryTurn>
      tab_group_chat_history_cache_;

  using MemoryBankChatHistoryTurnId =
      base::IdType64<class MemoryBankChatHistoryTurnIdTag>;
  MemoryBankChatHistoryTurnId::Generator
      memory_bank_chat_history_turn_id_generator_;
  base::LRUCache<MemoryBankChatHistoryTurnId,
                 optimization_guide::proto::ChatHistoryTurn>
      memory_bank_chat_history_cache_;

  // In-memory storage for feedback on Auto Todo items. The key is the ID of the
  // Auto Todo item in question and the value is whether the item was liked or
  // disliked by the user. This cache is to gather teamfood feedback only.
  base::LRUCache<std::string, bool> todo_feedback_cache_;

  // Single pending memory bank entry waiting to be saved by the active WebUI
  // dialog.
  std::optional<MemoryBankEntry> pending_memory_bank_entry_;

  // Backend storage engine for SQLite operations. May be null if DB storage is
  // disabled.
  std::unique_ptr<ContextHubBackend> context_hub_backend_;

  // Guaranteed to be non-null. If features::kMemoryBanks is disabled, this
  // will be a NoOpMemoryBank.
  std::unique_ptr<MemoryBank> memory_bank_;

  std::unique_ptr<TabGroupStore> tab_group_store_;

  std::unique_ptr<AutoTodosStore> auto_todos_store_;

  // Timestamp of the most recent successful First Party Auto Todos generation
  // during the current browser session.
  base::Time last_first_party_generation_time_;

  // Timestamp of the most recent successful Third Party Auto Todos generation
  // during the current browser session.
  base::Time last_third_party_generation_time_;

  // Periodic timer that generates and stores 1P AutoTodos during the session.
  base::RepeatingTimer first_party_auto_todos_timer_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;
#endif

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ContextHubService> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_H_
