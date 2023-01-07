// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

class AppInstanceWaiter : public apps::InstanceRegistry::Observer {
 public:
  AppInstanceWaiter(apps::InstanceRegistry& instance_registry,
                    const std::string& app_id,
                    apps::InstanceState state =
                        apps::InstanceState(apps::kVisible | apps::kActive |
                                            apps::kRunning | apps::kStarted))
      : apps::InstanceRegistry::Observer(&instance_registry),
        app_id_(app_id),
        state_(state) {}
  ~AppInstanceWaiter() override = default;

  void Await() { run_loop_.Run(); }

 private:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    if (update.AppId() == app_id_ && update.State() == state_) {
      run_loop_.Quit();
    }
  }

  void OnInstanceRegistryWillBeDestroyed(apps::InstanceRegistry*) override {
    NOTREACHED();
  }

  const std::string app_id_;
  const apps::InstanceState state_;
  base::RunLoop run_loop_;
};

std::vector<std::string> GetContextMenuForApp(const std::string& app_id) {
  std::vector<std::string> items;
  base::RunLoop run_loop;
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id));
  delegate->GetContextMenu(
      /*display_id=*/0,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::SimpleMenuModel> model) {
            items.reserve(model->GetItemCount());
            for (size_t i = 0; i < model->GetItemCount(); ++i) {
              items.push_back(base::UTF16ToUTF8(model->GetLabelAt(i)));
            }

            run_loop.Quit();
          }));
  run_loop.Run();
  return items;
}

void SelectContextMenuForApp(const std::string& app_id, size_t index) {
  base::RunLoop run_loop;
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(app_id));
  delegate->GetContextMenu(
      /*display_id=*/0, base::BindLambdaForTesting(
                            [&](std::unique_ptr<ui::SimpleMenuModel> model) {
                              DCHECK(index < model->GetItemCount());
                              model->ActivatedAt(index, /*event_flags=*/0);
                              run_loop.Quit();
                            }));
  run_loop.Run();
}

}  // namespace

class WebAppsCrosapiBrowserTest
    : public crosapi::AshRequiresLacrosBrowserTestBase {
 public:
  WebAppsCrosapiBrowserTest() = default;
  ~WebAppsCrosapiBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    crosapi::AshRequiresLacrosBrowserTestBase::SetUpOnMainThread();
    if (!HasLacrosArgument()) {
      return;
    }

    web_app::AppTypeInitializationWaiter(profile(), apps::AppType::kWeb)
        .Await();
  }

  std::string InstallWebApp(const std::string& start_url,
                            apps::WindowMode mode) {
    crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
        GetStandaloneBrowserTestController());
    std::string app_id;
    waiter.InstallWebApp(start_url, mode, &app_id);
    CHECK(!app_id.empty());
    web_app::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  Profile* profile() { return browser()->profile(); }

  apps::AppServiceProxy* AppServiceProxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }
};

IN_PROC_BROWSER_TEST_F(WebAppsCrosapiBrowserTest, PinUsingContextMenu) {
  if (!HasLacrosArgument()) {
    return;
  }

  const size_t kNewWindowIndex = 0;
  const size_t kPinIndex = 1;
  const size_t kUnpinIndex = 1;
  const size_t kCloseIndex = 2;

  const web_app::AppId app_id =
      InstallWebApp("https://example.org/", apps::WindowMode::kWindow);

  EXPECT_EQ(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    AppServiceProxy()->Launch(app_id, /*event_flags=*/0,
                              apps::LaunchSource::kFromAppListGrid);
    waiter.Await();
  }

  EXPECT_NE(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
  {
    std::vector<std::string> items = GetContextMenuForApp(app_id);
    ASSERT_EQ(5u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Pin");
    EXPECT_EQ(items[2], "Close");
    EXPECT_EQ(items[3], "Uninstall");
    EXPECT_EQ(items[4], "App info");
  }

  SelectContextMenuForApp(app_id, kPinIndex);

  // Note that Close sends an asynchronous command from Ash to Lacros, without
  // waiting for the Ash InstanceRegistry to be updated.
  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id,
                             apps::kDestroyed);
    SelectContextMenuForApp(app_id, kCloseIndex);
    waiter.Await();
  }

  EXPECT_NE(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
  {
    std::vector<std::string> items = GetContextMenuForApp(app_id);
    // Close is absent as there are no open windows.
    ASSERT_EQ(4u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Unpin");
    EXPECT_EQ(items[2], "Uninstall");
    EXPECT_EQ(items[3], "App info");
  }

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    SelectContextMenuForApp(app_id, kNewWindowIndex);
    SelectContextMenuForApp(app_id, kUnpinIndex);
    waiter.Await();
  }

  EXPECT_NE(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
  {
    std::vector<std::string> items = GetContextMenuForApp(app_id);
    ASSERT_EQ(5u, items.size());
    EXPECT_EQ(items[0], "New window");
    EXPECT_EQ(items[1], "Pin");
    EXPECT_EQ(items[2], "Close");
    EXPECT_EQ(items[3], "Uninstall");
    EXPECT_EQ(items[4], "App info");
  }

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id,
                             apps::kDestroyed);
    SelectContextMenuForApp(app_id, kCloseIndex);
    waiter.Await();
  }

  EXPECT_EQ(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
}

IN_PROC_BROWSER_TEST_F(WebAppsCrosapiBrowserTest, Uninstall) {
  if (!HasLacrosArgument()) {
    return;
  }

  const size_t kPinIndex = 1;
  const size_t kUninstallIndex = 3;

  const web_app::AppId app_id =
      InstallWebApp("https://example.org/", apps::WindowMode::kWindow);

  {
    AppInstanceWaiter waiter(AppServiceProxy()->InstanceRegistry(), app_id);
    AppServiceProxy()->Launch(app_id, /*event_flags=*/0,
                              apps::LaunchSource::kFromAppListGrid);
    waiter.Await();
  }

  EXPECT_NE(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);

  SelectContextMenuForApp(app_id, kUninstallIndex);
  AppUninstallDialogView::GetActiveViewForTesting()->CancelDialog();
  EXPECT_NE(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);

  SelectContextMenuForApp(app_id, kPinIndex);

  {
    AppInstanceWaiter app_instance_waiter(AppServiceProxy()->InstanceRegistry(),
                                          app_id, apps::kDestroyed);
    SelectContextMenuForApp(app_id, kUninstallIndex);
    AppUninstallDialogView::GetActiveViewForTesting()->AcceptDialog();
    web_app::AppReadinessWaiter(profile(), app_id,
                                apps::Readiness::kUninstalledByUser)
        .Await();
    app_instance_waiter.Await();
  }

  EXPECT_EQ(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
}
