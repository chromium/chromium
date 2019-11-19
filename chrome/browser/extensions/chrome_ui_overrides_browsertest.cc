// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/url_constants.h"

using ChromeUIOverridesBrowserTest = extensions::ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeUIOverridesBrowserTest,
                       BookmarkShortcutOverrides) {
  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("bookmarks_ui")));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_THIS_TAB));

  AddTabAtIndex(1,
                GURL(chrome::kChromeUINewTabURL),
                ui::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_ALL_TABS));
}
