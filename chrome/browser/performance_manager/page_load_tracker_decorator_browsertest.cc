// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;

// A class that waits for the GetLoadingState() property of a PageNode to
// transition to LoadingState::kLoadedIdle. Collects all intermediate states
// observed in-between. Generates an error if transitions are observed for
// another PageNode than |page_node|.
class PageLoadingStateObserver : public PageNode::ObserverDefaultImpl,
                                 public GraphOwnedDefaultImpl {
 public:
  PageLoadingStateObserver(base::WeakPtr<PageNode> page_node,
                           bool exit_if_already_loaded_idle)
      : page_node_(page_node) {
    DCHECK(PerformanceManagerImpl::IsAvailable());
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        // |exit_if_already_loaded_idle| is captured by copy because the lambda
        // can be executed after the constructor returns.
        base::BindLambdaForTesting([&, exit_if_already_loaded_idle](
                                       performance_manager::GraphImpl* graph) {
          EXPECT_TRUE(page_node_);

          if (exit_if_already_loaded_idle &&
              page_node_->GetLoadingState() ==
                  PageNode::LoadingState::kLoadedIdle) {
            QuitRunLoop();
          } else {
            graph_ = graph;
            graph_->AddPageNodeObserver(this);
          }
        }));
  }

  ~PageLoadingStateObserver() override = default;

  void Wait() {
    run_loop_.Run();
  }

  // Returns loading states observed before reaching LoadingState::kLoadedIdle.
  // Can only be accessed safely after Wait() has returned.
  const std::vector<PageNode::LoadingState>& observed_loading_states() const {
    // If |graph_| is not nullptr, the RunLoop wasn't quit and accessing
    // |observed_loading_states_| would be racy.
    DCHECK(!graph_);
    return observed_loading_states_;
  }

 private:
  void QuitRunLoop() {
    graph_ = nullptr;
    run_loop_.Quit();
  }

  // PageNodeObserver:
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override {
    EXPECT_EQ(page_node_.get(), page_node);

    if (page_node->GetLoadingState() == PageNode::LoadingState::kLoadedIdle) {
      graph_->RemovePageNodeObserver(this);
      QuitRunLoop();
      return;
    }

    observed_loading_states_.push_back(page_node->GetLoadingState());
  }

  // This RunLoop is quit when |page_node_->GetLoadingState()| is equal to
  // LoadingState::kLoadedIdle.
  base::RunLoop run_loop_;

  // The watched PageNode.
  const base::WeakPtr<PageNode> page_node_;

  // Observed states before reaching kLoadedIdle.
  std::vector<PageNode::LoadingState> observed_loading_states_;

  // Set when registering |this| as a PageNodeObserver. Used to unregister.
  raw_ptr<GraphImpl> graph_ = nullptr;
};

}  // namespace

using PageLoadTrackerDecoratorTest = InProcessBrowserTest;

// Integration test verifying that everything is hooked up in Chrome to update
// PageNode::GetLoadingState() is updated on navigation. See
// PageLoadTrackerDecoratorTest for low level unit tests.
IN_PROC_BROWSER_TEST_F(PageLoadTrackerDecoratorTest, PageNodeLoadingState) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Wait until GetLoadingState() is LoadingState::kLoadedIdle (the initial
  // navigation may or may not be ongoing).
  {
    PageLoadingStateObserver observer(page_node,
                                      /* exit_if_already_loaded_idle=*/true);
    observer.Wait();
  }

  // Create an Observer that will observe GetLoadingState() becoming
  // LoadingState::kLoadedIdle after the navigation below starts.
  PageLoadingStateObserver observer(page_node,
                                    /* exit_if_already_loaded_idle=*/false);

  // Navigate.
  browser()->OpenURL(
      content::OpenURLParams(
          embedded_test_server()->GetURL("/empty.html"), content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  // Wait until GetLoadingState() transitions to LoadingState::kLoadedIdle.
  observer.Wait();

  // States observed before reaching LoadingState::kLoadedIdle must follow one
  // of the two expected sequenced (state can go through |kLoadingTimedOut| or
  // not).
  EXPECT_THAT(observer.observed_loading_states(),
              AnyOf(ElementsAre(PageNode::LoadingState::kLoading,
                                PageNode::LoadingState::kLoadedBusy),
                    ElementsAre(PageNode::LoadingState::kLoading,
                                PageNode::LoadingState::kLoadingTimedOut,
                                PageNode::LoadingState::kLoading,
                                PageNode::LoadingState::kLoadedBusy)));
}

}  // namespace performance_manager
