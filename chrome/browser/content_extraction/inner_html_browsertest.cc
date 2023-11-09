// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_extraction/inner_html.h"

#include <optional>
#include <string>

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using InnerHtmlBrowserTest = InProcessBrowserTest;

namespace content_extraction {

IN_PROC_BROWSER_TEST_F(InnerHtmlBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/inner_text/subframe-a.html")));
  base::test::TestFuture<std::optional<std::string>> future;
  GetInnerHtml(*web_contents->GetPrimaryMainFrame(),
               future.GetCallback<const std::optional<std::string>&>());
  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ("<body>\n<p>a\n\n\n</p></body>", *result);
}

}  // namespace content_extraction
