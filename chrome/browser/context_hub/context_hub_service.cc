// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/check_deref.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/auto_todos/auto_todos_store.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_store.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"

namespace context_hub {

namespace {

optimization_guide::proto::MemoryBankEntry ToMemoryBankEntryProto(
    const MemoryBankEntry& entry) {
  optimization_guide::proto::MemoryBankEntry mb_proto;
  mb_proto.set_id(entry.id);
  switch (entry.type) {
    case MemoryBankType::kTab:
      mb_proto.set_type(optimization_guide::proto::MEMORY_BANK_TYPE_TAB);
      break;
    case MemoryBankType::kTextSelection:
      mb_proto.set_type(
          optimization_guide::proto::MEMORY_BANK_TYPE_TEXT_SELECTION);
      break;
  }
  mb_proto.set_timestamp_ms(entry.timestamp.InMillisecondsSinceUnixEpoch());
  mb_proto.set_url(entry.url.spec());
  mb_proto.set_tab_title(entry.tab_title);
  if (entry.selected_text.has_value()) {
    mb_proto.set_selected_text(*entry.selected_text);
  }
  for (const auto& tag : entry.tags) {
    mb_proto.add_tags(tag);
  }
  return mb_proto;
}

}  // namespace

ContextHubService::ContextHubService(
    personal_context::PersonalContextService* personal_context_service,
    optimization_guide::RemoteModelExecutor*
        optimization_guide_remote_model_executor,
    std::unique_ptr<MemoryBank> memory_bank,
    std::unique_ptr<TabGroupStore> tab_group_store,
    std::unique_ptr<ContextHubBackend> context_hub_backend,
    std::unique_ptr<AutoTodosStore> auto_todos_store)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      optimization_guide_remote_model_executor_(
          CHECK_DEREF(optimization_guide_remote_model_executor)),
      tab_group_chat_history_cache_(
          features::kMaxTabGroupChatHistoryTurns.Get()),
      todo_feedback_cache_(features::kMaxTodoFeedbackCacheSize.Get()),
      context_hub_backend_(std::move(context_hub_backend)),
      memory_bank_(std::move(memory_bank)),
      tab_group_store_(std::move(tab_group_store)),
      auto_todos_store_(std::move(auto_todos_store)) {
  CHECK(memory_bank_);
  if (auto_todos_store_) {
    auto_todos_store_->AddObserver(this);
  }
}

ContextHubService::~ContextHubService() {
  if (auto_todos_store_) {
    auto_todos_store_->RemoveObserver(this);
  }
}

void ContextHubService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextHubService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextHubService::OnAutoTodosChanged(
    base::span<const AutoTodoEntry> entries) {
  observers_.Notify(&Observer::OnAutoTodosChanged, entries);
}

void ContextHubService::GenerateFirstPartyAutoTodos(
    AutoTodosCallback callback) {
  if (!auto_todos_store_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  personal_context::proto::AutoTodosRequest request_metadata;
  personal_context::ContextMemoryRequestOptions options;
  options.request_timeout = features::kAutoTodosTimeoutSeconds.Get();

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
      request_metadata, options,
      base::BindOnce(&ContextHubService::OnFirstPartyAutoTodosFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextHubService::OnFirstPartyAutoTodosFetched(
    AutoTodosCallback callback,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  personal_context::proto::AutoTodosResponse response;
  if (!response.ParseFromString(result.response.value().value())) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO(crbug.com/540562062): Remove this once state management is handled.
  auto_todos_store_->Clear(base::DoNothing());

  std::vector<AutoTodoEntry> entries;
  entries.reserve(response.todos_size());
  for (const personal_context::proto::AutoTodoItem& todo : response.todos()) {
    AutoTodoEntry entry;
    // TODO(crbug.com/541276677): Remove when the observer is notified of cache
    // changes.
    entry.id = todo.title();
    entry.title = todo.title();
    entry.description = todo.description();
    entry.importance_score = todo.importance_score();
    entry.status = AutoTodoEntry::Status::kActive;

    FirstPartyData first_party;
    first_party.actionable_url = GURL(todo.actionable_url());
    for (const auto& ref : todo.source_references()) {
      if (ref.has_gmail()) {
        first_party.source_references.emplace_back(ref.gmail().message_url());
      }
    }
    entry.data = std::move(first_party);
    entries.push_back(std::move(entry));
  }

  auto_todos_store_->AddAllTodos(entries, base::DoNothing());
  std::move(callback).Run(std::move(entries));
}

void ContextHubService::GetAutoTodos(GetAutoTodosCallback callback) const {
  if (!auto_todos_store_) {
    std::move(callback).Run({});
    return;
  }
  auto_todos_store_->GetAllItems(std::move(callback));
}

void ContextHubService::UpdateAutoTodo(
    AutoTodoEntry item,
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_) {
    std::move(callback).Run(false);
    return;
  }
  auto_todos_store_->AddOrUpdateItem(std::move(item), std::move(callback));
}

void ContextHubService::SetTodoFeedback(
    browser::context_hub::mojom::AutoTodoItemFeedbackPtr feedback) {
  if (!feedback) {
    return;
  }
  todo_feedback_cache_.Put(feedback->todo_id, feedback->liked);
}

void ContextHubService::DeleteTodoFeedback(const std::string& id) {
  auto it = todo_feedback_cache_.Peek(id);
  if (it != todo_feedback_cache_.end()) {
    todo_feedback_cache_.Erase(it);
  }
}

void ContextHubService::ClearTodoFeedbacks() {
  todo_feedback_cache_.Clear();
}

std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr>
ContextHubService::GetTodoFeedbacks() const {
  std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr> feedbacks;
  feedbacks.reserve(todo_feedback_cache_.size());
  for (const auto& [todo_id, liked] : todo_feedback_cache_) {
    auto feedback = browser::context_hub::mojom::AutoTodoItemFeedback::New();
    feedback->todo_id = todo_id;
    feedback->liked = liked;
    feedbacks.push_back(std::move(feedback));
  }
  return feedbacks;
}

void ContextHubService::AddTabGroupChatHistoryTurn(
    optimization_guide::proto::ChatHistoryTurn::Role role,
    std::string_view message_content) {
  optimization_guide::proto::ChatHistoryTurn turn;
  turn.set_role(role);
  turn.set_message_content(message_content);
  turn.set_timestamp_ms(base::Time::Now().InMillisecondsSinceUnixEpoch());
  TabGroupChatHistoryTurnId id =
      TabGroupChatHistoryTurnId::FromUnsafeValue(turn.timestamp_ms());
  tab_group_chat_history_cache_.Put(id, std::move(turn));
}

std::vector<optimization_guide::proto::ChatHistoryTurn>
ContextHubService::GetTabGroupChatHistory() const {
  std::vector<optimization_guide::proto::ChatHistoryTurn> history;
  history.reserve(tab_group_chat_history_cache_.size());
  for (const auto& [id, turn] : base::Reversed(tab_group_chat_history_cache_)) {
    history.push_back(turn);
  }
  return history;
}

void ContextHubService::ClearTabGroupChatHistory() {
  tab_group_chat_history_cache_.Clear();
}

void ContextHubService::SaveTab(
    const GURL& url,
    std::string_view tab_title,
    std::string_view page_text,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->SaveTab(url, tab_title, page_text, std::move(callback));
}

void ContextHubService::SaveTextSelection(
    const GURL& url,
    std::string_view tab_title,
    std::string_view selected_text,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->SaveTextSelection(url, tab_title, selected_text,
                                  std::move(callback));
}

void ContextHubService::DeleteEntries(
    base::span<const int64_t> ids,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->DeleteEntries(ids, std::move(callback));
}

void ContextHubService::GetAllEntries(
    MemoryBank::GetEntriesCallback callback) const {
  memory_bank_->GetAllEntries(std::move(callback));
}

void ContextHubService::GetEntriesByIds(
    base::span<const int64_t> ids,
    MemoryBank::GetEntriesCallback callback) const {
  memory_bank_->GetEntriesByIds(ids, std::move(callback));
}

void ContextHubService::GetTabGroups(GetTabGroupsCallback callback) const {
  if (tab_group_store_) {
    tab_group_store_->GetAllGroups(std::move(callback));
  } else {
    std::move(callback).Run({});
  }
}

void ContextHubService::DeleteAllTabGroups(base::OnceClosure callback) {
  if (tab_group_store_) {
    tab_group_store_->DeleteAllGroups(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

// TODO(crbug.com/531938478): Update to handle APC ingestion.
void ContextHubService::GenerateTabGroups(std::vector<TabData> tabs,
                                          const std::string& user_command,
                                          GroupTabsCallback callback) {
  optimization_guide::proto::ContextHubRequest request;
  request.set_request_type(
      optimization_guide::proto::CONTEXT_HUB_REQUEST_TYPE_GROUPING);
  for (const TabData& tab : tabs) {
    optimization_guide::proto::EntryItem* entry_item =
        request.add_entry_items();
    optimization_guide::proto::Tab* tab_proto = entry_item->mutable_tab();
    tab_proto->set_tab_id(tab.id);
    tab_proto->set_title(tab.title);
    tab_proto->set_url(tab.url.spec());
  }

  for (const auto& turn : GetTabGroupChatHistory()) {
    *request.add_chat_history() = turn;
  }

  std::string_view trimmed_command =
      base::TrimWhitespaceASCII(user_command, base::TRIM_ALL);
  if (!trimmed_command.empty()) {
    request.set_user_command(std::string(trimmed_command));
    AddTabGroupChatHistoryTurn(
        optimization_guide::proto::ChatHistoryTurn::ROLE_USER, trimmed_command);
  }

  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextHub, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&ContextHubService::HandleTabGroupModelExecutionResult,
                     weak_factory_.GetWeakPtr(), std::move(tabs),
                     std::move(callback)));
}

void ContextHubService::ExecuteMemoryBankChat(
    base::span<const int64_t> entry_ids,
    const std::string& user_command,
    MemoryBankChatCallback callback) {
  std::string_view trimmed_command =
      base::TrimWhitespaceASCII(user_command, base::TRIM_ALL);
  if (trimmed_command.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  GetEntriesByIds(
      entry_ids,
      base::BindOnce(&ContextHubService::OnMemoryBankEntriesFetched,
                     weak_factory_.GetWeakPtr(), std::string(trimmed_command),
                     std::move(callback)));
}

void ContextHubService::OnMemoryBankEntriesFetched(
    const std::string& user_command,
    MemoryBankChatCallback callback,
    std::vector<MemoryBankEntry> entries) {
  optimization_guide::proto::ContextHubRequest request;
  request.set_request_type(
      optimization_guide::proto::CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);

  for (const MemoryBankEntry& entry : entries) {
    *request.add_entry_items()->mutable_memory_bank_entry() =
        ToMemoryBankEntryProto(entry);
  }

  request.set_user_command(user_command);

  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextHub, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(
          &ContextHubService::HandleMemoryBankChatModelExecutionResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextHubService::HandleMemoryBankChatModelExecutionResult(
    MemoryBankChatCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  std::optional<optimization_guide::proto::ContextHubResponse> response;
  if (result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::ContextHubResponse>(*result.response);
  }
  if (!response || !response->has_memory_bank_chat_response()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(
      response->memory_bank_chat_response().text_response());
}

void ContextHubService::HandleTabGroupModelExecutionResult(
    std::vector<TabData> tabs,
    GroupTabsCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  std::optional<optimization_guide::proto::ContextHubResponse> response;
  if (result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::ContextHubResponse>(*result.response);
  }
  if (!response || !response->has_group_response()) {
    std::move(callback).Run({}, std::move(tabs));
    return;
  }

  std::vector<TabGroupEntry> groups;

  base::flat_map<int64_t, size_t> tab_index_map;
  for (size_t i = 0; i < tabs.size(); ++i) {
    tab_index_map.emplace(tabs[i].id, i);
  }

  for (const optimization_guide::proto::TabGroupMinimal& group_proto :
       response->group_response().minimal_tab_groups()) {
    std::vector<int64_t> valid_tab_ids;
    for (int64_t tab_id : group_proto.tab_ids()) {
      if (tab_index_map.contains(tab_id) &&
          std::ranges::find(valid_tab_ids, tab_id) == valid_tab_ids.end()) {
        valid_tab_ids.push_back(tab_id);
      }
    }

    if (valid_tab_ids.size() >= 2) {
      TabGroupEntry entry;
      entry.label = group_proto.label();
      entry.created_timestamp = base::Time::Now();
      entry.last_accessed_timestamp = entry.created_timestamp;
      for (int64_t tab_id : valid_tab_ids) {
        entry.tab_ids.push_back(tab_id);
        if (tab_index_map.contains(tab_id)) {
          size_t index = tab_index_map.at(tab_id);
          entry.tabs.push_back(std::move(tabs[index]));
          tab_index_map.erase(tab_id);
        }
      }
      groups.push_back(std::move(entry));
    }
  }

  if (tab_group_store_) {
    tab_group_store_->DeleteAllGroups(base::BindOnce(
        [](base::WeakPtr<ContextHubService> self,
           std::vector<TabGroupEntry> groups) {
          if (self && self->tab_group_store_) {
            self->tab_group_store_->AddAllGroups(std::move(groups),
                                                 base::DoNothing());
          }
        },
        weak_factory_.GetWeakPtr(), groups));
  }

  std::vector<TabData> ungrouped_tabs;
  for (context_hub::TabData& tab : tabs) {
    if (tab_index_map.contains(tab.id)) {
      ungrouped_tabs.push_back(std::move(tab));
    }
  }

  std::move(callback).Run(std::move(groups), std::move(ungrouped_tabs));
}

void ContextHubService::GroupTabs(std::vector<TabData> tabs,
                                  const std::string& user_command,
                                  GroupTabsCallback callback) {
  if (tabs.size() < 2) {
    std::move(callback).Run({}, std::move(tabs));
    return;
  }

  GenerateTabGroups(std::move(tabs), user_command, std::move(callback));
}

}  // namespace context_hub
