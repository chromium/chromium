// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/vector_icons.h"
#include "url/gurl.h"

class AppServiceContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  AppServiceContextMenuBrowserTest() = default;
  ~AppServiceContextMenuBrowserTest() override = default;

  const gfx::VectorIcon& GetExpectedLaunchNewIcon(int command_id) {
    if (command_id == ash::USE_LAUNCH_TYPE_REGULAR)
      return views::kNewTabIcon;
    else if (command_id == ash::USE_LAUNCH_TYPE_WINDOW)
      return views::kNewWindowIcon;
    else
      return views::kLaunchIcon;
  }
};

IN_PROC_BROWSER_TEST_F(AppServiceContextMenuBrowserTest,
                       LaunchNewMenuItemDynamicallyChanges) {
  Profile* profile = browser()->profile();
  auto web_app_install_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
          GURL("https://example.org"));
  webapps::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_install_info));

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ChromeAppListItem* item = model_updater->FindItem(app_id);

  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  item->GetContextMenuModel(
      ash::AppListItemContext::kNone,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
            menu_model = std::move(created_menu);
            run_loop.Quit();
          }));
  run_loop.Run();

  auto launch_new_command_index =
      menu_model->GetIndexOfCommandId(ash::LAUNCH_NEW);
  ASSERT_TRUE(launch_new_command_index);

  auto* launch_new_submodel =
      menu_model->GetSubmenuModelAt(launch_new_command_index.value());

  EXPECT_GT(launch_new_submodel->GetItemCount(), 0u);
  for (size_t launch_new_item_index = 0;
       launch_new_item_index < launch_new_submodel->GetItemCount();
       ++launch_new_item_index) {
    const auto label_from_submenu =
        launch_new_submodel->GetLabelAt(launch_new_item_index);
    launch_new_submodel->ActivatedAt(launch_new_item_index);
    web_app::WebAppProvider::GetForTest(profile)
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();
    EXPECT_TRUE(launch_new_submodel->IsItemCheckedAt(launch_new_item_index));

    // Parent `LAUNCH_NEW` item label and icon change dynamically after
    // selection.
    EXPECT_EQ(menu_model->GetLabelAt(launch_new_command_index.value()),
              label_from_submenu);
    EXPECT_EQ(menu_model->GetIconAt(launch_new_command_index.value())
                  .GetVectorIcon()
                  .vector_icon(),
              &GetExpectedLaunchNewIcon(
                  launch_new_submodel->GetCommandIdAt(launch_new_item_index)));
  }
}
