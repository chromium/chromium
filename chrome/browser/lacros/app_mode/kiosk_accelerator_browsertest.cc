// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/app_mode/test/accelerator_helpers.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/test/browser_test.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using crosapi::mojom::BrowserInitParams;
using crosapi::mojom::BrowserInitParamsPtr;
using crosapi::mojom::CreationResult;
using crosapi::mojom::SessionType;

namespace {

const char kNavigationUrl[] = "https://www.example.com/";

// Observes BrowserList and blocks until a given browser is removed.
class BrowserClosedWaiter : public BrowserListObserver {
 public:
  explicit BrowserClosedWaiter(Browser* browser) : browser_{browser} {
    BrowserList::AddObserver(this);
  }

  ~BrowserClosedWaiter() override { BrowserList::RemoveObserver(this); }

  [[nodiscard]] bool Wait() { return future_.Wait(); }

 private:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser_ == browser) {
      future_.SetValue();
    }
  }

  raw_ptr<Browser> browser_ = nullptr;
  base::test::TestFuture<void> future_;
};

[[nodiscard]] bool WaitUntilBrowserClosed(Browser* browser) {
  auto waiter = BrowserClosedWaiter(browser);
  return waiter.Wait();
}

}  // namespace

// Tests browser view accelerators work normally in non kiosk session.
class NonKioskAcceleratorTest : public InProcessBrowserTest {
 protected:
  NonKioskAcceleratorTest() = default;
  ~NonKioskAcceleratorTest() override = default;

  void SetUpOnMainThread() override {
    browser_service_ = std::make_unique<BrowserServiceLacros>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    browser_service_.reset();
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

 private:
  std::unique_ptr<BrowserServiceLacros> browser_service_;
};

// Tests browser view accelerators do not work in kiosk session.
class WebKioskAcceleratorTest : public InProcessBrowserTest {
 protected:
  WebKioskAcceleratorTest() = default;
  ~WebKioskAcceleratorTest() override = default;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    browser_service_ = std::make_unique<BrowserServiceLacros>();
    InProcessBrowserTest::SetUpOnMainThread();
    SetKioskSessionType();
    CreateKioskWindow();
    CloseNonKioskWindow();
    SelectFirstBrowser();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceAppMode);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    browser_service_.reset();
  }

 private:
  void SetKioskSessionType() {
    BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->session_type = SessionType::kWebKioskSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void CreateKioskWindow() {
    base::test::TestFuture<CreationResult> future;
    browser_service()->NewFullscreenWindow(
        GURL(kNavigationUrl),
        display::Screen::GetScreen()->GetDisplayForNewWindows().id(),
        future.GetCallback());
    ASSERT_EQ(future.Take(), CreationResult::kSuccess);
  }

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

  void CloseNonKioskWindow() {
    ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
    for (Browser* browser : *BrowserList::GetInstance()) {
      const GURL& url =
          browser->tab_strip_model()->GetActiveWebContents()->GetURL();
      if (url != kNavigationUrl) {
        browser->window()->Close();
        ASSERT_TRUE(WaitUntilBrowserClosed(browser));
        break;
      }
    }
    ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  }

  std::unique_ptr<BrowserServiceLacros> browser_service_;
};

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, CloseTabWorks) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(chrome::PressCloseTabAccelerator(browser()));
  ASSERT_TRUE(WaitUntilBrowserClosed(browser()));
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, CloseWindowWorks) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(chrome::PressCloseWindowAccelerator(browser()));
  ASSERT_TRUE(WaitUntilBrowserClosed(browser()));
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(WebKioskAcceleratorTest, CloseTabAndWindowDontWork) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_FALSE(chrome::PressCloseTabAccelerator(browser()));
  ASSERT_FALSE(chrome::PressCloseWindowAccelerator(browser()));
  base::RunLoop loop;
  loop.RunUntilIdle();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
}
