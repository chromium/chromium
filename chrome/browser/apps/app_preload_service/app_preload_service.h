// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace apps {

class DeviceInfoManager;

class PreloadAppDefinition;

struct DeviceInfo;

class AppPreloadService : public KeyedService {
 public:
  explicit AppPreloadService(Profile* profile);
  AppPreloadService(const AppPreloadService&) = delete;
  AppPreloadService& operator=(const AppPreloadService&) = delete;
  ~AppPreloadService() override;

  static AppPreloadService* Get(Profile* profile);

  // Registers prefs used for state management of the App Preload Service.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // This function begins the process to get a list of apps from the back end
  // service, processes the list and installs the app list. This call should
  // only be used the first time a profile is created on the device as this call
  // installs a set of default and OEM apps.
  void StartAppInstallationForFirstLogin(DeviceInfo device_info);

 private:
  friend class AppPreloadServiceTest;
  FRIEND_TEST_ALL_PREFIXES(AppPreloadServiceTest, FirstLoginPrefSet);

  // Processes the list of apps retrieved by the server connector.
  void OnGetAppsForFirstLoginCompleted(std::vector<PreloadAppDefinition> apps);

  const base::Value::Dict& GetStateManager() const;

  raw_ptr<Profile> profile_;
  std::unique_ptr<AppPreloadServerConnector> server_connector_;
  std::unique_ptr<DeviceInfoManager> device_info_manager_;

  // For testing
  base::OnceClosure check_first_pref_set_callback_;

  // |weak_ptr_factory_| must be the last member of this class.
  base::WeakPtrFactory<AppPreloadService> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVICE_H_
