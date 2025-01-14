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

}  // namespace
}  // namespace extensions
