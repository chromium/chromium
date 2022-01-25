// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/app_restore/full_restore_utils.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kExampleUrl1[] = "https://examples.com";

}  // namespace

class WmDesksPrivateApiTest : public ExtensionApiTest {
 public:
  WmDesksPrivateApiTest() = default;
  ~WmDesksPrivateApiTest() override = default;

  void SetUpOnMainThread() override {
    ::full_restore::SetActiveProfilePath(browser()->profile()->GetPath());
    ExtensionApiTest::SetUpOnMainThread();
  }

  Browser* CreateBrowserWindow(const GURL& url) {
    // Create a new browser and add |url| to it.
    Browser::CreateParams params(Browser::TYPE_NORMAL, browser()->profile(),
                                 /*user_gesture=*/false);
    Browser* browser = Browser::Create(params);
    chrome::AddTabAt(browser, url, /*index=*/-1,
                     /*foreground=*/true);
    browser->window()->Show();
    return browser;
  }
};

IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, WmDesksPrivateApiTest) {
  CreateBrowserWindow(GURL(kExampleUrl1));
  ASSERT_TRUE(RunExtensionTest("wm_desks_private")) << message_;
}

}  // namespace extensions
