// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_test_util.h"

#include "ash/shell.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/shelf/app_service/exo_app_type_resolver.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kTestAppName[] = "Test ARC App";
constexpr char kTestAppName2[] = "Test ARC App 2";
constexpr char kTestAppActivity[] = "test.arc.app.package.activity";
constexpr char kTestAppActivity2[] = "test.arc.app.package.activity2";

}  // namespace

views::Widget* CreateExoWindow(const std::string& window_app_id,
                               const std::string& app_id) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  params.context = Shell::GetPrimaryRootWindow();

  exo::WMHelper::AppPropertyResolver::Params resolver_params;
  resolver_params.app_id = window_app_id;
  resolver_params.for_creation = true;
  ExoAppTypeResolver().PopulateProperties(resolver_params,
                                          params.init_properties_container);

  if (!app_id.empty()) {
    params.init_properties_container.SetProperty(app_restore::kAppIdKey,
                                                 app_id);
  }

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));

  // Make the window resizeable.
  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanResize |
          aura::client::kResizeBehaviorCanMaximize);

  exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
  widget->Show();
  widget->Activate();
  return widget;
}

views::Widget* CreateExoWindow(const std::string& window_app_id) {
  return CreateExoWindow(window_app_id, std::string());
}

std::string GetTestApp1Id(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestAppActivity);
}

std::string GetTestApp2Id(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestAppActivity2);
}

std::vector<arc::mojom::AppInfoPtr> GetTestAppsList(
    const std::string& package_name,
    bool multi_app) {
  std::vector<arc::mojom::AppInfoPtr> apps;

  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = kTestAppName;
  app->package_name = package_name;
  app->activity = kTestAppActivity;
  app->sticky = false;
  apps.push_back(std::move(app));

  if (multi_app) {
    app = arc::mojom::AppInfo::New();
    app->name = kTestAppName2;
    app->package_name = package_name;
    app->activity = kTestAppActivity2;
    app->sticky = false;
    apps.push_back(std::move(app));
  }

  return apps;
}

// static
void AppLaunchInfoSaveWaiter::Wait(bool allow_save) {
  ::full_restore::FullRestoreSaveHandler* save_handler =
      ::full_restore::FullRestoreSaveHandler::GetInstance();

  if (allow_save) {
    save_handler->AllowSave();
  }

  base::OneShotTimer* timer = save_handler->GetTimerForTesting();
  if (timer->IsRunning()) {
    // Simulate timeout, and the launch info is saved.
    timer->FireNow();
  }
  content::RunAllTasksUntilIdle();

  ::full_restore::FullRestoreReadHandler::GetInstance()
      ->profile_path_to_restore_data_.clear();
}

}  // namespace ash
