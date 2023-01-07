// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_FETCHER_H_

#include <map>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// This class is responsible for parsing & filtering the data received from
// the ComponentInstaller.
class GameFetcher : public AppFetcher,
                    public AppProvisioningDataManager::Observer {
 public:
  explicit GameFetcher(Profile* profile);
  GameFetcher(const GameFetcher&) = delete;
  GameFetcher& operator=(const GameFetcher&) = delete;
  ~GameFetcher() override;

  // AppFetcher:
  void GetApps(ResultCallback callback) override;
  base::CallbackListSubscription RegisterForAppUpdates(
      RepeatingResultCallback callback) override;
  void GetIcon(const std::string& app_id,
               int32_t size_hint_in_dip,
               GetIconCallback callback) override;

  // AppProvisioningDataManager::Observer:
  void OnAppWithLocaleListUpdated(
      const proto::AppWithLocaleList& app_with_locale_list) override;

  void SetResultsForTesting(
      const proto::AppWithLocaleList& app_with_locale_list);
  void SetLocaleForTesting(const std::string& language,
                           const std::string& country);

 private:
  std::vector<Result> GetAppsForCurrentLocale(
      const proto::AppWithLocaleList& app_data);

  bool AvailableInCurrentLocale(
      const apps::proto::LocaleAvailability& app_with_locale);

  std::u16string GetLocalisedName(
      const apps::proto::LocaleAvailability& app_with_locale,
      Profile* profile);

  absl::optional<std::string> test_country_;

  absl::optional<std::string> test_language_;

  std::vector<Result> last_results_;

  // The key for this map is the App ID, while the value is a pointer to a
  // Result in the last_results_ vector.
  std::map<std::string, Result*> app_id_to_result_;

  raw_ptr<Profile> profile_;

  ResultCallbackList result_callback_list_;

  base::ScopedObservation<AppProvisioningDataManager,
                          AppProvisioningDataManager::Observer>
      app_provisioning_data_observeration_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_FETCHER_H_
