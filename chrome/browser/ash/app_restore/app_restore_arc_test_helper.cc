// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_arc_test_helper.h"

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace ash {

AppRestoreArcTestHelper::AppRestoreArcTestHelper() = default;

AppRestoreArcTestHelper::~AppRestoreArcTestHelper() = default;

void AppRestoreArcTestHelper::SetUpCommandLine(
    base::CommandLine* command_line) {
  arc::SetArcAvailableCommandLineForTesting(command_line);
}

void AppRestoreArcTestHelper::SetUpInProcessBrowserTestFixture() {
  arc::ArcSessionManager::SetUiEnabledForTesting(false);
}

void AppRestoreArcTestHelper::SetUpOnMainThread(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  arc::SetArcPlayStoreEnabledForProfile(profile_, true);

  // This ensures `GetAppPrefs()->GetApp()` below never returns nullptr.
  base::RunLoop run_loop;
  GetAppPrefs()->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
  run_loop.Run();
}

void AppRestoreArcTestHelper::StartInstance() {
  app_instance_ = std::make_unique<arc::FakeAppInstance>(GetAppHost());
  arc::ArcServiceManager::Get()->arc_bridge_service()->app()->SetInstance(
      app_instance_.get());
}

void AppRestoreArcTestHelper::StopInstance() {
  if (app_instance_) {
    arc::ArcServiceManager::Get()->arc_bridge_service()->app()->CloseInstance(
        app_instance_.get());
  }
  arc::ArcSessionManager::Get()->Shutdown();
}

void AppRestoreArcTestHelper::SendPackageAdded(
    const std::string& package_name) {
  arc::mojom::ArcPackageInfo package_info;
  package_info.package_name = package_name;
  package_info.package_version = 1;
  package_info.last_backup_android_id = 1;
  package_info.last_backup_time = 1;
  package_info.sync = false;
  GetAppHost()->OnPackageAdded(arc::mojom::ArcPackageInfo::From(package_info));

  base::RunLoop().RunUntilIdle();
}

void AppRestoreArcTestHelper::InstallTestApps(const std::string& package_name,
                                              bool multi_app) {
  StartInstance();

  GetAppHost()->OnAppListRefreshed(GetTestAppsList(package_name, multi_app));

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      GetAppPrefs()->GetApp(GetTestApp1Id(package_name));
  DCHECK(app_info);
  DCHECK(app_info->ready);
  if (multi_app) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info2 =
        GetAppPrefs()->GetApp(GetTestApp2Id(package_name));
    DCHECK(app_info2);
    DCHECK(app_info2->ready);
  }

  SendPackageAdded(package_name);
}

void AppRestoreArcTestHelper::CreateTask(const std::string& app_id,
                                         int32_t task_id,
                                         int32_t session_id) {
  auto info = GetAppPrefs()->GetApp(app_id);
  GetAppHost()->OnTaskCreated(task_id, info->package_name, info->activity,
                              info->name, info->intent_uri, session_id);
}

void AppRestoreArcTestHelper::UpdateThemeColor(int32_t task_id,
                                               uint32_t primary_color,
                                               uint32_t status_bar_color) {
  auto empty_icon = arc::mojom::RawIconPngData::New();
  GetAppHost()->OnTaskDescriptionChanged(task_id, "", std::move(empty_icon),
                                         primary_color, status_bar_color);
}

ArcAppListPrefs* AppRestoreArcTestHelper::GetAppPrefs() {
  DCHECK(profile_);
  return ArcAppListPrefs::Get(profile_);
}

arc::mojom::AppHost* AppRestoreArcTestHelper::GetAppHost() {
  return GetAppPrefs();
}

}  // namespace ash
