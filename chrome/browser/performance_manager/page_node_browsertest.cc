// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/process_manager.h"

namespace performance_manager {

namespace {

using PageNodeBrowserTest = extensions::ExtensionBrowserTest;

void ExpectPageType(base::WeakPtr<PageNode> page_node, PageType expected_type) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    EXPECT_EQ(page_node->GetType(),
                                              expected_type);
                                    run_loop.Quit();
                                  }));
  run_loop.Run();
}

}  // namespace

// Integration test verifying that the correct type is set for a PageNode
// associated with a tab.
IN_PROC_BROWSER_TEST_F(PageNodeBrowserTest, TypeTab) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  ExpectPageType(page_node, PageType::kTab);
}

// Integration test verifying that the correct type is set for a PageNode
// associated with an extension background page.
IN_PROC_BROWSER_TEST_F(PageNodeBrowserTest, TypeExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/browser_action/basics"));
  ASSERT_TRUE(extension);
  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension->id());
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->host_contents());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(
          host->host_contents());
  ExpectPageType(page_node, PageType::kExtension);
}

}  // namespace performance_manager
