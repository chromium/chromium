// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/drive_recent_file_suggestion_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/justifications.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace ash {
namespace {

constexpr char kBaseHistogramName[] = "Ash.Search.FileSuggestions.DriveRecents";

drivefs::mojom::QueryParametersPtr CreateRecentlyModifiedQuery(
    base::TimeDelta max_recency) {
  auto query = drivefs::mojom::QueryParameters::New();
  query->modified_time = base::Time::Now() - max_recency;
  query->modified_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  query->viewed_time = base::Time::Now() - max_recency;
  query->viewed_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  query->page_size = 15;
  query->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  query->sort_field = drivefs::mojom::QueryParameters::SortField::kLastModified;
  return query;
}

drivefs::mojom::QueryParametersPtr CreateRecentlyViewedQuery(
    base::TimeDelta max_recency) {
  auto query = drivefs::mojom::QueryParameters::New();
  query->page_size = 15;
  query->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  query->sort_field =
      drivefs::mojom::QueryParameters::SortField::kLastViewedByMe;
  query->viewed_time = base::Time::Now() - max_recency;
  query->viewed_time_operator =
      drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  return query;
}

drivefs::mojom::QueryParametersPtr CreateSharedWithMeQuery() {
  auto query = drivefs::mojom::QueryParameters::New();
  query->page_size = 15;
  query->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  query->shared_with_me = true;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  query->sort_field = drivefs::mojom::QueryParameters::SortField::kSharedWithMe;
  return query;
}

FileSuggestData CreateFileSuggestionWithJustification(
    const base::FilePath& path,
    const std::optional<std::string>& title,
    const std::optional<base::Time>& modified_time,
    const std::optional<base::Time>& viewed_time,
    const std::optional<base::Time>& shared_time,
    const std::optional<std::u16string>& justification_string,
    const std::optional<std::string>& drive_file_id,
    const std::optional<std::string>& icon_url) {
  return FileSuggestData(
      FileSuggestionType::kDriveFile, path, title, justification_string,
      modified_time, viewed_time, shared_time,
      /*new_score=*/std::nullopt, drive_file_id, /*icon_url=*/icon_url);
}

std::optional<std::u16string> CreateSuggestionStatusForNonSharedFile(
    const drivefs::mojom::FileMetadata& file_metadata) {
  const base::Time& modified_time = file_metadata.modification_time;
  const base::Time& viewed_time = file_metadata.last_viewed_by_me_time;
  if (viewed_time > modified_time) {
    return app_list::GetJustificationString(
        FileSuggestionJustificationType::kViewed, viewed_time, "");
  }

  // Last modification was by the user.
  if (file_metadata.modified_by_me_time &&
      !file_metadata.modified_by_me_time->is_null() &&
      file_metadata.modified_by_me_time >= modified_time) {
    return app_list::GetJustificationString(
        FileSuggestionJustificationType::kModifiedByCurrentUser,
        *file_metadata.modified_by_me_time, "");
  }

  const auto& user_info = file_metadata.last_modifying_user;
  return app_list::GetJustificationString(
      FileSuggestionJustificationType::kModified, modified_time,
      user_info ? user_info->display_name : "");
}

std::optional<FileSuggestData> CreateFileSuggestion(
    const base::FilePath& path,
    const drivefs::mojom::FileMetadata& file_metadata,
    base::TimeDelta max_recency) {
  const base::Time& viewed_time = file_metadata.last_viewed_by_me_time;
  const base::Time modified_time =
      file_metadata.modified_by_me_time.value_or(base::Time());
  const std::string& icon_url = file_metadata.custom_icon_url;
  const std::optional<std::string>& title = file_metadata.title;

  // If the file was shared with user, but not yet viewed by the user, surface
  // it as a shared file.
  if (const std::optional<base::Time>& shared_time =
          file_metadata.shared_with_me_time;
      shared_time && !shared_time->is_null() && viewed_time.is_null()) {
    if ((base::Time::Now() - *shared_time).magnitude() > max_recency) {
      return std::nullopt;
    }
    std::string sharing_user_name;
    if (features::IsShowSharingUserInLauncherContinueSectionEnabled()) {
      const auto& user_info = file_metadata.sharing_user;
      sharing_user_name = user_info ? user_info->display_name : "";
    }
    return {CreateFileSuggestionWithJustification(
        path, title, /*modified_time=*/std::nullopt,
        /*viewed_time*/ std::nullopt, *shared_time,
        app_list::GetJustificationString(
            FileSuggestionJustificationType::kShared, *shared_time,
            sharing_user_name),
        file_metadata.item_id, icon_url)};
  }

  if (viewed_time <= file_metadata.modification_time &&
      (base::Time::Now() - file_metadata.modification_time).magnitude() <=
          max_recency) {
    base::UmaHistogramBoolean(
        base::JoinString({kBaseHistogramName, "ModifyingUserMetadataPresent"},
                         "."),
        !!file_metadata.last_modifying_user);
  }

  if ((base::Time::Now() - modified_time).magnitude() > max_recency) {
    if ((base::Time::Now() - viewed_time).magnitude() > max_recency) {
      return std::nullopt;
    }

    return CreateFileSuggestionWithJustification(
        path, title,
        /*modified_time=*/std::nullopt, viewed_time,
        /*shared_time=*/std::nullopt,
        CreateSuggestionStatusForNonSharedFile(file_metadata),
        file_metadata.item_id, icon_url);
  }

  return CreateFileSuggestionWithJustification(
      path, title, modified_time, viewed_time, /*shared_time=*/std::nullopt,
      CreateSuggestionStatusForNonSharedFile(file_metadata),
      file_metadata.item_id, icon_url);
}

}  // namespace

DriveRecentFileSuggestionProvider::DriveRecentFileSuggestionProvider(
    Profile* profile,
    base::RepeatingCallback<void(FileSuggestionType)> notify_update_callback)
    : FileSuggestionProvider(notify_update_callback),
      profile_(profile),
      max_recency_(GetMaxFileSuggestionRecency()) {
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile);
  if (drive_integration_service) {
    drive::DriveIntegrationService::Observer::Observe(
        drive_integration_service);
    auto* drive_fs_host = drive_integration_service->GetDriveFsHost();
    if (drive_fs_host) {
      drivefs::DriveFsHost::Observer::Observe(drive_fs_host);
    }
  }
}

DriveRecentFileSuggestionProvider::~DriveRecentFileSuggestionProvider() =
    default;

void DriveRecentFileSuggestionProvider::GetSuggestFileData(
    GetSuggestFileDataCallback callback) {
  const bool has_active_request =
      !on_drive_results_ready_callback_list_.empty();

  // Return early if there is an active search request. `callback` will run when
  // the active search completes.
  if (has_active_request) {
    // Add `callback` to the waiting list, and return.
    on_drive_results_ready_callback_list_.AddUnsafe(std::move(callback));
    return;
  }

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);

  // When drive service is disabled or mount fails, send back an empty list of
  // results to avoid further waiting for file suggest data. Check
  // `mount_failed` before checking `IsMounted` to catch the case where not only
  // are we not mounted, but mounting has completely failed.
  if (drive_service &&
      (!drive_service->is_enabled() || drive_service->mount_failed())) {
    std::move(callback).Run(
        /*suggest_results=*/std::vector<FileSuggestData>());
    return;
  }

  // If there is not any available drive service, return early.
  if (!drive_service || !drive_service->IsMounted()) {
    std::move(callback).Run(/*suggest_results=*/std::nullopt);
    return;
  }

  if (can_use_cache_) {
    std::move(callback).Run(GetSuggestionsFromLatestQueryResults());
    return;
  }

  on_drive_results_ready_callback_list_.AddUnsafe(std::move(callback));

  search_start_time_ = base::Time::Now();
  query_result_files_by_path_.clear();

  // Allow using cached results for requests that come in after the current one
  // finishes.
  can_use_cache_ = true;

  base::RepeatingClosure search_callback = base::BarrierClosure(
      3, base::BindOnce(
             &DriveRecentFileSuggestionProvider::OnRecentFilesSearchesCompleted,
             weak_factory_.GetWeakPtr()));
  PerformSearch(SearchType::kLastModified,
                CreateRecentlyModifiedQuery(max_recency_), drive_service,
                search_callback);
  PerformSearch(SearchType::kLastViewed,
                CreateRecentlyViewedQuery(max_recency_), drive_service,
                search_callback);
  PerformSearch(SearchType::kSharedWithUser, CreateSharedWithMeQuery(),
                drive_service, search_callback);
}

// static
std::string DriveRecentFileSuggestionProvider::GetHistogramSuffix(
    SearchType search_type) {
  switch (search_type) {
    case SearchType::kLastViewed:
      return "Viewed";
    case SearchType::kLastModified:
      return "Modified";
    case SearchType::kSharedWithUser:
      return "Shared";
  }
}

void DriveRecentFileSuggestionProvider::OnDriveIntegrationServiceDestroyed() {
  drive::DriveIntegrationService::Observer::Reset();
  drivefs::DriveFsHost::Observer::Reset();

  can_use_cache_ = false;
  query_result_files_by_path_.clear();
  on_drive_results_ready_callback_list_.Notify(
      /*suggest_results=*/std::nullopt);
}

void DriveRecentFileSuggestionProvider::OnFileSystemMounted() {
  if (!drivefs::DriveFsHost::Observer::GetHost()) {
    auto* drive_fs_host = drive::DriveIntegrationService::Observer::GetService()
                              ->GetDriveFsHost();
    if (drive_fs_host) {
      drivefs::DriveFsHost::Observer::Observe(drive_fs_host);
    }
  }
  NotifySuggestionUpdate(FileSuggestionType::kDriveFile);
}

void DriveRecentFileSuggestionProvider::OnHostDestroyed() {
  drivefs::DriveFsHost::Observer::Reset();
}

void DriveRecentFileSuggestionProvider::OnUnmounted() {
  can_use_cache_ = false;
  query_result_files_by_path_.clear();
  on_drive_results_ready_callback_list_.Notify(
      /*suggest_results=*/std::nullopt);

  NotifySuggestionUpdate(FileSuggestionType::kDriveFile);
}

void DriveRecentFileSuggestionProvider::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  can_use_cache_ = false;

  for (const auto& change : changes) {
    if (change.type == drivefs::mojom::FileChange::Type::kDelete) {
      query_result_files_by_path_.erase(change.path);
    }
  }
}

void DriveRecentFileSuggestionProvider::PerformSearch(
    SearchType search_type,
    drivefs::mojom::QueryParametersPtr query,
    drive::DriveIntegrationService* drive_service,
    base::RepeatingClosure callback) {
  drive_service->GetDriveFsHost()->PerformSearch(
      std::move(query),
      base::BindOnce(
          &DriveRecentFileSuggestionProvider::OnSearchRequestComplete,
          weak_factory_.GetWeakPtr(), search_type, std::move(callback)));
}

void DriveRecentFileSuggestionProvider::MaybeUpdateItemSuggestCache(
    base::PassKey<FileSuggestKeyedService>) {}

void DriveRecentFileSuggestionProvider::OnSearchRequestComplete(
    SearchType search_type,
    base::RepeatingClosure callback,
    drive::FileError error,
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  // `error` is an enum, but has negative values, so UmaEnumeration does not
  // work - record it as a count instead.
  base::UmaHistogramCounts100(
      base::JoinString(
          {kBaseHistogramName, "QueryResult", GetHistogramSuffix(search_type)},
          "."),
      std::abs(error));

  if (!drive::IsFileErrorOk(error) &&
      error != drive::FileError::FILE_ERROR_INVALID_OPERATION) {
    can_use_cache_ = false;
  }

  if (drive::IsFileErrorOk(error) && items) {
    base::UmaHistogramTimes(
        base::JoinString({kBaseHistogramName, "DurationOnSuccess",
                          GetHistogramSuffix(search_type)},
                         "."),
        base::Time::Now() - search_start_time_);
    base::UmaHistogramCounts100(
        base::JoinString(
            {kBaseHistogramName, "ItemCount", GetHistogramSuffix(search_type)},
            "."),
        items->size());

    size_t folder_count = 0u;
    for (auto& item : *items) {
      if (item->metadata->type ==
          drivefs::mojom::FileMetadata::Type::kDirectory) {
        ++folder_count;
        continue;
      }
      query_result_files_by_path_.emplace(item->path,
                                          std::move(item->metadata));
    }
    base::UmaHistogramCounts100(
        base::JoinString({kBaseHistogramName, "FolderCount",
                          GetHistogramSuffix(search_type)},
                         "."),
        folder_count);

  } else {
    base::UmaHistogramTimes(
        base::JoinString({kBaseHistogramName, "DurationOnError",
                          GetHistogramSuffix(search_type)},
                         "."),
        base::Time::Now() - search_start_time_);
  }
  callback.Run();
}

std::vector<FileSuggestData>
DriveRecentFileSuggestionProvider::GetSuggestionsFromLatestQueryResults() {
  drive::DriveIntegrationService* const drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  CHECK(drive_service);

  std::vector<FileSuggestData> results;
  for (const auto& item : query_result_files_by_path_) {
    base::FilePath root("/");
    base::FilePath path = drive_service->GetMountPointPath();
    if (!root.AppendRelativePath(item.first, &path)) {
      continue;
    }

    std::optional<FileSuggestData> suggestion =
        CreateFileSuggestion(path, *item.second, max_recency_);
    if (suggestion) {
      results.push_back(*suggestion);
    }
  }

  base::ranges::sort(results, [](const auto& lhs, const auto& rhs) {
    if ((lhs.modified_time || rhs.modified_time) &&
        lhs.modified_time != rhs.modified_time) {
      return lhs.modified_time.value_or(base::Time()) >
             rhs.modified_time.value_or(base::Time());
    }

    if ((lhs.viewed_time || rhs.viewed_time) &&
        lhs.viewed_time != rhs.viewed_time) {
      return lhs.viewed_time.value_or(base::Time()) >
             rhs.viewed_time.value_or(base::Time());
    }

    return lhs.shared_time.value_or(base::Time()) >
           rhs.shared_time.value_or(base::Time());
  });

  return results;
}

void DriveRecentFileSuggestionProvider::OnRecentFilesSearchesCompleted() {
  drive::DriveIntegrationService* const drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!drive_service || !drive_service->IsMounted()) {
    can_use_cache_ = false;
    query_result_files_by_path_.clear();
    on_drive_results_ready_callback_list_.Notify(
        std::vector<FileSuggestData>());
    NotifySuggestionUpdate(FileSuggestionType::kDriveFile);
    return;
  }

  std::vector<FileSuggestData> results = GetSuggestionsFromLatestQueryResults();

  base::UmaHistogramTimes(
      base::JoinString({kBaseHistogramName, "DurationOnSuccess.Total"}, "."),
      base::Time::Now() - search_start_time_);
  base::UmaHistogramCounts100(
      base::JoinString({kBaseHistogramName, "ItemCount.Total"}, "."),
      results.size());

  for (size_t i = 0; i < results.size(); ++i) {
    if (!results[i].modified_time && !results[i].viewed_time &&
        results[i].shared_time) {
      base::UmaHistogramCounts100(
          base::JoinString({kBaseHistogramName, "FirstSharedSuggestionIndex"},
                           "."),
          i);
      break;
    }
  }

  on_drive_results_ready_callback_list_.Notify(results);
  if (can_use_cache_) {
    NotifySuggestionUpdate(FileSuggestionType::kDriveFile);
  }
}

}  // namespace ash
