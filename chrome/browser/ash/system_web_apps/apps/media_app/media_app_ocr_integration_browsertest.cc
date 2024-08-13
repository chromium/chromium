// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_handler_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace {

// Path to a subfolder in chrome/test/data that holds test files.
constexpr base::FilePath::CharType kTestFilesFolderInTestData[] =
    FILE_PATH_LITERAL("chromeos/file_manager");

// A small square image PDF created by a camera.
constexpr char kFilePdfImg[] = "img.pdf";

// A 1-page (8.5" x 11") PDF with some text and metadata.
constexpr char kFilePdfTall[] = "tall.pdf";

class MediaAppOcrIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  MediaAppOcrIntegrationTest() {}

  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();
  }

  void LaunchAndWait(const ash::SystemAppLaunchParams& params) {
    content::TestNavigationObserver observer =
        content::TestNavigationObserver(GURL(ash::kChromeUIMediaAppURL));
    observer.StartWatchingNewWebContents();
    ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::MEDIA,
                                 params);
    observer.Wait();
  }
};

// Waits for the number of active Browsers in the test process to reach `count`.
void WaitForBrowserCount(size_t count) {
  EXPECT_LE(BrowserList::GetInstance()->size(), count) << "Too many browsers";
  while (BrowserList::GetInstance()->size() < count) {
    ui_test_utils::WaitForBrowserToOpen();
  }
}

// Gets the base::FilePath for a named file in the test folder.
base::FilePath TestFile(const std::string& ascii_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.Append(kTestFilesFolderInTestData);
  path = path.AppendASCII(ascii_name);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(path));
  return path;
}

// Waits for a file to finish loading, assuming the busy attribute on the app
// element disappears when this happens. Uses the last active web UI.
void WaitForFirstFileLoadInActiveWindow(const std::string& filename) {
  constexpr char kWaitForAppIdleScript[] = R"(
      (async function waitForFileLoad() {
        await waitForNode('.app-bar-filename[filename="$1"]',
                          ['backlight-app-bar', 'backlight-app']);
        await waitForNode('backlight-app:not([busy])');
        return 'loaded';
      })();
  )";

  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  MediaAppUiBrowserTest::PrepareAppForTest(web_ui);

  EXPECT_EQ("loaded",
            MediaAppUiBrowserTest::EvalJsInAppFrame(
                web_ui, base::ReplaceStringPlaceholders(kWaitForAppIdleScript,
                                                        {filename}, nullptr)));
}

}  // namespace

// Test that the Media App connects to the OCR service when opening PDFs.
IN_PROC_BROWSER_TEST_P(MediaAppOcrIntegrationTest, MediaAppLaunchPdfMulti) {
  // Without any instance of MediaApp open, there are no corresponding handlers.
  auto* ax_factory = ash::AXMediaAppHandlerFactory::GetInstance();
  EXPECT_EQ(ax_factory->media_app_receivers().size(), 0u);

  // Launch one PDF window and test one handler was created for the guest frame.
  ash::SystemAppLaunchParams pdf_params_window1;
  pdf_params_window1.launch_paths = {TestFile(kFilePdfImg)};
  LaunchAndWait(pdf_params_window1);
  WaitForBrowserCount(2);  // 1 extra for the browser test browser.
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(browser_list->size(), 2u);

  WaitForFirstFileLoadInActiveWindow(kFilePdfImg);
  // There should be one handler after one PDF window is opened. If it's in the
  // UniqueReceiverSet, this also means it's bound to a remote.
  EXPECT_EQ(ax_factory->media_app_receivers().size(), 1u);

  // Launch a second PDF window and check it's got a second handler.
  ash::SystemAppLaunchParams pdf_params_window2;
  pdf_params_window2.launch_paths = {TestFile(kFilePdfTall)};
  LaunchAndWait(pdf_params_window2);
  WaitForBrowserCount(3);  // 1 extra for the browser test browser.
  EXPECT_EQ(browser_list->size(), 3u);

  WaitForFirstFileLoadInActiveWindow(kFilePdfTall);
  // There should be a second handler after a second PDF window is opened.
  EXPECT_EQ(ax_factory->media_app_receivers().size(), 2u);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppOcrIntegrationTest);
