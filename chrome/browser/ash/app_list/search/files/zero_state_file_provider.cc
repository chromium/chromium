// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/zero_state_file_provider.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/files/justifications.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

constexpr char kSchema[] = "zero_state_file://";
constexpr size_t kMaxLocalFiles = 10u;

// Screenshots are identified as files that match ScreenshotXXX.png in the
// Downloads folder.
bool IsScreenshot(const base::FilePath& path,
                  const base::FilePath& downloads_path) {
  return path.DirName() == downloads_path && path.Extension() == ".png" &&
         path.BaseName().value().rfind("Screenshot", 0) == 0;
}

bool IsDriveDisabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
}

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : SearchProvider(SearchCategory::kFiles),
      profile_(profile),
      thumbnail_loader_(profile),
      file_suggest_service_(
          ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
              profile)),
      downloads_path_(
          file_manager::util::GetDownloadsFolderForProfile(profile)) {
  DCHECK(profile_);
  file_suggest_service_observation_.Observe(file_suggest_service_);
}

ZeroStateFileProvider::~ZeroStateFileProvider() = default;

ash::AppListSearchResultType ZeroStateFileProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateFile;
}

void ZeroStateFileProvider::StartZeroState() {
  query_start_time_ = base::TimeTicks::Now();

  // Despite this being for zero-state _local_ files only, we disable all
  // results in the Continue section if Drive is disabled.
  if (IsDriveDisabled(profile_)) {
    return;
  }

  file_suggest_service_->GetSuggestFileData(
      ash::FileSuggestionType::kLocalFile,
      base::BindOnce(&ZeroStateFileProvider::OnSuggestFileDataFetched,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateFileProvider::StopZeroState() {
  // Cancel any pending zero state callbacks.
  weak_factory_.InvalidateWeakPtrs();
}

void ZeroStateFileProvider::OnSuggestFileDataFetched(
    const std::optional<std::vector<ash::FileSuggestData>>& suggest_results) {
  if (suggest_results)
    SetSearchResults(*suggest_results);
}

void ZeroStateFileProvider::SetSearchResults(
    const std::vector<ash::FileSuggestData>& results) {
  const bool timestamp_based_score =
      ash::features::UseMixedFileLauncherContinueSection();
  const base::TimeDelta max_recency = ash::GetMaxFileSuggestionRecency();

  // Use valid results for search results.
  SearchProvider::Results new_results;
  for (size_t i = 0; i < std::min(results.size(), kMaxLocalFiles); ++i) {
    const auto& filepath = results[i].file_path;
    if (!IsScreenshot(filepath, downloads_path_)) {
      DCHECK(results[i].score.has_value());

      const double score = timestamp_based_score ? ash::ToTimestampBasedScore(
                                                       results[i], max_recency)
                                                 : *results[i].score;
      auto result = std::make_unique<FileResult>(
          results[i].id, filepath, results[i].prediction_reason,
          ash::AppListSearchResultType::kZeroStateFile,
          ash::SearchResultDisplayType::kContinue, score, std::u16string(),
          FileResult::Type::kFile, profile_, /*thumbnail_loader=*/nullptr);
      if (results[i].modified_time) {
        result->SetContinueFileSuggestionType(
            ash::ContinueFileSuggestionType::kModifiedByCurrentUserFile);
      } else if (results[i].viewed_time) {
        result->SetContinueFileSuggestionType(
            ash::ContinueFileSuggestionType::kViewedFile);
      }
      new_results.push_back(std::move(result));
    }
  }

  if (app_list_features::IsForceShowContinueSectionEnabled())
    AppendFakeSearchResults(&new_results);

  UMA_HISTOGRAM_TIMES("Apps.AppList.ZeroStateFileProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
  SwapResults(&new_results);
}

void ZeroStateFileProvider::AppendFakeSearchResults(Results* results) {
  constexpr int kTotalFakeFiles = 3;
  for (int i = 0; i < kTotalFakeFiles; ++i) {
    const base::FilePath path(base::FilePath(FILE_PATH_LITERAL(
        base::StrCat({"Fake-file-", base::NumberToString(i), ".png"}))));
    results->emplace_back(std::make_unique<FileResult>(
        /*id=*/kSchema + path.value(), path, u"-",
        ash::AppListSearchResultType::kZeroStateFile,
        ash::SearchResultDisplayType::kContinue, 0.1f, std::u16string(),
        FileResult::Type::kFile, profile_, /*thumbnail_loader=*/nullptr));
  }
}

void ZeroStateFileProvider::OnFileSuggestionUpdated(
    ash::FileSuggestionType type) {
  if (type == ash::FileSuggestionType::kLocalFile) {
    StartZeroState();
  }
}

}  // namespace app_list
