// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/drive_search_provider.h"

#include <cmath>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/app_list/search/common/string_util.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.DriveSearchProvider.Status", status);
}

// Stats each file to retrieve its last accessed time.
std::vector<std::unique_ptr<DriveSearchProvider::FileInfo>> GetFileInfo(
    std::vector<drivefs::mojom::QueryItemPtr> items,
    base::FilePath mount_path) {
  std::vector<std::unique_ptr<DriveSearchProvider::FileInfo>> item_info;

  for (const auto& item : items) {
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

    base::File::Info info;
    absl::optional<base::Time> last_accessed;
    if (base::GetFileInfo(reparented_path, &info))
      last_accessed = info.last_accessed;

    item_info.push_back(std::make_unique<DriveSearchProvider::FileInfo>(
        reparented_path, std::move(item->metadata), last_accessed));
  }

  return item_info;
}

}  // namespace

DriveSearchProvider::FileInfo::FileInfo(
    const base::FilePath& reparented_path,
    drivefs::mojom::FileMetadataPtr metadata,
    const absl::optional<base::Time>& last_accessed)
    : reparented_path(reparented_path), last_accessed(last_accessed) {
  this->metadata = std::move(metadata);
}

DriveSearchProvider::FileInfo::~FileInfo() = default;

DriveSearchProvider::DriveSearchProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(drive_service_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DriveSearchProvider::~DriveSearchProvider() = default;

ash::AppListSearchResultType DriveSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kDriveSearch;
}

void DriveSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();

  weak_factory_.InvalidateWeakPtrs();

  if (!drive_service_ || !drive_service_->is_enabled()) {
    LogStatus(Status::kDriveUnavailable);
    return;
  }

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  drive_service_->SearchDriveByFileName(
      base::UTF16ToUTF8(query), kMaxResults,
      drivefs::mojom::QueryParameters::SortField::kLastModified,
      drivefs::mojom::QueryParameters::SortDirection::kDescending,
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
      base::BindOnce(&DriveSearchProvider::OnSearchDriveByFileName,
                     weak_factory_.GetWeakPtr()));
}

void DriveSearchProvider::StopQuery() {
  weak_factory_.InvalidateWeakPtrs();
  last_query_.clear();
  last_tokenized_query_.reset();
}

void DriveSearchProvider::OnSearchDriveByFileName(
    drive::FileError error,
    std::vector<drivefs::mojom::QueryItemPtr> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FileError::FILE_ERROR_OK) {
    LogStatus(Status::kFileError);
    return;
  }

  // Filter out shared files if the query length is below a threshold.
  if (last_query_.size() < kMinQuerySizeForSharedFiles) {
    std::vector<drivefs::mojom::QueryItemPtr> filtered_items;
    for (auto& item : items) {
      if (!item->metadata->shared)
        filtered_items.push_back(std::move(item));
    }
    items = std::move(filtered_items);
  }

  if (!drive_service_->IsMounted())
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetFileInfo, std::move(items),
                     drive_service_->GetMountPointPath()),
      base::BindOnce(&DriveSearchProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void DriveSearchProvider::SetSearchResults(
    std::vector<std::unique_ptr<DriveSearchProvider::FileInfo>> item_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SearchProvider::Results results;
  for (const auto& info : item_info) {
    double relevance = FileResult::CalculateRelevance(
        last_tokenized_query_, info->reparented_path, info->last_accessed);

    std::unique_ptr<FileResult> result;
    GURL url(info->metadata->alternate_url);
    if (info->metadata->type ==
        drivefs::mojom::FileMetadata::Type::kDirectory) {
      const auto type = info->metadata->shared
                            ? FileResult::Type::kSharedDirectory
                            : FileResult::Type::kDirectory;
      result =
          MakeResult(info->reparented_path, relevance, type, GetDriveId(url));
    } else {
      result = MakeResult(info->reparented_path, relevance,
                          FileResult::Type::kFile, GetDriveId(url));
    }
    results.push_back(std::move(result));
  }

  SwapResults(&results);
  LogStatus(Status::kOk);
  UMA_HISTOGRAM_TIMES("Apps.AppList.DriveSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> DriveSearchProvider::MakeResult(
    const base::FilePath& reparented_path,
    double relevance,
    FileResult::Type type,
    const absl::optional<std::string>& drive_id) {
  // Add "Google Drive" as details.
  std::u16string details =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);

  auto result = std::make_unique<FileResult>(
      /*id=*/kDriveSearchSchema + reparented_path.value(), reparented_path,
      details, ash::AppListSearchResultType::kDriveSearch,
      ash::SearchResultDisplayType::kList, relevance, last_query_, type,
      profile_);
  result->set_drive_id(drive_id);
  return result;
}

}  // namespace app_list
