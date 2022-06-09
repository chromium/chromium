// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/app_registration_waiter.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

class AppInstanceWaiter : public apps::InstanceRegistry::Observer {
 public:
  AppInstanceWaiter(apps::InstanceRegistry& instance_registry,
                    const std::string& app_id)
      : apps::InstanceRegistry::Observer(&instance_registry), app_id_(app_id) {}
  ~AppInstanceWaiter() override = default;

  void AwaitRunning() {
    if (instance_registry()->ContainsAppId(app_id_))
      return;

    awaiting_running_ = true;
    run_loop_.Run();
  }

  void AwaitStopped() {
    if (!instance_registry()->ContainsAppId(app_id_))
      return;

    awaiting_stopped_ = true;
    run_loop_.Run();
  }

 private:
  void OnInstanceUpdate(const apps::InstanceUpdate&) override {
    if ((awaiting_running_ && instance_registry()->ContainsAppId(app_id_)) ||
        (awaiting_stopped_ && !instance_registry()->ContainsAppId(app_id_))) {
      run_loop_.Quit();
    }
  }

  void OnInstanceRegistryWillBeDestroyed(apps::InstanceRegistry*) override {
    NOTREACHED();
  }

  const std::string app_id_;
  base::RunLoop run_loop_;
  bool awaiting_running_ = false;
  bool awaiting_stopped_ = false;
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
            for (int i = 0; i < model->GetItemCount(); ++i) {
              items.push_back(base::UTF16ToUTF8(model->GetLabelAt(i)));
            }

            run_loop.Quit();
          }));
  run_loop.Run();
  return items;
}

void SelectContextMenuForApp(const std::string& app_id, int index) {
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

class WebAppsCrosapiBrowserTest : public InProcessBrowserTest {
 public:
  WebAppsCrosapiBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kLacrosSupport, features::kWebAppsCrosapi}, {});
  }
  ~WebAppsCrosapiBrowserTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    if (!ash_starter_.HasLacrosArgument()) {
      return;
    }
    ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
  }

  void SetUpOnMainThread() override {
    if (!ash_starter_.HasLacrosArgument()) {
      return;
    }
    auto* manager = crosapi::CrosapiManager::Get();
    test_controller_ash_ = std::make_unique<crosapi::TestControllerAsh>();
    manager->crosapi_ash()->SetTestControllerForTesting(
        test_controller_ash_.get());

    ash_starter_.StartLacros(this);
  }

  std::string InstallWebApp(const std::string& start_url,
                            apps::WindowMode mode) {
    crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
        test_controller_ash_->GetStandaloneBrowserTestController().get());
    std::string app_id;
    waiter.InstallWebApp(start_url, mode, &app_id);
    web_app::AppRegistrationWaiter(browser()->profile(), app_id).Await();
    return app_id;
  }

  Profile* profile() { return browser()->profile(); }

  apps::AppServiceProxy* AppServiceProxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  const test::AshBrowserTestStarter& ash_starter() { return ash_starter_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  test::AshBrowserTestStarter ash_starter_;
  std::unique_ptr<crosapi::TestControllerAsh> test_controller_ash_;
};

IN_PROC_BROWSER_TEST_F(WebAppsCrosapiBrowserTest, PinUsingContextMenu) {
  if (!ash_starter().HasLacrosArgument()) {
    return;
  }

  const int kNewWindowIndex = 0;
  const int kPinIndex = 1;
  const int kUnpinIndex = 1;
  const int kCloseIndex = 2;

  const web_app::AppId app_id =
      InstallWebApp("https://example.org/", apps::WindowMode::kBrowser);
  EXPECT_EQ(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
  AppServiceProxy()->Launch(app_id, /*event_flags=*/0,
                            apps::mojom::LaunchSource::kFromAppListGrid);
  AppInstanceWaiter(AppServiceProxy()->InstanceRegistry(), app_id)
      .AwaitRunning();
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
  SelectContextMenuForApp(app_id, kCloseIndex);

  // Note that Close sends an asynchronous command from Ash to Lacros, without
  // waiting for the Ash InstanceRegistry to be updated.
  AppInstanceWaiter(AppServiceProxy()->InstanceRegistry(), app_id)
      .AwaitStopped();

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

  SelectContextMenuForApp(app_id, kNewWindowIndex);
  SelectContextMenuForApp(app_id, kUnpinIndex);
  AppInstanceWaiter(AppServiceProxy()->InstanceRegistry(), app_id)
      .AwaitRunning();
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

  SelectContextMenuForApp(app_id, kCloseIndex);
  AppInstanceWaiter(AppServiceProxy()->InstanceRegistry(), app_id)
      .AwaitStopped();
  EXPECT_EQ(ash::ShelfModel::Get()->ItemIndexByAppID(app_id), -1);
}
