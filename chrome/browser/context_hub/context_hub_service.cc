// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"

namespace context_hub {

ContextHubService::ContextHubService(
    personal_context::PersonalContextService* personal_context_service,
    std::unique_ptr<MemoryBank> memory_bank)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
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

}  // namespace context_hub
