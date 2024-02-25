// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_MOCK_ARC_APP_LIST_PREFS_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_MOCK_ARC_APP_LIST_PREFS_OBSERVER_H_

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace arc {

class MockArcAppListPrefsObserver : public ArcAppListPrefs::Observer {
 public:
  MockArcAppListPrefsObserver();
  ~MockArcAppListPrefsObserver() override;

  // ArcAppListPrefs::Observer:
  MOCK_METHOD2(OnAppRegistered,
               void(const std::string& app_id,
                    const ArcAppListPrefs::AppInfo& app_info));
  MOCK_METHOD2(OnAppStatesChanged,
               void(const std::string& id,
                    const ArcAppListPrefs::AppInfo& app_info));
  MOCK_METHOD1(OnAppRemoved, void(const std::string& id));
  MOCK_METHOD2(OnAppIconUpdated,
               void(const std::string& id,
                    const ArcAppIconDescriptor& descriptor));
  MOCK_METHOD2(OnAppNameUpdated,
               void(const std::string& id, const std::string& name));
  MOCK_METHOD1(OnAppLastLaunchTimeUpdated, void(const std::string& app_id));
  MOCK_METHOD5(OnTaskCreated,
               void(int32_t task_id,
                    const std::string& package_name,
                    const std::string& activity,
                    const std::string& intent,
                    int32_t session_id));
  MOCK_METHOD5(OnTaskDescriptionChanged,
               void(int32_t task_id,
                    const std::string& label,
                    const arc::mojom::RawIconPngData& icon,
                    uint32_t primary_color,
                    uint32_t status_bar_color));
  MOCK_METHOD1(OnTaskDestroyed, void(int32_t task_id));
  MOCK_METHOD1(OnTaskSetActive, void(int32_t task_id));
  MOCK_METHOD2(OnNotificationsEnabledChanged,
               void(const std::string& package_name, bool enabled));
  MOCK_METHOD1(OnPackageInstalled,
               void(const arc::mojom::ArcPackageInfo& package_info));
  MOCK_METHOD1(OnPackageModified,
               void(const arc::mojom::ArcPackageInfo& package_info));
  MOCK_METHOD2(OnPackageRemoved,
               void(const std::string& package_name, bool uninstalled));
  MOCK_METHOD0(OnPackageListInitialRefreshed, void());
  MOCK_METHOD1(OnInstallationStarted, void(const std::string& package_name));
  MOCK_METHOD3(OnInstallationFinished,
               void(const std::string& package_name,
                    bool success,
                    bool is_launchable_app));
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_MOCK_ARC_APP_LIST_PREFS_OBSERVER_H_
