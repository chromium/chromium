// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/string_value.pb.h"
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
void ContextHubService::GenerateTabGroups(std::string prompt) {
  optimization_guide::proto::StringValue request;
  request.set_value(std::move(prompt));

  // TODO(crbug.com/531920873): Use prod feature key once available.
  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kTest, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&ContextHubService::HandleModelExecutionResult,
                     weak_factory_.GetWeakPtr()));
}

void ContextHubService::HandleModelExecutionResult(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (result.response.has_value()) {
    std::optional<optimization_guide::proto::StringValue> string_value =
        optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::StringValue>(result.response.value());
    if (string_value) {
      // TODO(crbug.com/482383206): Handle model execution response.
      DVLOG(1) << "Model execution result: " << string_value->value();
    }
  }
}

}  // namespace context_hub
