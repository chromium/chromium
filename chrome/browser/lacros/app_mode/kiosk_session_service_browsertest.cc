// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"

#include "base/test/bind.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"
#include "ui/display/screen.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::KioskSessionService;
using crosapi::mojom::SessionType;

const char kNavigationUrl[] = "https://www.google.com/";

class FakeKioskSessionServiceLacros : public KioskSessionServiceLacros {
 public:
  FakeKioskSessionServiceLacros() = default;
  ~FakeKioskSessionServiceLacros() override = default;

  // KioskSessionServiceLacros:
  void AttemptUserExit() override { std::move(after_attempt_user_exit_).Run(); }

  void set_after_attempt_user_exit(base::OnceClosure closure) {
    after_attempt_user_exit_ = std::move(closure);
  }

 private:
  base::OnceClosure after_attempt_user_exit_;
};

class KioskSessionServiceBrowserTest : public InProcessBrowserTest {
 protected:
  KioskSessionServiceBrowserTest() = default;
  ~KioskSessionServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Initialize lacros browser service.
    browser_service_ = std::make_unique<BrowserServiceLacros>();

    // Set up the main thread.
    InProcessBrowserTest::SetUpOnMainThread();

    // Initialize a fake kiosk session service for testing.
    kiosk_session_service_lacros_ =
        std::make_unique<FakeKioskSessionServiceLacros>();
  }

  bool IsServiceAvailable() const {
    auto* lacros_chrome_service = chromeos::LacrosService::Get();
    return lacros_chrome_service &&
           lacros_chrome_service
               ->IsAvailable<crosapi::mojom::KioskSessionService>();
  }

  void SetSessionType(SessionType type) {
    BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->session_type = type;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void CreateKioskMainWindow() {
    bool use_callback = false;
    browser_service()->NewFullscreenWindow(
        GURL(kNavigationUrl),
        display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
        base::BindLambdaForTesting([&](CreationResult result) {
          use_callback = true;
          EXPECT_EQ(result, CreationResult::kSuccess);
        }));
    EXPECT_TRUE(use_callback);
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

  FakeKioskSessionServiceLacros* kiosk_session_service_lacros() const {
    return kiosk_session_service_lacros_.get();
  }

 private:
  std::unique_ptr<BrowserServiceLacros> browser_service_;
  std::unique_ptr<FakeKioskSessionServiceLacros> kiosk_session_service_lacros_;
};

IN_PROC_BROWSER_TEST_F(KioskSessionServiceBrowserTest, AttemptUserExit) {
  SetSessionType(SessionType::kWebKioskSession);
  CreateKioskMainWindow();

  // Verify the install URL stored in the service.
  EXPECT_EQ(kiosk_session_service_lacros()->GetInstallURL(),
            GURL(kNavigationUrl));

  // Close all browser windows, which should trigger `AttemptUserExit` API call.
  base::RunLoop run_loop;
  kiosk_session_service_lacros()->set_after_attempt_user_exit(
      run_loop.QuitClosure());
  CloseAllBrowsers();
  run_loop.Run();

  // Verify that all windows have been closed.
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}
