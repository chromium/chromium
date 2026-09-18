// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/zero_state_drive_provider.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

using SuggestResults = std::vector<ash::FileSuggestData>;

void LogLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Apps.AppList.DriveZeroStateProvider.Latency",
                          latency);
}

}  // namespace

ZeroStateDriveProvider::ZeroStateDriveProvider(Profile* profile)
    : SearchProvider(SearchCategory::kFiles),
      profile_(profile),
      file_suggest_service_(
          ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
              profile)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_, base::NotFatalUntil::M160);

  // `FileSuggestKeyedServiceFactory` ensures to build the keyed
  // service when the app list syncable service is built. Meanwhile,
  // `ZeroStateDriveProvider` is built only when the app list syncable service
  // exists. Therefore, `file_suggest_service_` should always be true.
  CHECK(file_suggest_service_, base::NotFatalUntil::M160);

  file_suggest_service_observation_.Observe(file_suggest_service_);
}

ZeroStateDriveProvider::~ZeroStateDriveProvider() = default;

void ZeroStateDriveProvider::StopZeroState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel any in-flight queries for this provider.
  suggestion_query_weak_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType ZeroStateDriveProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateDrive;
}

void ZeroStateDriveProvider::StartZeroState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  query_start_time_ = base::TimeTicks::Now();

  // Cancel any in-flight queries for this provider.
  suggestion_query_weak_factory_.InvalidateWeakPtrs();

  file_suggest_service_->GetSuggestFileData(
      ash::FileSuggestionType::kDriveFile,
      base::BindOnce(&ZeroStateDriveProvider::OnSuggestFileDataFetched,
                     suggestion_query_weak_factory_.GetWeakPtr()));
}

void ZeroStateDriveProvider::OnSuggestFileDataFetched(
    const std::optional<SuggestResults>& suggest_results) {
  // Fail to fetch the suggest data, so return early.
  if (!suggest_results) {
    // Send empty result list to search controller to unblock zero state.
    SearchProvider::Results empty_results;
    SwapResults(&empty_results);
    return;
  }

  SetSearchResults(*suggest_results);
}

void ZeroStateDriveProvider::SetSearchResults(
    const SuggestResults& suggest_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Assign scores to results by simply using their position in the results
  // list. The order of results from the ItemSuggest API is significant:
  // the first is better than the second, etc. Resulting scores are in [0, 1].
  const double total_items = static_cast<double>(suggest_results.size());
  int item_index = 0;

  SearchProvider::Results provider_results;
  for (const auto& result : suggest_results) {
    const double score = 1.0 - item_index / total_items;
    ++item_index;
    auto provider_result = std::make_unique<FileResult>(
        result.id, result.file_path, result.prediction_reason,
        ash::AppListSearchResultType::kZeroStateDrive,
        ash::SearchResultDisplayType::kContinue, score, std::u16string(),
        FileResult::Type::kFile, profile_, /*thumbnail_loader=*/nullptr);
    if (result.modified_time) {
      provider_result->SetContinueFileSuggestionType(
          ash::ContinueFileSuggestionType::kModifiedByCurrentUserDrive);
    } else if (result.viewed_time) {
      provider_result->SetContinueFileSuggestionType(
          ash::ContinueFileSuggestionType::kViewedDrive);
    } else if (result.shared_time) {
      provider_result->SetContinueFileSuggestionType(
          ash::ContinueFileSuggestionType::kSharedWithUserDrive);
    }

    provider_results.emplace_back(std::move(provider_result));
  }

  SwapResults(&provider_results);
  LogLatency(base::TimeTicks::Now() - query_start_time_);
}

void ZeroStateDriveProvider::OnFileSuggestionUpdated(
    ash::FileSuggestionType type) {
  if (type == ash::FileSuggestionType::kDriveFile) {
    StartZeroState();
  }
}

}  // namespace app_list
