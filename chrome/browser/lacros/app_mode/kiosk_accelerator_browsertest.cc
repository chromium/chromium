// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/app_mode/test/accelerator_helpers.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

namespace {

const char kWebAppUrl[] = "https://www.example.com/";

[[nodiscard]] bool WaitUntilBrowserClosed(Browser* browser) {
  TestBrowserClosedWaiter waiter(browser);
  return waiter.WaitUntilClosed();
}

void CreateKioskWindow() {
  Browser::CreateParams params = web_app::CreateParamsForApp(
      kWebAppUrl,
      /*is_popup=*/true, /*trusted_source=*/true, /*window_bounds=*/gfx::Rect(),
      /*profile=*/ProfileManager::GetPrimaryUserProfile(),
      /*user_gesture*/ true);
  web_app::CreateWebAppWindowMaybeWithHomeTab(kWebAppUrl, params);
}

void SetBrowserInitParamsForWebKiosk() {
  BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->session_type = SessionType::kWebKioskSession;
  init_params->device_settings = crosapi::mojom::DeviceSettings::New();
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
}

}  // namespace

// Tests browser view accelerators work normally in non kiosk session.
using NonKioskAcceleratorTest = InProcessBrowserTest;

// Tests browser view accelerators do not work in kiosk session.
class WebKioskAcceleratorTest : public InProcessBrowserTest {
 protected:
  WebKioskAcceleratorTest() = default;
  ~WebKioskAcceleratorTest() override = default;

  void SetUp() override {
    SetBrowserInitParamsForWebKiosk();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetKioskSessionType();
    CreateKioskWindow();
    WaitForNonKioskWindowToClose();
    SelectFirstBrowser();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceAppMode);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  void SetKioskSessionType() {
    BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->session_type = SessionType::kWebKioskSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void WaitForNonKioskWindowToClose() {
    // The test framework automatically spawns a separate, non-kiosk browser
    // window.
    // This browser will automatically be closed by the code, but that happens
    // asynchronous so wait here until that's done.
    ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);

    for (Browser* browser : *BrowserList::GetInstance()) {
      // The non kiosk window will be automatically closed, but we have to wait
      // for the closing to finish.
      if (browser->IsAttemptingToCloseBrowser() ||
          browser->IsBrowserClosing()) {
        ASSERT_TRUE(WaitUntilBrowserClosed(browser));
        // do not continue looping since the browser list is now invalidated.
        break;
      }
    }

    ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  }
};

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, CloseTabWorks) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(PressCloseTabAccelerator(browser()));
  ASSERT_TRUE(WaitUntilBrowserClosed(browser()));
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, CloseWindowWorks) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(PressCloseWindowAccelerator(browser()));
  ASSERT_TRUE(WaitUntilBrowserClosed(browser()));
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(WebKioskAcceleratorTest, CloseTabAndWindowDontWork) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_FALSE(PressCloseTabAccelerator(browser()));
  ASSERT_FALSE(PressCloseWindowAccelerator(browser()));
  base::RunLoop loop;
  loop.RunUntilIdle();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
}
