// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_platform_browsertest.h"

#include "content/public/test/browser_test.h"

namespace extensions {
namespace {

IN_PROC_BROWSER_TEST_F(ExtensionPlatformBrowserTest, NavigateToURLInNewTab) {
  ASSERT_EQ(GetTabCount(), 1);
  EXPECT_TRUE(NavigateToURLInNewTab(GURL("about:blank")));
  EXPECT_EQ(GetTabCount(), 2);
}

IN_PROC_BROWSER_TEST_F(ExtensionPlatformBrowserTest, OpenAndCloseTab) {
  ASSERT_EQ(GetTabCount(), 1);
  content::WebContents* web_contents1 = GetActiveWebContents();

  // Open a new tab.
  EXPECT_TRUE(NavigateToURLInNewTab(GURL("about:blank")));
  EXPECT_EQ(GetTabCount(), 2);
  content::WebContents* web_contents2 = GetActiveWebContents();
  ASSERT_TRUE(web_contents2);

  // Close the new tab.
  CloseTabForWebContents(web_contents2);
  EXPECT_EQ(GetTabCount(), 1);

  // The first tab is active.
  EXPECT_EQ(web_contents1, GetActiveWebContents());
}

}  // namespace
}  // namespace extensions
