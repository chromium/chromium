// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/zero_state_drive_provider.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "content/public/browser/browser_context.h"

namespace app_list {
namespace {

using SuggestResults = std::vector<ash::FileSuggestData>;

// How long to wait before making the first request for results from the
// ItemSuggestCache.
constexpr base::TimeDelta kFirstUpdateDelay = base::Seconds(10);

void LogLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Apps.AppList.DriveZeroStateProvider.Latency",
                          latency);
}

}  // namespace

ZeroStateDriveProvider::ZeroStateDriveProvider(
    Profile* profile,
    SearchController* search_controller,
    drive::DriveIntegrationService* drive_service,
    session_manager::SessionManager* session_manager)
    : SearchProvider(SearchCategory::kFiles),
      profile_(profile),
      drive_service_(drive_service),
      session_manager_(session_manager),
      file_suggest_service_(
          ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
              profile)),
      construction_time_(base::Time::Now()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);

  // `FileSuggestKeyedServiceFactory` ensures to build the keyed
  // service when the app list syncable service is built. Meanwhile,
  // `ZeroStateDriveProvider` is built only when the app list syncable service
  // exists. Therefore, `file_suggest_service_` should always be true.
  DCHECK(file_suggest_service_);

  if (drive_service_) {
    if (drive_service_->IsMounted()) {
      // DriveFS is mounted, so we can fetch results immediately.
      OnFileSystemMounted();
    } else {
      // Wait for DriveFS to be mounted, then fetch results. This happens in
      // OnFileSystemMounted.
      Observe(drive_service_.get());
    }
  }

  if (session_manager_)
    session_observation_.Observe(session_manager_.get());

  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager)
    power_observation_.Observe(power_manager);

  file_suggest_service_observation_.Observe(file_suggest_service_);
}

ZeroStateDriveProvider::~ZeroStateDriveProvider() = default;

void ZeroStateDriveProvider::OnFileSystemMounted() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ZeroStateDriveProvider::MaybeUpdateCache,
                     update_cache_weak_factory_.GetWeakPtr()),
      kFirstUpdateDelay);
}

void ZeroStateDriveProvider::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "ZeroStateDriveProvider::OnSessionStateChanged");
  // Update cache if the user has logged in.
  if (session_manager_->session_state() ==
      session_manager::SessionState::ACTIVE) {
    MaybeUpdateCache();
  }
}

void ZeroStateDriveProvider::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  // Update cache if the screen changed from off to on.
  if (screen_off_ && !proto.dimmed() && !proto.off()) {
    MaybeUpdateCache();
  }
  screen_off_ = proto.off();
}

void ZeroStateDriveProvider::StopZeroState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel any in-flight queries for this provider.
  suggestion_query_weak_factory_.InvalidateWeakPtrs();

  // Update the cache for when the zero state is next requested.
  MaybeUpdateCache();
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
  //
  // If drive files and local files need to be mixed in continue section, create
  // ranking using time stamps, so local and drive files are consistently
  // ranked.
  const bool timestamp_based_score =
      ash::features::UseMixedFileLauncherContinueSection();

  const double total_items = static_cast<double>(suggest_results.size());
  int item_index = 0;

  const base::TimeDelta max_recency = ash::GetMaxFileSuggestionRecency();
  SearchProvider::Results provider_results;
  for (const auto& result : suggest_results) {
    const double score = timestamp_based_score
                             ? ash::ToTimestampBasedScore(result, max_recency)
                             : (1.0 - item_index / total_items);
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

void ZeroStateDriveProvider::MaybeUpdateCache() {
  if (base::Time::Now() - kFirstUpdateDelay > construction_time_) {
    file_suggest_service_->MaybeUpdateItemSuggestCache(
        base::PassKey<ZeroStateDriveProvider>());
  }
}

void ZeroStateDriveProvider::OnFileSuggestionUpdated(
    ash::FileSuggestionType type) {
  if (type == ash::FileSuggestionType::kDriveFile) {
    StartZeroState();
  }
}

}  // namespace app_list
