// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/app_launch_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {
namespace full_restore {

namespace {

const char kAppId[] = "mldnpnnoiloahfhddhobgjeophloidmo";
const int32_t kId = 100;

}  // namespace

class AppLaunchHandlerBrowserTest : public InProcessBrowserTest {
 public:
  AppLaunchHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kFullRestore);
  }
  ~AppLaunchHandlerBrowserTest() override = default;

  void WaitForAppLaunchInfoSaved() {
    ::full_restore::FullRestoreSaveHandler* save_handler =
        ::full_restore::FullRestoreSaveHandler::GetInstance();
    base::OneShotTimer* timer = save_handler->GetTimerForTesting();
    EXPECT_TRUE(timer->IsRunning());

    // Simulate timeout, and the launch info is saved.
    timer->FireNow();
    content::RunAllTasksUntilIdle();
  }

  void CreateWebApp() {
    auto web_application_info = std::make_unique<WebApplicationInfo>();
    web_application_info->start_url = GURL("https://example.org");
    web_app::AppId app_id =
        web_app::InstallWebApp(profile(), std::move(web_application_info));

    // Wait for app service to see the newly installed app.
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->FlushMojoCallsForTesting();
  }

  bool FindWebAppWindow() {
    for (auto* browser : *BrowserList::GetInstance()) {
      aura::Window* window = browser->window()->GetNativeWindow();
      if (window->GetProperty(::full_restore::kRestoreWindowIdKey) == kId)
        return true;
    }
    return false;
  }

  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, NotLaunchBrowser) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create AppLaunchHandler, and set should restore.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();

  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, RestoreAndAddApp) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler, and set should restore.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, AddAppAndRestore) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  CreateWebApp();

  // Set should restore
  app_launch_handler->SetShouldRestore();

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, NotRestore) {
  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady();

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
  EXPECT_FALSE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, RestoreAndLaunchBrowser) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  // Set should restore
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest, LaunchBrowserAndRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());

  // Set should restore
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest,
                       RestoreAndLaunchBrowserAndAddApp) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler, and set should restore.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  CreateWebApp();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerBrowserTest,
                       LaunchBrowserAndAddAppAndRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::full_restore::AppLaunchInfo>(
                                extension_misc::kChromeAppId, kId));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::full_restore::AppLaunchInfo>(
          kAppId, kId, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create AppLaunchHandler.
  auto app_launch_handler = std::make_unique<AppLaunchHandler>(profile());

  app_launch_handler->LaunchBrowserWhenReady();
  content::RunAllTasksUntilIdle();

  CreateWebApp();
  content::RunAllTasksUntilIdle();

  // Set should restore
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
  EXPECT_TRUE(FindWebAppWindow());
}

class AppLaunchHandlerNoBrowserBrowserTest
    : public AppLaunchHandlerBrowserTest {
 public:
  AppLaunchHandlerNoBrowserBrowserTest() = default;
  ~AppLaunchHandlerNoBrowserBrowserTest() override = default;

  // BrowserTestBase:
  void PreRunTestOnMainThread() override {
    set_skip_initial_restore(true);

    AppLaunchHandlerBrowserTest::PreRunTestOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AppLaunchHandlerNoBrowserBrowserTest,
                       NoBrowserOnLaunch) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

}  // namespace full_restore
}  // namespace chromeos
