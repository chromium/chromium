// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/tool_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

using OpenUrlResult = base::expected<std::monostate, std::string>;

class ToolControllerBrowserTest : public InProcessBrowserTest {
 public:
  ToolControllerBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlCurrentTab) {
  // Verify ActorKeyedService is available.
  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  ASSERT_TRUE(actor_service);

  // TODO(b/544821996): Switch to using a real E2E setup (when ready), instead
  // of directly instantiating ToolController here.
  ToolController controller(browser()->GetProfile());
  base::test::TestFuture<OpenUrlResult> future;

  const GURL& url = embedded_test_server()->GetURL("/title1.html");
  controller.OpenUrl(browser(), url.spec(), /*new_tab=*/false,
                     future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value())
      << (result.has_value() ? "" : "OpenUrl failed: " + result.error());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), url);
}

}  // namespace

}  // namespace ttc
