// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

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

  BrowserServiceLacros* browser_service() const {
    return browser_service_.get();
  }

 private:
  std::unique_ptr<BrowserServiceLacros> browser_service_;
};

IN_PROC_BROWSER_TEST_F(BrowserServiceLacrosBrowserTest, NewFullscreenWindow) {
  // Verify the window has been created successfully.
  bool use_callback = false;
  browser_service()->NewFullscreenWindow(
      GURL(kNavigationUrl),
      base::BindLambdaForTesting([&](crosapi::mojom::CreationResult result) {
        use_callback = true;
        EXPECT_EQ(result, crosapi::mojom::CreationResult::kSuccess);
      }));
  EXPECT_TRUE(use_callback);

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
