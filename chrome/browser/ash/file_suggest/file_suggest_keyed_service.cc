// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_suggest/drive_file_suggestion_provider.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/ash/file_suggest/local_file_suggestion_provider.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {

namespace {
using SuggestResults = std::vector<FileSuggestData>;
}  // namespace

FileSuggestKeyedService::FileSuggestKeyedService(
    Profile* profile,
    app_list::PersistentProto<app_list::RemovedResultsProto> proto)
    : profile_(profile), proto_(std::move(proto)) {
  DCHECK(profile_);

  proto_.RegisterOnRead(
      base::BindOnce(&FileSuggestKeyedService::OnRemovedSuggestionProtoReady,
                     weak_factory_.GetWeakPtr()));
  proto_.Init();

  drive_file_suggestion_provider_ =
      std::make_unique<DriveFileSuggestionProvider>(
          profile, base::BindRepeating(
                       &FileSuggestKeyedService::OnSuggestionProviderUpdated,
                       weak_factory_.GetWeakPtr()));

  local_file_suggestion_provider_ =
      std::make_unique<LocalFileSuggestionProvider>(
          profile, base::BindRepeating(
                       &FileSuggestKeyedService::OnSuggestionProviderUpdated,
                       weak_factory_.GetWeakPtr()));
}

FileSuggestKeyedService::~FileSuggestKeyedService() = default;

void FileSuggestKeyedService::MaybeUpdateItemSuggestCache(
    base::PassKey<app_list::ZeroStateDriveProvider>) {
  drive_file_suggestion_provider_->MaybeUpdateItemSuggestCache(
      base::PassKey<FileSuggestKeyedService>());
}

void FileSuggestKeyedService::GetSuggestFileData(
    FileSuggestionType type,
    GetSuggestFileDataCallback callback) {
  // Always return null if `proto_` is not ready.
  if (!proto_.initialized()) {
    std::move(callback).Run(/*suggestions=*/absl::nullopt);
    return;
  }

  GetSuggestFileDataCallback filter_suggestions_callback =
      base::BindOnce(&FileSuggestKeyedService::FilterRemovedSuggestions,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  switch (type) {
    case FileSuggestionType::kDriveFile:
      drive_file_suggestion_provider_->GetSuggestFileData(
          std::move(filter_suggestions_callback));
      return;
    case FileSuggestionType::kLocalFile:
      local_file_suggestion_provider_->GetSuggestFileData(
          std::move(filter_suggestions_callback));
      return;
  }
}

void FileSuggestKeyedService::RemoveSuggestionsAndNotify(
    const std::vector<base::FilePath>& absolute_file_paths) {
  if (!IsProtoInitialized()) {
    return;
  }

  std::vector<std::pair<FileSuggestionType, std::string>> type_id_pairs;
  for (const auto& file_path : absolute_file_paths) {
    DCHECK(file_path.IsAbsolute());

    // Calculate the suggestion type based on `file_path`.
    GURL crack_url;
    const bool resolve_success =
        file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile_, file_path, file_manager::util::GetFileManagerURL(),
            &crack_url);
    DCHECK(resolve_success);
    const storage::FileSystemURL& file_system_url =
        file_manager::util::GetFileManagerFileSystemContext(profile_)
            ->CrackURLInFirstPartyContext(crack_url);
    DCHECK(file_system_url.is_valid());
    const FileSuggestionType type =
        file_system_url.type() == storage::kFileSystemTypeDriveFs
            ? FileSuggestionType::kDriveFile
            : FileSuggestionType::kLocalFile;

    type_id_pairs.emplace_back(type, CalculateSuggestionId(type, file_path));
  }
  RemoveSuggestionsByTypeIdPairs(type_id_pairs);
}

void FileSuggestKeyedService::RemoveSuggestionBySearchResultAndNotify(
    const SearchResultMetadata& search_result) {
  if (!IsProtoInitialized()) {
    return;
  }

  // `search_result` should refer to a suggested file.
  DCHECK(search_result.result_type ==
             ash::AppListSearchResultType::kZeroStateDrive ||
         search_result.result_type ==
             ash::AppListSearchResultType::kZeroStateFile);

  RemoveSuggestionsByTypeIdPairs(
      {{search_result.result_type ==
                ash::AppListSearchResultType::kZeroStateDrive
            ? FileSuggestionType::kDriveFile
            : FileSuggestionType::kLocalFile,
        search_result.id}});
}

app_list::PersistentProto<app_list::RemovedResultsProto>*
FileSuggestKeyedService::GetProto(
    base::PassKey<app_list::RemovedResultsRanker>) {
  return &proto_;
}

void FileSuggestKeyedService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FileSuggestKeyedService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FileSuggestKeyedService::HasPendingSuggestionFetchForTest() const {
  return drive_file_suggestion_provider_
             ->HasPendingDriveSuggestionFetchForTest() ||
         local_file_suggestion_provider_
             ->HasPendingLocalSuggestionFetchForTest();
}

void FileSuggestKeyedService::OnSuggestionProviderUpdated(
    FileSuggestionType type) {
  if (IsProtoInitialized()) {
    for (auto& observer : observers_) {
      observer.OnFileSuggestionUpdated(type);
    }
  }
}

bool FileSuggestKeyedService::IsReadyForTest() const {
  return local_file_suggestion_provider_->IsInitialized() &&
         IsProtoInitialized();
}

void FileSuggestKeyedService::FilterRemovedSuggestions(
    GetSuggestFileDataCallback callback,
    const absl::optional<std::vector<FileSuggestData>>& suggestions) {
  DCHECK(IsProtoInitialized());

  // There are no candidate suggestions to filter. Therefore, return early.
  if (!suggestions.has_value() || suggestions->empty()) {
    std::move(callback).Run(suggestions);
    return;
  }

  std::vector<FileSuggestData> filtered_suggestions;
  for (const auto& suggestion : *suggestions) {
    if (!proto_->removed_ids().contains(suggestion.id)) {
      // Skip the suggestions whose ids exist in `proto_`.
      filtered_suggestions.push_back(suggestion);
    }
  }

  std::move(callback).Run(filtered_suggestions);
}

bool FileSuggestKeyedService::IsProtoInitialized() const {
  return proto_.initialized();
}

void FileSuggestKeyedService::OnRemovedSuggestionProtoReady(
    app_list::ReadStatus read_status) {
  OnSuggestionProviderUpdated(FileSuggestionType::kDriveFile);

  if (local_file_suggestion_provider_->IsInitialized()) {
    OnSuggestionProviderUpdated(FileSuggestionType::kLocalFile);
  }
}

void FileSuggestKeyedService::RemoveSuggestionsByTypeIdPairs(
    const std::vector<std::pair<FileSuggestionType, std::string>>&
        type_id_pairs) {
  DCHECK(IsProtoInitialized());

  // Record the types of the removed suggestions. `observers_` should be
  // notified of the updates on these types.
  base::flat_set<FileSuggestionType> types_to_update;

  for (const auto& [type, id] : type_id_pairs) {
    // Record the suggestion id to the storage proto's map.
    // Note: We are using a map for its set capabilities; the map value is
    // arbitrary.
    const bool success =
        proto_->mutable_removed_ids()->insert({id, false}).second;

    // Skip the suggestion whose id is already in `proto_`.
    if (success) {
      types_to_update.insert(type);
    }
  }

  proto_.StartWrite();

  for (const auto& type : types_to_update) {
    OnSuggestionProviderUpdated(type);
  }
}

}  // namespace ash
