// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_properties_decorator.h"

#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/test/browser_test.h"

namespace performance_manager {

using TabPropertiesDecoratorBrowserTest = InProcessBrowserTest;

// Integration test verifying that when a PageNode is created for a tab, the
// corresponding tab properties is set.
IN_PROC_BROWSER_TEST_F(TabPropertiesDecoratorBrowserTest, SetIsTab) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Get PageNode associated with the current tab.
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Get data from the PageNode and verify the tab properties.
  base::RunLoop run_loop;
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    EXPECT_TRUE(TabPropertiesDecorator::Data::FromPageNode(page_node.get())
                    ->IsInTabStrip());
    run_loop.Quit();
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);
  run_loop.Run();
}

}  // namespace performance_manager
