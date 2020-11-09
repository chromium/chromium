// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// A class that waits for the IsLoading() property of a PageNode to transition
// to a desired value. Generates an error if the first IsLoading() transitions
// is not for the observed PageNode. Ignores IsLoading() transitions after the
// first one.
class PageIsLoadingObserver : public PageNode::ObserverDefaultImpl,
                              public GraphOwnedDefaultImpl {
 public:
  PageIsLoadingObserver(base::WeakPtr<PageNode> page_node,
                        bool desired_is_loading)
      : page_node_(page_node), desired_is_loading_(desired_is_loading) {
    DCHECK(PerformanceManagerImpl::IsAvailable());
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindLambdaForTesting([&](performance_manager::GraphImpl* graph) {
          EXPECT_TRUE(page_node_);

          if (page_node_->IsLoading() == desired_is_loading_) {
            run_loop_.Quit();
          } else {
            graph_ = graph;
            graph_->AddPageNodeObserver(this);
          }
        }));
  }

  ~PageIsLoadingObserver() override = default;

  void Wait() {
    // The RunLoop is quit when |page_node_->IsLoading()| becomes equal to
    // |desired_is_loading_|.
    run_loop_.Run();
  }

 private:
  // PageNodeObserver:
  void OnIsLoadingChanged(const PageNode* page_node) override {
    EXPECT_EQ(page_node_.get(), page_node);
    EXPECT_EQ(page_node->IsLoading(), desired_is_loading_);
    graph_->RemovePageNodeObserver(this);
    run_loop_.Quit();
  }

  // This RunLoop is quit when |page_node_->IsLoading()| is equal to
  // |desired_is_loading_|.
  base::RunLoop run_loop_;

  // The watched PageNode.
  const base::WeakPtr<PageNode> page_node_;

  // Desired value for |page_node_->IsLoading()|.
  const bool desired_is_loading_;

  // Set when registering |this| as a PageNodeObserver. Used to unregister.
  GraphImpl* graph_ = nullptr;
};

}  // namespace

class PageLoadTrackerDecoratorTest : public InProcessBrowserTest {
 public:
  PageLoadTrackerDecoratorTest() = default;
  ~PageLoadTrackerDecoratorTest() override = default;
};

// Integration test verifying that everything is hooked up in Chrome to update
// PageNode::IsLoading() is updated on navigation. See
// PageLoadTrackerDecoratorTest for low level unit tests.
IN_PROC_BROWSER_TEST_F(PageLoadTrackerDecoratorTest, PageNodeIsLoading) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Wait until IsLoading() is false (the initial navigation may or may not be
  // ongoing).
  PageIsLoadingObserver observer1(page_node, false);
  observer1.Wait();

  // Create an Observer that will observe IsLoading() becoming true when the
  // navigation below starts.
  PageIsLoadingObserver observer2(page_node, true);

  // Navigate.
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/empty.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  // Wait until IsLoading() is true.
  observer2.Wait();

  // Wait until IsLoading() is false.
  PageIsLoadingObserver observer3(page_node, false);
  observer3.Wait();
}

}  // namespace performance_manager
