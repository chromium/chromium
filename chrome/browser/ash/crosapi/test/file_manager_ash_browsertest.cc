// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_manager_ash.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "content/public/test/browser_test.h"

namespace crosapi {
namespace {

using FileManagerCrosapiTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(FileManagerCrosapiTest, ShowItemInFolder) {
  const base::FilePath bad_path("/does/not/exist");
  base::RunLoop run_loop1;

  // A non-existent path.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->ShowItemInFolder(
      bad_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop1.QuitClosure()));
  run_loop1.Run();

  const base::FilePath folder_path =
      file_manager::util::GetMyFilesFolderForProfile(browser()->profile());
  base::RunLoop run_loop2;

  // A valid folder.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->ShowItemInFolder(
      folder_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kSucceeded);
            quit_closure.Run();
          },
          run_loop2.QuitClosure()));
  run_loop2.Run();

  const base::FilePath file_path = folder_path.Append("test_file.txt");
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::WriteFile(file_path, ""));
  }
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        EXPECT_TRUE(base::DeleteFile(file_path));
      }));
  base::RunLoop run_loop3;

  // A valid file.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->ShowItemInFolder(
      file_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kSucceeded);
            quit_closure.Run();
          },
          run_loop3.QuitClosure()));
  run_loop3.Run();
}

IN_PROC_BROWSER_TEST_F(FileManagerCrosapiTest, OpenFolder) {
  const base::FilePath bad_path("/does/not/exist");
  base::RunLoop run_loop1;

  // A non-existent path.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFolder(
      bad_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop1.QuitClosure()));
  run_loop1.Run();

  const base::FilePath folder_path =
      file_manager::util::GetMyFilesFolderForProfile(browser()->profile());
  base::RunLoop run_loop2;

  // A valid folder.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFolder(
      folder_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kSucceeded);
            quit_closure.Run();
          },
          run_loop2.QuitClosure()));
  run_loop2.Run();

  const base::FilePath file_path = folder_path.Append("test_file.txt");
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::WriteFile(file_path, ""));
  }
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        EXPECT_TRUE(base::DeleteFile(file_path));
      }));

  base::RunLoop run_loop3;

  // A valid file but not a folder.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFolder(
      file_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedInvalidType);
            quit_closure.Run();
          },
          run_loop3.QuitClosure()));
  run_loop3.Run();
}

IN_PROC_BROWSER_TEST_F(FileManagerCrosapiTest, OpenFile) {
  const base::FilePath bad_path("/does/not/exist");
  base::RunLoop run_loop1;

  // A non-existent path.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFile(
      bad_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedPathNotFound);
            quit_closure.Run();
          },
          run_loop1.QuitClosure()));
  run_loop1.Run();

  const base::FilePath folder_path =
      file_manager::util::GetMyFilesFolderForProfile(browser()->profile());
  base::RunLoop run_loop2;

  // A valid folder but not a file.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFile(
      folder_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedInvalidType);
            quit_closure.Run();
          },
          run_loop2.QuitClosure()));
  run_loop2.Run();

  const base::FilePath txtfile_path = folder_path.Append("test_file.txt");
  const base::FilePath pngfile_path = folder_path.Append("test_file.png");
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::WriteFile(txtfile_path, ""));
    EXPECT_TRUE(base::WriteFile(pngfile_path, ""));
  }
  base::ScopedClosureRunner scoped_closure_runner(
      base::BindLambdaForTesting([&]() {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;
        EXPECT_TRUE(base::DeleteFile(txtfile_path));
        EXPECT_TRUE(base::DeleteFile(pngfile_path));
      }));

  base::RunLoop run_loop3;

  // A valid file but there is no application to open txt file.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFile(
      txtfile_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedNoHandlerForFileType);
            quit_closure.Run();
          },
          run_loop3.QuitClosure()));
  run_loop3.Run();

  std::vector<apps::AppPtr> apps;
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, "fake-chrome-app");
  app->handles_intents = true;
  app->readiness = apps::Readiness::kReady;
  apps::IntentFilters filters;
  filters.push_back(apps_util::CreateFileFilter({"view"}, {}, {"txt"},
                                                "fake-chrome-app", false));
  app->intent_filters = std::move(filters);
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppRegistryCache()
      .OnApps(std::move(apps), apps::AppType::kChromeApp, false);

  base::RunLoop run_loop4;

  // A valid txt file and the app which matches intent filters exists.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFile(
      txtfile_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kSucceeded);
            quit_closure.Run();
          },
          run_loop4.QuitClosure()));
  run_loop4.Run();

  base::RunLoop run_loop5;

  // A valid file but there is no application to open png file.
  CrosapiManager::Get()->crosapi_ash()->file_manager_ash()->OpenFile(
      pngfile_path,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, mojom::OpenResult result) {
            EXPECT_EQ(result, mojom::OpenResult::kFailedNoHandlerForFileType);
            quit_closure.Run();
          },
          run_loop5.QuitClosure()));
  run_loop5.Run();
}

}  // namespace
}  // namespace crosapi
