// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_APP_LAUNCH_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/app_restore/restore_data.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {
class AppUpdate;
enum class AppTypeName;
}  // namespace apps

class Profile;

namespace ash {

// The AppLaunchHandler class launches apps from `restore_data_` as well as
// observes app updates.
class AppLaunchHandler : public apps::AppRegistryCache::Observer {
 public:
  explicit AppLaunchHandler(Profile* profile);
  AppLaunchHandler(const AppLaunchHandler&) = delete;
  AppLaunchHandler& operator=(const AppLaunchHandler&) = delete;
  ~AppLaunchHandler() override;

  // Returns true if there are some restore data. Otherwise, returns false.
  bool HasRestoreData() const;

  // Called when an app has launched. Overriders can use this to record
  // histograms based on `app_type_name`.
  virtual void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) = 0;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }

  ::app_restore::RestoreData* restore_data() { return restore_data_.get(); }

 protected:
  // Note: LaunchApps does not launch browser windows, this is handled
  // separately.
  void LaunchApps();

  // Protected for descendants.
  void ObserveCache(apps::AppRegistryCache* source);

  // Called before a system web app or chrome app is launched. Lets
  // subclasses decide if they want to move an existing window associated
  // with `app_id`, or continue with trying to launch the app. Optional
  // launch parameters may be present in `launch_list`.
  virtual bool ShouldLaunchSystemWebAppOrChromeApp(
      const std::string& app_id,
      const ::app_restore::RestoreData::LaunchList& launch_list);

  // Called before an extension type app is launched. Allows subclasses to
  // perform some setup prior to launching an extension type app.
  virtual void OnExtensionLaunching(const std::string& app_id) {}

  virtual base::WeakPtr<AppLaunchHandler> GetWeakPtrAppLaunchHandler() = 0;

  void set_restore_data(
      std::unique_ptr<::app_restore::RestoreData> restore_data) {
    restore_data_ = std::move(restore_data);
  }

 private:
  void LaunchApp(apps::AppType app_type, const std::string& app_id);

  virtual void LaunchSystemWebAppOrChromeApp(
      apps::AppType app_type,
      const std::string& app_id,
      const ::app_restore::RestoreData::LaunchList& launch_list);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<::app_restore::RestoreData> restore_data_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_APP_LAUNCH_HANDLER_H_
