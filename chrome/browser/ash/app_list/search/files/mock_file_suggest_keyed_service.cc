// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/mock_file_suggest_keyed_service.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ash/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

std::unique_ptr<KeyedService>
MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService(
    const base::FilePath& proto_path,
    content::BrowserContext* context) {
  PersistentProto<RemovedResultsProto> proto(proto_path, base::TimeDelta());
  return std::make_unique<app_list::MockFileSuggestKeyedService>(
      Profile::FromBrowserContext(context), std::move(proto));
}

MockFileSuggestKeyedService::MockFileSuggestKeyedService(
    Profile* profile,
    PersistentProto<RemovedResultsProto> proto)
    : app_list::FileSuggestKeyedService(profile, std::move(proto)) {
  ON_CALL(*this, RemoveSuggestionsAndNotify)
      .WillByDefault(
          [this](const std::vector<base::FilePath>& suggested_file_paths) {
            FileSuggestKeyedService::RemoveSuggestionsAndNotify(
                suggested_file_paths);
          });
}

MockFileSuggestKeyedService::~MockFileSuggestKeyedService() = default;

void MockFileSuggestKeyedService::GetSuggestFileData(
    app_list::FileSuggestionType type,
    GetSuggestFileDataCallback callback) {
  if (!IsProtoInitialized()) {
    std::move(callback).Run(/*suggestions=*/absl::nullopt);
    return;
  }

  // Emulate `FileSuggestKeyedService` that returns data asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MockFileSuggestKeyedService::RunGetSuggestFileDataCallback,
          weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void MockFileSuggestKeyedService::SetSuggestionsForType(
    app_list::FileSuggestionType type,
    const absl::optional<std::vector<app_list::FileSuggestData>>& suggestions) {
  type_suggestion_mappings_[type] = suggestions;
  OnSuggestionProviderUpdated(type);
}

void MockFileSuggestKeyedService::RunGetSuggestFileDataCallback(
    app_list::FileSuggestionType type,
    GetSuggestFileDataCallback callback) {
  absl::optional<std::vector<FileSuggestData>> suggestions;
  auto iter = type_suggestion_mappings_.find(type);
  if (iter != type_suggestion_mappings_.end())
    suggestions = iter->second;
  FilterRemovedSuggestions(std::move(callback), suggestions);
}

}  // namespace app_list
