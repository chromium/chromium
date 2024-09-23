// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"

namespace extensions {

// Tests that background pages are marked as never composited to prevent GPU
// resource allocation. See crbug.com/362165 and crbug.com/163698.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, BackgroundPageIsNeverComposited) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ProcessManager* manager = ProcessManager::Get(browser()->profile());
  ExtensionHost* host = FindHostWithPath(manager, "/backgroundpage.html", 1);
  ASSERT_TRUE(host->host_contents()->GetDelegate()->IsNeverComposited(
      host->host_contents()));
}

}  // namespace extensions
