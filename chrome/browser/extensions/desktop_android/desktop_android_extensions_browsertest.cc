// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class DesktopAndroidExtensionsBrowserTest : public AndroidBrowserTest {
 public:
  DesktopAndroidExtensionsBrowserTest() = default;
  ~DesktopAndroidExtensionsBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// The following is a simple test exercising a basic navigation and script
// injection. This doesn't exercise any extensions logic, but ensures Chrome
// successfully starts and can navigate the web.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest, SanityCheck) {
  ASSERT_EQ(TabModelList::models().size(), 1u);

  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("example.com", "/title1.html")));

  EXPECT_EQ("This page has no title.",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
}

}  // namespace extensions
