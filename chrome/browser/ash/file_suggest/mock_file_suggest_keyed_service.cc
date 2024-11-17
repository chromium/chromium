// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

std::unique_ptr<KeyedService>
MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService(
    const base::FilePath& proto_path,
    content::BrowserContext* context) {
  PersistentProto<app_list::RemovedResultsProto> proto(proto_path,
                                                       base::TimeDelta());
  return std::make_unique<MockFileSuggestKeyedService>(
      Profile::FromBrowserContext(context), std::move(proto));
}

MockFileSuggestKeyedService::MockFileSuggestKeyedService(
    Profile* profile,
    PersistentProto<app_list::RemovedResultsProto> proto)
    : FileSuggestKeyedService(profile, std::move(proto)) {
  ON_CALL(*this, GetSuggestFileData)
      .WillByDefault(
          [this](FileSuggestionType type, GetSuggestFileDataCallback callback) {
            if (!IsProtoInitialized()) {
              std::move(callback).Run(/*suggestions=*/std::nullopt);
              return;
            }

            // Emulate `FileSuggestKeyedService` that returns data
            // asynchronously.
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &MockFileSuggestKeyedService::RunGetSuggestFileDataCallback,
                    weak_factory_.GetWeakPtr(), type, std::move(callback)));
          });

  ON_CALL(*this, RemoveSuggestionsAndNotify)
      .WillByDefault(
          [this](const std::vector<base::FilePath>& suggested_file_paths) {
            FileSuggestKeyedService::RemoveSuggestionsAndNotify(
                suggested_file_paths);
          });
}

MockFileSuggestKeyedService::~MockFileSuggestKeyedService() = default;

void MockFileSuggestKeyedService::SetSuggestionsForType(
    FileSuggestionType type,
    const std::optional<std::vector<FileSuggestData>>& suggestions) {
  type_suggestion_mappings_[type] = suggestions;
  OnSuggestionProviderUpdated(type);
}

void MockFileSuggestKeyedService::RunGetSuggestFileDataCallback(
    FileSuggestionType type,
    GetSuggestFileDataCallback callback) {
  std::optional<std::vector<FileSuggestData>> suggestions;
  auto iter = type_suggestion_mappings_.find(type);
  if (iter != type_suggestion_mappings_.end()) {
    suggestions = iter->second;
  }
  GetSuggestFileDataCallback filter_removed_suggestions_callback =
      base::BindOnce(&MockFileSuggestKeyedService::FilterRemovedSuggestions,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  FilterDuplicateSuggestions(std::move(filter_removed_suggestions_callback),
                             suggestions);
}

}  // namespace ash
