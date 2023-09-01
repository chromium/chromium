// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_ALMANAC_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_ALMANAC_FETCHER_H_

#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

class Profile;

namespace apps {

// This class processes data received from Almanac.
class AlmanacFetcher : public AppFetcher {
 public:
  explicit AlmanacFetcher(Profile* profile);
  AlmanacFetcher(const AlmanacFetcher&) = delete;
  AlmanacFetcher& operator=(const AlmanacFetcher&) = delete;
  ~AlmanacFetcher() override;

  void GetApps(ResultCallback callback) override;

  base::CallbackListSubscription RegisterForAppUpdates(
      RepeatingResultCallback callback) override;

  void GetIcon(const std::string& app_id,
               int32_t size_hint_in_dip,
               GetIconCallback callback) override;

  // Parses all app data on update and notifies all subscribers with it.
  void OnAppsUpdate(const proto::LauncherAppResponse& launcher_app_response);

 private:
  raw_ptr<Profile> profile_;

  std::vector<Result> apps_;

  ResultCallbackList subscribers_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_ALMANAC_FETCHER_H_
