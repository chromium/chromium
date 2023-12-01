// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/inner_text_extractor.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/compose/core/browser/config.h"
#include "content/public/test/browser_test.h"

using ComposeInnerTextBrowserTest = InProcessBrowserTest;

namespace compose {

IN_PROC_BROWSER_TEST_F(ComposeInnerTextBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test1.html")));
  base::test::TestFuture<const std::string&> inner_text_future;
  InnerTextExtractor inner_text_extractor;
  inner_text_extractor.Extract(web_contents, inner_text_future.GetCallback());
  EXPECT_EQ("AaB Cb a2D", inner_text_future.Get());

  // Check 2 extractions in parallel.
  base::test::TestFuture<const std::string&> inner_text_future2;
  base::test::TestFuture<const std::string&> inner_text_future3;
  inner_text_extractor.Extract(web_contents, inner_text_future2.GetCallback());
  inner_text_extractor.Extract(web_contents, inner_text_future3.GetCallback());
  EXPECT_EQ("AaB Cb a2D", inner_text_future2.Get());
  EXPECT_EQ("AaB Cb a2D", inner_text_future3.Get());
}

IN_PROC_BROWSER_TEST_F(ComposeInnerTextBrowserTest, MaxInnerText) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/compose/test1.html")));
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.inner_text_max_bytes = 3;

  // Should trim the inner text to the first 3 characters.
  base::test::TestFuture<const std::string&> inner_text_future;
  InnerTextExtractor inner_text_extractor;
  inner_text_extractor.Extract(web_contents, inner_text_future.GetCallback());
  EXPECT_EQ("AaB", inner_text_future.Get());
}
}  // namespace compose
