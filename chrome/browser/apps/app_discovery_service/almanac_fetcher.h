// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_ALMANAC_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_ALMANAC_FETCHER_H_

#include "chrome/browser/apps/almanac_api_client/proto_file_manager.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace apps {
class AlmanacIconCache;

// This class processes data received from Almanac.
class AlmanacFetcher : public AppFetcher {
 public:
  AlmanacFetcher(Profile* profile,
                 std::unique_ptr<AlmanacIconCache> icon_cache);
  AlmanacFetcher(const AlmanacFetcher&) = delete;
  AlmanacFetcher& operator=(const AlmanacFetcher&) = delete;
  ~AlmanacFetcher() override;

  void GetApps(ResultCallback callback) override;

  base::CallbackListSubscription RegisterForAppUpdates(
      RepeatingResultCallback callback) override;

  void GetIcon(const std::string& icon_id,
               int32_t size_hint_in_dip,
               GetIconCallback callback) override;

  // Registers prefs used for calling the Almanac.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Allows tests to skip the check of whether the user has an official Google
  // API key so that we can trigger an Almanac query.
  static void SetSkipApiKeyCheckForTesting(bool skip_api_key_check);

  // Methods exposed for testing the Almanac server.

  // Returns the time the server was last called.
  base::Time GetLastAppsUpdateTime() const;

  // Sets the time when the server was last called.
  void SetLastAppsUpdateTime(base::Time value);

 private:
  // Downloads apps from the server, updates the disk and the in-memory app
  // caches, and notifies the class subscribers.
  void DownloadApps();

  // Writes the response to disk if the call to the server succeeded or reads
  // the cached data otherwise.
  void OnServerResponse(std::optional<proto::LauncherAppResponse> response);

  // Updates the app caches and relevant profile preferences on a successful
  // response.
  void OnFileWritten(proto::LauncherAppResponse response, bool write_complete);

  // Parses all app data on update and notifies all subscribers with it.
  void OnAppsUpdate(std::optional<proto::LauncherAppResponse> response);

  raw_ptr<Profile> profile_;

  std::vector<Result> apps_;

  ResultCallbackList subscribers_;

  std::unique_ptr<ProtoFileManager<proto::LauncherAppResponse>>
      proto_file_manager_;
  std::unique_ptr<AlmanacIconCache> icon_cache_;

  base::WeakPtrFactory<AlmanacFetcher> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_ALMANAC_FETCHER_H_
