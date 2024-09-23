// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/drive_search_provider.h"

#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/app_list/search/common/string_util.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/drivefs/drivefs_search_query.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;

constexpr char kDriveSearchSchema[] = "drive_search://";
constexpr int kMaxResults = 50;
constexpr size_t kMinQuerySizeForSharedFiles = 5u;

// Outcome of a call to DriveSearchProvider::Start. These values persist
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class Status {
  kOk = 0,
  kDriveUnavailable = 1,
  kFileError = 2,
  kMaxValue = kFileError,
};

void LogStatus(Status status) {
  base::UmaHistogramEnumeration("Apps.AppList.DriveSearchProvider.Status",
                                status);
}

}  // namespace

DriveSearchProvider::DriveSearchProvider(Profile* profile,
                                         bool should_filter_shared_files,
                                         bool should_filter_directories)
    : SearchProvider(SearchCategory::kFiles),
      should_filter_shared_files_(should_filter_shared_files),
      should_filter_directories_(should_filter_directories),
      profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(drive_service_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DriveSearchProvider::~DriveSearchProvider() = default;

void DriveSearchProvider::SetQuerySource(
    drivefs::mojom::QueryParameters::QuerySource query_source) {
  query_source_ = query_source;
}

ash::AppListSearchResultType DriveSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kDriveSearch;
}

void DriveSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();

  weak_factory_.InvalidateWeakPtrs();

  if (!drive_service_ || !drive_service_->is_enabled()) {
    Results empty_results;
    SwapResults(&empty_results);
    LogStatus(Status::kDriveUnavailable);
    return;
  }

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  drivefs_search_query_ = drive_service_->CreateSearchQueryByFileName(
      base::UTF16ToUTF8(query), kMaxResults,
      drivefs::mojom::QueryParameters::SortField::kLastModified,
      drivefs::mojom::QueryParameters::SortDirection::kDescending,
      query_source_);

  if (drivefs_search_query_ == nullptr) {
    Results empty_results;
    SwapResults(&empty_results);
    LogStatus(Status::kDriveUnavailable);
    return;
  }

  drivefs_search_query_->GetNextPage(
      base::BindOnce(&DriveSearchProvider::OnSearchDriveByFileName,
                     weak_factory_.GetWeakPtr()));
}

void DriveSearchProvider::StopQuery() {
  weak_factory_.InvalidateWeakPtrs();
  last_query_.clear();
  last_tokenized_query_.reset();
  drivefs_search_query_.reset();
}

void DriveSearchProvider::OnSearchDriveByFileName(
    drive::FileError error,
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!drive::IsFileErrorOk(error) || !items.has_value()) {
    Results empty_results;
    SwapResults(&empty_results);
    LogStatus(Status::kFileError);
    return;
  }

  // Filter out shared files if it was not disabled in the ctor, and the query
  // length is below a threshold.
  if (should_filter_shared_files_ &&
      last_query_.size() < kMinQuerySizeForSharedFiles) {
    std::vector<drivefs::mojom::QueryItemPtr> filtered_items;
    for (auto& item : *items) {
      if (!item->metadata->shared) {
        filtered_items.push_back(std::move(item));
      }
    }
    items = std::move(filtered_items);
  }

  if (!drive_service_->IsMounted()) {
    return;
  }

  base::UmaHistogramTimes("Apps.AppList.DriveSearchProvider.DriveFSLatency",
                          base::TimeTicks::Now() - query_start_time_);
  results_returned_time_ = base::TimeTicks::Now();

  base::FilePath mount_path = drive_service_->GetMountPointPath();

  SearchProvider::Results results;
  for (const auto& item : *items) {
    // Strip leading separators so that the path can be reparented.
    const auto& path = item->path;
    DCHECK(!path.value().empty());
    const base::FilePath relative_path =
        !path.value().empty() && base::FilePath::IsSeparator(path.value()[0])
            ? base::FilePath(path.value().substr(1))
            : path;

    // Reparent the file path into the user's DriveFS mount.
    DCHECK(!relative_path.IsAbsolute());
    base::FilePath reparented_path = mount_path.Append(relative_path.value());

    std::optional<base::Time> last_accessed;
    // DriveFS surfaces atime in the virtual vilesystem as
    // `last_viewed_by_me_time` (http://go/lkrez), which does not account for
    // "last viewed by other users".
    // We can use `last_viewed_by_me_time` from the metadata response directly
    // as it should be the same (http://go/yufhv).
    // This avoids needing to perform I/O to stat the file.
    if (base::Time viewed_time = item->metadata->last_viewed_by_me_time;
        !viewed_time.is_null()) {
      last_accessed = viewed_time;
    }

    double relevance = FileResult::CalculateRelevance(
        last_tokenized_query_, reparented_path, last_accessed);

    std::unique_ptr<FileResult> result;
    GURL url(item->metadata->alternate_url);
    if (item->metadata->type ==
        drivefs::mojom::FileMetadata::Type::kDirectory) {
      // TODO: b/357740941 - Move this filtering to the DriveFS query.
      if (should_filter_directories_) {
        continue;
      }
      const auto type = item->metadata->shared
                            ? FileResult::Type::kSharedDirectory
                            : FileResult::Type::kDirectory;
      result = MakeResult(reparented_path, relevance, type, url,
                          item->metadata->item_id, item->metadata->title);
    } else {
      result = MakeResult(reparented_path, relevance, FileResult::Type::kFile,
                          url, item->metadata->item_id, item->metadata->title);
    }
    results.push_back(std::move(result));
  }

  SwapResults(&results);
  LogStatus(Status::kOk);
  base::UmaHistogramTimes("Apps.AppList.DriveSearchProvider.FileInfoLatency",
                          base::TimeTicks::Now() - results_returned_time_);
  base::UmaHistogramTimes("Apps.AppList.DriveSearchProvider.Latency",
                          base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> DriveSearchProvider::MakeResult(
    const base::FilePath& reparented_path,
    double relevance,
    FileResult::Type type,
    const GURL& url,
    const std::optional<std::string>& id,
    const std::optional<std::string>& title) {
  // Add "Google Drive" as details.
  std::u16string details =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);

  auto result = std::make_unique<FileResult>(
      /*id=*/kDriveSearchSchema + reparented_path.value(), reparented_path,
      details, ash::AppListSearchResultType::kDriveSearch,
      ash::SearchResultDisplayType::kList, relevance, last_query_, type,
      profile_, /*thumbnail_loader=*/nullptr);
  if (id.has_value()) {
    result->set_drive_id(id);
  } else {
    result->set_drive_id(GetDriveId(url));
  }
  if (title.has_value()) {
    result->SetTitle(base::UTF8ToUTF16(*title));
  }
  result->set_url(url);
  return result;
}

}  // namespace app_list
