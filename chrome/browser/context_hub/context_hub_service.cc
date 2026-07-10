// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <algorithm>
#include <array>
#include <string>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"

namespace context_hub {

ContextHubService::ContextHubService(
    personal_context::PersonalContextService* personal_context_service,
    optimization_guide::RemoteModelExecutor*
        optimization_guide_remote_model_executor,
    std::unique_ptr<MemoryBank> memory_bank)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      optimization_guide_remote_model_executor_(
          CHECK_DEREF(optimization_guide_remote_model_executor)),
      memory_bank_(std::move(memory_bank)) {
  CHECK(memory_bank_);
}

ContextHubService::~ContextHubService() = default;

void ContextHubService::GenerateAutoTodos(AutoTodosCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kAutoTodos)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  personal_context::proto::AutoTodosRequest request_metadata;
  personal_context::ContextMemoryRequestOptions options;
  options.request_timeout =
      base::Seconds(features::kAutoTodosTimeoutSeconds.Get());

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
      request_metadata, options,
      base::BindOnce(&ContextHubService::OnAutoTodosFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextHubService::OnAutoTodosFetched(
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

  std::move(callback).Run(std::move(response));
}

void ContextHubService::SaveTab(
    const GURL& url,
    const std::string& tab_title,
    const std::string& page_text,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->SaveTab(url, tab_title, page_text, std::move(callback));
}

void ContextHubService::SaveTextSelection(
    const GURL& url,
    const std::string& tab_title,
    const std::string& selected_text,
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
    MemoryBank::GetAllEntriesCallback callback) const {
  memory_bank_->GetAllEntries(std::move(callback));
}

// TODO(crbug.com/531938478): Update to handle APC ingestion.
void ContextHubService::GenerateTabGroups(std::vector<TabData> tabs,
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

  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextHub, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&ContextHubService::HandleModelExecutionResult,
                     weak_factory_.GetWeakPtr(), std::move(tabs),
                     std::move(callback)));
}

void ContextHubService::HandleModelExecutionResult(
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

  std::vector<TabGroupData> groups;
  std::vector<TabData> ungrouped_tabs;

  base::flat_map<int32_t, size_t> tab_index_map;
  for (size_t i = 0; i < tabs.size(); ++i) {
    tab_index_map.emplace(tabs[i].id, i);
  }

  for (const optimization_guide::proto::TabGroup& group_proto :
       response->group_response().tab_groups()) {
    std::vector<int32_t> valid_tab_ids;
    for (const optimization_guide::proto::Tab& tab_proto : group_proto.tabs()) {
      int32_t tab_id = static_cast<int32_t>(tab_proto.tab_id());
      if (tab_index_map.contains(tab_id) &&
          std::ranges::find(valid_tab_ids, tab_id) == valid_tab_ids.end()) {
        valid_tab_ids.push_back(tab_id);
      }
    }

    if (valid_tab_ids.size() >= 2) {
      TabGroupData group_data;
      group_data.label = group_proto.label();
      for (int32_t tab_id : valid_tab_ids) {
        group_data.tabs.push_back(std::move(tabs[tab_index_map[tab_id]]));
        tab_index_map.erase(tab_id);
      }
      groups.push_back(std::move(group_data));
    }
  }

  for (context_hub::TabData& tab : tabs) {
    if (tab_index_map.contains(tab.id)) {
      ungrouped_tabs.push_back(std::move(tab));
    }
  }

  std::move(callback).Run(std::move(groups), std::move(ungrouped_tabs));
}

void ContextHubService::GroupTabs(std::vector<TabData> tabs,
                                  GroupTabsCallback callback) {
  if (tabs.size() < 2) {
    std::move(callback).Run({}, std::move(tabs));
    return;
  }

  GenerateTabGroups(std::move(tabs), std::move(callback));
}

}  // namespace context_hub
