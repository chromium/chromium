// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_extraction/inner_text.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using InnerTextBrowserTest = InProcessBrowserTest;

namespace content_extraction {

IN_PROC_BROWSER_TEST_F(InnerTextBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/inner_text/test1.html")));
  base::test::TestFuture<std::unique_ptr<InnerTextResult>> future;
  GetInnerText(*web_contents->GetPrimaryMainFrame(), {}, future.GetCallback());
  std::unique_ptr<InnerTextResult> result = future.Take();
  ASSERT_TRUE(result);
  // Inner-text result is combined as followed:
  // test1 contains "A<a>B C<b>D"
  // <a> is subframe-a, which contains "a"
  // <b> is subframe-b, which contains "b<a>2"
  EXPECT_EQ("AaB Cb a2D", result->inner_text);
}

}  // namespace content_extraction
