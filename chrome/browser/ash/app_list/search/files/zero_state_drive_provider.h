// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash {
struct FileSuggestData;
enum class FileSuggestionType;
}  // namespace ash

namespace app_list {
class SearchController;

class ZeroStateDriveProvider : public SearchProvider,
                               public drive::DriveIntegrationServiceObserver,
                               public session_manager::SessionManagerObserver,
                               public chromeos::PowerManagerClient::Observer,
                               public ash::FileSuggestKeyedService::Observer {
 public:
  ZeroStateDriveProvider(Profile* profile,
                         SearchController* search_controller,
                         drive::DriveIntegrationService* drive_service,
                         session_manager::SessionManager* session_manager);
  ~ZeroStateDriveProvider() override;

  ZeroStateDriveProvider(const ZeroStateDriveProvider&) = delete;
  ZeroStateDriveProvider& operator=(const ZeroStateDriveProvider&) = delete;

  // drive::DriveIntegrationServiceObserver:
  void OnFileSystemMounted() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

  // SearchProvider:
  void StartZeroState() override;
  void StopZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  // Called when file suggestion data are fetched from the service.
  void OnSuggestFileDataFetched(
      const absl::optional<std::vector<ash::FileSuggestData>>& suggest_results);

  // Builds the search results from file suggestions then publishes the results.
  void SetSearchResults(
      const std::vector<ash::FileSuggestData>& suggest_results);

  std::unique_ptr<FileResult> MakeListResult(
      const std::string& result_id,
      const base::FilePath& filepath,
      const absl::optional<std::u16string>& prediction_reason,
      const float relevance);

  // Requests an update from the ItemSuggestCache, but only if the call is long
  // enough after the provider was constructed. This helps ease resource
  // contention at login, and prevents the call from failing because Google auth
  // tokens haven't been set up yet. If the productivity launcher is disabled,
  // this does nothing.
  void MaybeUpdateCache();

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(ash::FileSuggestionType type) override;

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;
  session_manager::SessionManager* const session_manager_;

  const base::raw_ptr<ash::FileSuggestKeyedService> file_suggest_service_;

  const base::Time construction_time_;
  base::TimeTicks query_start_time_;

  // Whether or not the screen is off due to idling.
  bool screen_off_ = true;

  base::ScopedObservation<drive::DriveIntegrationService,
                          drive::DriveIntegrationServiceObserver>
      drive_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_observation_{this};
  base::ScopedObservation<ash::FileSuggestKeyedService,
                          ash::FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to guard the task of updating the item suggest cache.
  base::WeakPtrFactory<ZeroStateDriveProvider> update_cache_weak_factory_{this};

  // Used to guard the query for drive file suggestions.
  base::WeakPtrFactory<ZeroStateDriveProvider> suggestion_query_weak_factory_{
      this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
