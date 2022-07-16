// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/chromeos/app_mode/app_session.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

const char kNavigationUrl[] = "https://www.google.com/";

class BrowserServiceLacrosBrowserTest : public InProcessBrowserTest {
 public:
  BrowserServiceLacrosBrowserTest() = default;
  BrowserServiceLacrosBrowserTest(const BrowserServiceLacrosBrowserTest&) =
      delete;
  BrowserServiceLacrosBrowserTest& operator=(
      const BrowserServiceLacrosBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    browser_service_ = std::make_unique<BrowserServiceLacros>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetSessionType(SessionType type) {
    BrowserInitParamsPtr init_params = BrowserInitParams::New();
    init_params->session_type = type;
    chromeos::LacrosService::Get()->SetInitParamsForTests(
        std::move(init_params));
  }

  void CreateFullscreenWindow() {
    bool use_callback = false;
    browser_service()->NewFullscreenWindow(
        GURL(kNavigationUrl),
        base::BindLambdaForTesting([&](CreationResult result) {
          use_callback = true;
          EXPECT_EQ(result, CreationResult::kSuccess);
        }));
    EXPECT_TRUE(use_callback);

    // Verify `AppSession` object is created when `NewFullscreenWindow` is
    // called in the Web Kiosk session. Then, disable the `AttemptUserExit`
    // method to do nothing.
    if (chromeos::LacrosService::Get()->init_params()->session_type ==
        SessionType::kWebKioskSession) {
      chromeos::AppSession* app_session =
          KioskSessionServiceLacros::Get()->GetAppSessionForTesting();
      EXPECT_TRUE(app_session);
      app_session->SetAttemptUserExitForTesting(base::DoNothing());
    }
  }

  void CreateNewWindow() {
    Browser::Create(Browser::CreateParams(browser()->profile(), false));
  }

  void VerifyFullscreenWindow() {
    // Verify the browser status.
    Browser* browser = BrowserList::GetInstance()->GetLastActive();
    EXPECT_EQ(browser->initial_show_state(), ui::SHOW_STATE_FULLSCREEN);
    EXPECT_TRUE(browser->is_trusted_source());
    EXPECT_TRUE(browser->window()->IsFullscreen());
    EXPECT_TRUE(browser->window()->IsVisible());

    // Verify the web content.
    content::WebContents* web_content =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_content->GetURL(), kNavigationUrl);
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

 private:
  std::unique_ptr<BrowserServiceLacros> browser_service_;
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest, NewFullscreenWindow) {
  CreateFullscreenWindow();
  VerifyFullscreenWindow();
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       BlockAdditionalWindowsInWebKiosk) {
  SetSessionType(SessionType::kWebKioskSession);
  CreateFullscreenWindow();

  // The new window should be blocked in the web Kiosk session.
  const size_t browser_count = BrowserList::GetInstance()->size();
  CreateNewWindow();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(BrowserList::GetInstance()->size(), browser_count);
}

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest,
                       AllowAdditionalWindowsInRegularSession) {
  SetSessionType(SessionType::kRegularSession);
  CreateFullscreenWindow();

  // The new window should be allowed in the regular session.
  const size_t browser_count = BrowserList::GetInstance()->size();
  CreateNewWindow();
  EXPECT_EQ(BrowserList::GetInstance()->size(), browser_count + 1);
}
