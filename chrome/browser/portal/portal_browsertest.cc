// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

using content::WebContents;

class PortalBrowserTest : public InProcessBrowserTest {
 public:
  PortalBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1006633): Test fails on Chrome OS.
#if defined(OS_CHROMEOS)
#define MAYBE_PortalActivation DISABLED_PortalActivation
#else
#define MAYBE_PortalActivation PortalActivation
#endif
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, MAYBE_PortalActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* contents = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1, tab_strip_model->count());

  EXPECT_TRUE(content::ExecJs(contents, "loadPromise"));
  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  EXPECT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  EXPECT_TRUE(content::ExecJs(contents, "activate()"));
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(portal_contents, tab_strip_model->GetActiveWebContents());
}

// TODO(crbug.com/1006633): Test fails on Chrome OS.
#if defined(OS_CHROMEOS)
#define MAYBE_DevToolsWindowStaysOpenAfterActivation \
  DISABLED_DevToolsWindowStaysOpenAfterActivation
#else
#define MAYBE_DevToolsWindowStaysOpenAfterActivation \
  DevToolsWindowStaysOpenAfterActivation
#endif
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       MAYBE_DevToolsWindowStaysOpenAfterActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(content::ExecJs(contents, "loadPromise"));
  DevToolsWindow* dev_tools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  WebContents* main_web_contents =
      DevToolsWindowTesting::Get(dev_tools_window)->main_web_contents();
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(contents, nullptr));

  EXPECT_TRUE(content::ExecJs(contents, "activate()"));
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(
                browser()->tab_strip_model()->GetActiveWebContents(), nullptr));
}
