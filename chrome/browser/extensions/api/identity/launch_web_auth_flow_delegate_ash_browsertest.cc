// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/launch_web_auth_flow_delegate_ash.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_context.h"

namespace extensions {

class LaunchWebAuthFlowDelegateAshBrowserTest : public InProcessBrowserTest {
 public:
  LaunchWebAuthFlowDelegateAshBrowserTest() = default;

  LaunchWebAuthFlowDelegateAshBrowserTest(
      const LaunchWebAuthFlowDelegateAshBrowserTest&) = delete;
  LaunchWebAuthFlowDelegateAshBrowserTest& operator=(
      const LaunchWebAuthFlowDelegateAshBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Needed to launch Files app.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
    file_ = file_manager::test::CopyTestFilesIntoMyFiles(profile(),
                                                         {"text.docx"})[0];
  }

 protected:
  Browser* OpenFilesAppWindow() {
    ui_test_utils::BrowserChangeObserver browser_added_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    base::RunLoop run_loop;
    file_manager::util::ShowItemInFolder(
        profile(), file_.path(),
        base::BindLambdaForTesting(
            [&run_loop](platform_util::OpenOperationResult result) {
              EXPECT_EQ(platform_util::OpenOperationResult::OPEN_SUCCEEDED,
                        result);
              run_loop.Quit();
            }));
    run_loop.Run();
    browser_added_observer.Wait();

    Browser* files_browser =
        FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::FILE_MANAGER);
    EXPECT_NE(files_browser, nullptr);
    return files_browser;
  }

  Profile* profile() { return browser()->profile(); }

  storage::FileSystemURL file_;
};

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowDelegateAshBrowserTest,
                       EmptyExtensionId) {
  LaunchWebAuthFlowDelegateAsh delegate;

  base::test::TestFuture<std::optional<gfx::Rect>> future;
  delegate.GetOptionalWindowBounds(profile(), "", future.GetCallback());
  std::optional<gfx::Rect> result = future.Get();

  EXPECT_EQ(result, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowDelegateAshBrowserTest,
                       UnrecognizedExtensionId) {
  LaunchWebAuthFlowDelegateAsh delegate;

  base::test::TestFuture<std::optional<gfx::Rect>> future;
  delegate.GetOptionalWindowBounds(profile(), "abcdef", future.GetCallback());
  std::optional<gfx::Rect> result = future.Get();

  EXPECT_EQ(result, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowDelegateAshBrowserTest,
                       OdfsNoFilesApp) {
  LaunchWebAuthFlowDelegateAsh delegate;

  base::test::TestFuture<std::optional<gfx::Rect>> future;
  delegate.GetOptionalWindowBounds(profile(), extension_misc::kODFSExtensionId,
                                   future.GetCallback());
  std::optional<gfx::Rect> result = future.Get();

  EXPECT_EQ(result, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowDelegateAshBrowserTest,
                       OdfsFilesAppSmallWindow) {
  LaunchWebAuthFlowDelegateAsh delegate;

  Browser* files_app_browser = OpenFilesAppWindow();
  files_app_browser->window()->SetBounds(gfx::Rect(200, 200, 600, 600));

  base::test::TestFuture<std::optional<gfx::Rect>> future;
  delegate.GetOptionalWindowBounds(profile(), extension_misc::kODFSExtensionId,
                                   future.GetCallback());
  std::optional<gfx::Rect> result = future.Get();

  EXPECT_EQ(result, gfx::Rect(193, 170, 615, 660));
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowDelegateAshBrowserTest,
                       OdfsFilesAppLargeWindow) {
  LaunchWebAuthFlowDelegateAsh delegate;

  Browser* files_app_browser = OpenFilesAppWindow();
  files_app_browser->window()->SetBounds(gfx::Rect(200, 200, 700, 700));

  base::test::TestFuture<std::optional<gfx::Rect>> future;
  delegate.GetOptionalWindowBounds(profile(), extension_misc::kODFSExtensionId,
                                   future.GetCallback());
  std::optional<gfx::Rect> result = future.Get();

  EXPECT_EQ(result, gfx::Rect(242, 220, 615, 660));
}

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowDelegateAshBrowserTest,
                       OdfsFilesAppOffscreenWindow) {
  LaunchWebAuthFlowDelegateAsh delegate;

  Browser* files_app_browser = OpenFilesAppWindow();
  files_app_browser->window()->SetBounds(gfx::Rect(-50, -80, 600, 600));

  base::test::TestFuture<std::optional<gfx::Rect>> future;
  delegate.GetOptionalWindowBounds(profile(), extension_misc::kODFSExtensionId,
                                   future.GetCallback());
  std::optional<gfx::Rect> result = future.Get();

  EXPECT_EQ(result, gfx::Rect(0, 0, 615, 660));
}

}  // namespace extensions
